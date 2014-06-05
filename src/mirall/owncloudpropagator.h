/*
 * Copyright (C) by Olivier Goffart <ogoffart@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#ifndef OWNCLOUDPROPAGATOR_H
#define OWNCLOUDPROPAGATOR_H

#include <neon/ne_request.h>
#include <QHash>
#include <QObject>
#include <qelapsedtimer.h>

#include "syncfileitem.h"

struct hbf_transfer_s;
struct ne_session_s;
struct ne_decompress_s;

namespace Mirall {

class SyncJournalDb;
class OwncloudPropagator;

class PropagatorJob : public QObject {
    Q_OBJECT
protected:
    OwncloudPropagator *_propagator;
    void emitReady() {
        bool wasReady = _readySent;
        _readySent = true;
        if (!wasReady)
            emit ready();
    };
public:
    bool _readySent;
    explicit PropagatorJob(OwncloudPropagator* propagator) : _propagator(propagator), _readySent(false) {}

public slots:
    virtual void start() = 0;
    virtual void abort() {}
signals:
    /**
     * Emitted when the job is fully finished
     */
    void finished(SyncFileItem::Status);

    /**
     * Emitted when one item has been completed within a job.
     */
    void completed(const SyncFileItem &);

    /**
     * Emitted when all the sub-jobs have been scheduled and
     * we are ready and more jobs might be started
     * This signal is not always emitted.
     */
    void ready();

    void progress(const SyncFileItem& item, quint64 bytes);

};

/*
 * Propagate a directory, and all its sub entries.
 */
class PropagateDirectory : public PropagatorJob {
    Q_OBJECT
public:
    // e.g: create the directory
    QScopedPointer<PropagatorJob>_firstJob;

    // all the sub files or sub directories.
    QVector<PropagatorJob *> _subJobs;

    SyncFileItem _item;

    int _current; // index of the current running job
    int _runningNow; // number of subJob running now
    SyncFileItem::Status _hasError;  // NoStatus,  or NormalError / SoftError if there was an error


    explicit PropagateDirectory(OwncloudPropagator *propagator, const SyncFileItem &item = SyncFileItem())
        : PropagatorJob(propagator)
        , _firstJob(0), _item(item),  _current(-1), _runningNow(0), _hasError(SyncFileItem::NoStatus) { }

    virtual ~PropagateDirectory() {
        qDeleteAll(_subJobs);
    }

    void append(PropagatorJob *subJob) {
        _subJobs.append(subJob);
    }

    virtual void start();
    virtual void abort() {
        if (_firstJob)
            _firstJob->abort();
        foreach (PropagatorJob *j, _subJobs)
            j->abort();
    }

private slots:
    void startJob(PropagatorJob *next) {
        connect(next, SIGNAL(finished(SyncFileItem::Status)), this, SLOT(slotSubJobFinished(SyncFileItem::Status)), Qt::QueuedConnection);
        connect(next, SIGNAL(completed(SyncFileItem)), this, SIGNAL(completed(SyncFileItem)));
        connect(next, SIGNAL(progress(SyncFileItem,quint64)), this, SIGNAL(progress(SyncFileItem,quint64)));
        connect(next, SIGNAL(ready()), this, SLOT(slotSubJobReady()));
        _runningNow++;
        QMetaObject::invokeMethod(next, "start", Qt::QueuedConnection);
    }

    void slotSubJobFinished(SyncFileItem::Status status);
    void slotSubJobReady();
};


/*
 * Abstract class to propagate a single item
 * (Only used for neon job)
 */
class PropagateItemJob : public PropagatorJob {
    Q_OBJECT
protected:
    void done(SyncFileItem::Status status, const QString &errorString = QString());

    bool checkForProblemsWithShared(int httpStatusCode, const QString& msg);

    /*
     * set a custom restore job message that is used if the restore job succeeded.
     * It is displayed in the activity view.
     */
    QString restoreJobMsg() const { return _restoreJobMsg; }
    void setRestoreJobMsg( const QString& msg = QString() ) { _restoreJobMsg = msg; }

    SyncFileItem  _item;
    QString       _restoreJobMsg;

protected slots:
    void slotRestoreJobCompleted(const SyncFileItem& );

private:
    QScopedPointer<PropagateItemJob> _restoreJob;

public:
    PropagateItemJob(OwncloudPropagator* propagator, const SyncFileItem &item)
        : PropagatorJob(propagator), _item(item) {}

};

// Dummy job that just mark it as completed and ignored.
class PropagateIgnoreJob : public PropagateItemJob {
    Q_OBJECT
public:
    PropagateIgnoreJob(OwncloudPropagator* propagator,const SyncFileItem& item)
        : PropagateItemJob(propagator, item) {}
    void start() {
        done(SyncFileItem::FileIgnored, _item._errorString);
    }
};


class OwncloudPropagator : public QObject {
    Q_OBJECT

    PropagateItemJob *createJob(const SyncFileItem& item);
    QScopedPointer<PropagateDirectory> _rootJob;
    bool useLegacyJobs();

public:
    /* 'const' because they are accessed by the thread */

    QThread* _neonThread;
    ne_session_s * const _session;

    const QString _localDir; // absolute path to the local directory. ends with '/'
    const QString _remoteDir; // path to the root of the remote. ends with '/'  (include remote.php/webdav)
    const QString _remoteFolder; // folder. (same as remoteDir but without remote.php/webdav)

    SyncJournalDb * const _journal;
    bool _finishedEmited; // used to ensure that finished is only emit once

public:
    OwncloudPropagator(ne_session_s *session, const QString &localDir, const QString &remoteDir, const QString &remoteFolder,
                       SyncJournalDb *progressDb, QThread *neonThread)
            : _neonThread(neonThread)
            , _session(session)
            , _localDir((localDir.endsWith(QChar('/'))) ? localDir : localDir+'/' )
            , _remoteDir((remoteDir.endsWith(QChar('/'))) ? remoteDir : remoteDir+'/' )
            , _remoteFolder((remoteFolder.endsWith(QChar('/'))) ? remoteFolder : remoteFolder+'/' )
            , _journal(progressDb)
            , _finishedEmited(false)
            , _activeJobs(0)
    { }

    void start(const SyncFileItemVector &_syncedItems);

    QAtomicInt _downloadLimit;
    QAtomicInt _uploadLimit;

    QAtomicInt _abortRequested; // boolean set by the main thread to abort.

    /* The number of currently active jobs */
    int _activeJobs;

    bool isInSharedDirectory(const QString& file);
    bool localFileNameClash(const QString& relfile);

    void abort() {
        _abortRequested.fetchAndStoreOrdered(true);
        if (_rootJob) {
            _rootJob->abort();
        }
        emitFinished();
    }

    // timeout in seconds
    static int httpTimeout();

private slots:

    /** Emit the finished signal and make sure it is only emit once */
    void emitFinished() {
        if (!_finishedEmited)
            emit finished();
        _finishedEmited = true;
    }

signals:
    void completed(const SyncFileItem &);
    void progress(const SyncFileItem&, quint64 bytes);
    void finished();
    /**
     * Called when we detect that the total number of bytes changes (because a download or upload
     * turns out to be bigger or smaller than what was initially computed in the update phase
     */
    void adjustTotalTransmissionSize( qint64 adjust );

};

}

#endif
