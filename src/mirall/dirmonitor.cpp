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

#include "mirall/dirmonitor.h"
#include "mirall/folder.h"

#include <stdint.h>

#include <QFileInfo>
#include <QFlags>
#include <QDebug>
#include <QDir>
#include <QMutexLocker>
#include <QStringList>
#include <QTimer>

#include "FileWatcher/FileWatcher.h"

namespace Mirall {

void DirMonitor::UpdateListener::handleFileAction(FW::WatchID watchid, const String& dir, const String& filename,
                      FW::Action action)
{
    QString qdir = QString::fromStdString(dir);
    QString qfile = QString::fromStdString(filename);

    qDebug() << "XXXXXXXXX ";

    switch(action)
    {
    case FW::Actions::Add:
        qDebug() << "File (" << qdir + "/" + qfile << ") Added! ";
        break;
    case FW::Actions::Delete:
        qDebug() << "File (" << qdir + "/" + qfile << ") Deleted! ";
        break;
    case FW::Actions::Modified:
        qDebug() << "File (" << qdir + "/" + qfile << ") Modified! ";
        break;
    default:
        qDebug() << "Should never happen!";
    }
}

DirMonitor::DirMonitor(QObject *parent)
    : QObject(parent),
    _fileWatcher(new FW::FileWatcher),
    _listener(new UpdateListener)
{

}

DirMonitor::~DirMonitor()
{

}

QString DirMonitor::root() const
{
    return _root;
}

void DirMonitor::addIgnoreListFile( const QString& file )
{
    if( file.isEmpty() ) return;

    QFile infile( file );
    if (!infile.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    while (!infile.atEnd()) {
        QString line = QString::fromLocal8Bit( infile.readLine() ).trimmed();
        if( !(line.startsWith( QLatin1Char('#') ) || line.isEmpty()) ) {
            _ignores.append(line);
        }
    }
}

QStringList DirMonitor::ignores() const
{
    return _ignores;
}

void DirMonitor::slotRegisterBusyFile( const QString& file, bool ignore )
{

}

void DirMonitor::addPath(const QString &path )
{
    _fileWatcher->addWatch(path.toStdString(), _listener.data(), true);
    _fileWatcher->update();
}

void DirMonitor::removePath(const QString &path )
{
    _fileWatcher->removeWatch(path.toStdString());
}

void DirMonitor::update()
{
    _fileWatcher->update();
}
} // namespace Mirall
