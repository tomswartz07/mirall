/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#ifndef MIRALL_FOLDERWATCHER_H
#define MIRALL_FOLDERWATCHER_H

#include "config.h"

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTime>
#include <QHash>
#include <QScopedPointer>
#include <QSet>

class QTimer;

namespace Mirall {

class FolderWatcherPrivate;

/*
 * Folder Watcher monitors a directory and its sub directories
 * for changes in the local file system. Changes are signalled
 * through the folderChanged() signal.
 *
 * Note that if new folders are created, this folderwatcher class
 * does not automatically adds them to the list of monitored
 * dirs. That is the responsibility of the user of this class to
 * call addPath() with the new dir.
 */

class FolderWatcher : public QObject
{
    Q_OBJECT
public:
    /**
     * @param root Path of the root of the folder
     */
    FolderWatcher(const QString &root, QObject *parent = 0L);
    virtual ~FolderWatcher();

    /**
      * Set a file name to load a file with ignore patterns.
      *
      * Valid entries do not start with a hash sign (#)
      * and may contain wildcards
      */
    void addIgnoreListFile( const QString& );

    QStringList ignores() const;

    /**
     * Not all backends are recursive by default.
     * Those need to be notified when a directory is added or removed while the watcher is disabled.
     * This is a no-op for backend that are recursive
     */
    void addPath(const QString&);
    void removePath(const QString&);

    /* Check if the path is ignored. */
    bool pathIsIgnored( const QString& path );

signals:
    /** Emitted when one of the paths is changed */
    void folderChanged(const QString &path);

    /** Emitted if an error occurs */
    void error(const QString& error);

protected slots:
    // called from the implementations to indicate a change in path
    void changeDetected( const QString& path);
    void changeDetected( const QStringList& paths);

protected:
    QHash<QString, int> _pendingPathes;

private:
    QScopedPointer<FolderWatcherPrivate> _d;
    QStringList _ignores;
    QTime _timer;
    QSet<QString> _lastPaths;

    friend class FolderWatcherPrivate;
};

}

#endif
