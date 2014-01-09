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


#ifndef MIRALL_DIRMONITOR_H
#define MIRALL_DIRMONITOR_H

#include "config.h"

#include "mirall/folder.h"

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTime>
#include <QHash>

#include <string>

#include "FileWatcher/FileWatcher.h"

namespace Mirall {

// FIXME: Not pretty.
/// Type for a string
typedef std::string String;
/// Type for a watch id
typedef unsigned long WatchID;

/**
 * Watches a folder and sub folders for changes
 *
 * Will notify changed files relative to the root()
 * directory.
 *
 * If too many changes happen in a short time interval,
 * it will accumulate and be fired together later.
 */
class DirMonitor : public QObject
{
    Q_OBJECT

public:
    DirMonitor(QObject *parent = 0);
    /**
     * @param root Path of the root of the folder
     */
    ~DirMonitor();

    /**
     * Root path being monitored
     */
    QString root() const;

    void update();
    /**
      * Set a file name to load a file with ignore patterns.
      *
      * Valid entries do not start with a hash sign (#)
      * and may contain wildcards
      */
    void addIgnoreListFile( const QString& ); // FIXME create a class that handles ignores

    QStringList ignores() const;

    /**
     * Not all backends are recursive by default.
     * Those need to be notified when a directory is added or removed while the watcher is disabled.
     * This is a no-op for backend that are recursive
     */
    void addPath(const QString&);
    void removePath(const QString&);

public slots:
    /*
     * Called when the propagator starts to work on a file, switch DirMonitor
     * ignoring on and off by the bool parameter ignore.
     */
    void slotRegisterBusyFile( const QString& file, bool ignore = true );

signals:
    /** Emitted when one of the paths is changed */
    void folderChanged(const QStringList &pathList);
    void filesChanged( const QStringList& files);

    /** Emitted if an error occurs */
    void error(const QString& error);


private:
    class UpdateListener : public FW::FileWatchListener
    {
    public:
        UpdateListener() : FW::FileWatchListener() {}
        void handleFileAction(FW::WatchID watchid, const String& dir, const String& filename,
                              FW::Action action);

    };

    QString _root;
    // paths pending to notified
    // QStringList _pendingPaths;
    QStringList _ignores;

    QScopedPointer<FW::FileWatcher> _fileWatcher;
    QScopedPointer<UpdateListener> _listener;
};

}

#endif
