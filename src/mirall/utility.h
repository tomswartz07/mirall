/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
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

#ifndef UTILITY_H
#define UTILITY_H

#include "owncloudlib.h"
#include <QString>
#include <QByteArray>
#include <QDateTime>
#include <QElapsedTimer>
#include <QHash>

class QWidget;

namespace Mirall {

namespace Utility
{
    OWNCLOUDSYNC_EXPORT void sleep(int sec);
    OWNCLOUDSYNC_EXPORT void usleep(int usec);
    OWNCLOUDSYNC_EXPORT QString formatFingerprint( const QByteArray&, bool colonSeparated = true );
    OWNCLOUDSYNC_EXPORT void setupFavLink( const QString &folder );
    OWNCLOUDSYNC_EXPORT bool writeRandomFile( const QString& fname, int size = -1);
    OWNCLOUDSYNC_EXPORT QString octetsToString( qint64 octets );
    OWNCLOUDSYNC_EXPORT QString platform();
    OWNCLOUDSYNC_EXPORT QByteArray userAgentString();
    OWNCLOUDSYNC_EXPORT void raiseDialog(QWidget *);
    OWNCLOUDSYNC_EXPORT bool hasLaunchOnStartup(const QString &appName);
    OWNCLOUDSYNC_EXPORT void setLaunchOnStartup(const QString &appName, const QString& guiName, bool launch);
    OWNCLOUDSYNC_EXPORT qint64 freeDiskSpace(const QString &path, bool *ok = 0);
    OWNCLOUDSYNC_EXPORT QString toCSyncScheme(const QString &urlStr);
    OWNCLOUDSYNC_EXPORT void showInFileManager(const QString &localPath);
    /** Like QLocale::toString(double, 'f', prec), but drops trailing zeros after the decimal point */

    /**
     * @brief compactFormatDouble - formats a double value human readable.
     *
     * @param value the value to format.
     * @param prec the precision.
     * @param unit an optional unit that is appended if present.
     * @return the formatted string.
     */
    OWNCLOUDSYNC_EXPORT QString compactFormatDouble(double value, int prec, const QString& unit = QString::null);

    // porting methods
    OWNCLOUDSYNC_EXPORT QString escape(const QString&);
    OWNCLOUDSYNC_EXPORT QString dataLocation();

    // conversion function QDateTime <-> time_t   (because the ones builtin work on only unsigned 32bit)
    OWNCLOUDSYNC_EXPORT QDateTime qDateTimeFromTime_t(qint64 t);
    OWNCLOUDSYNC_EXPORT qint64 qDateTimeToTime_t(const QDateTime &t);


    // convinience OS detection methods
    OWNCLOUDSYNC_EXPORT bool isWindows();
    OWNCLOUDSYNC_EXPORT bool isMac();
    OWNCLOUDSYNC_EXPORT bool isUnix();
    OWNCLOUDSYNC_EXPORT bool isLinux(); // use with care

    // Case preserving file system underneath?
    // if this function returns true, the file system is case preserving,
    // that means "test" means the same as "TEST" for filenames.
    // if false, the two cases are two different files.
    OWNCLOUDSYNC_EXPORT bool fsCasePreserving();

    class StopWatch {
    private:
        QHash<QString, quint64> _lapTimes;
        QDateTime _startTime;
        QElapsedTimer _timer;
    public:
        void start();
        void stop();
        quint64 addLapTime( const QString& lapName );
        void reset();

        // out helpers, return the masured times.
        QDateTime startTime() const;
        QDateTime timeOfLap( const QString& lapName ) const;
        quint64 durationOfLap( const QString& lapName ) const;
    };
}

}
#endif // UTILITY_H
