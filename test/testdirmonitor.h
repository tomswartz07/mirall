/*
 *    This software is in the public domain, furnished "as is", without technical
 *       support, and with no warranty, express or implied, as to its usefulness for
 *          any purpose.
 *          */

#ifndef MIRALL_TESTDIRMONITOR_H
#define MIRALL_TESTDIRMONITOR_H

#include <QtTest>

#include "mirall/dirmonitor.h"
#include "mirall/utility.h"

using namespace Mirall;

class TestDirMonitor : public QObject
{
    Q_OBJECT

private:
    QString _root;
    DirMonitor *_dm;

    void touch( const QString& file ) {
        QByteArray cmd( "touch " + file.toLocal8Bit() );
        system( cmd );
    }

private slots:

    void initTestCase() {
        _root = QDir::tempPath() + "/" + "test_" + QString::number(qrand());
        qDebug() << "creating test directory tree in " << _root;
        QDir rootDir(_root);

        rootDir.mkpath(_root + "/a1/b1/c1");
        rootDir.mkpath(_root + "/a1/b1/c2");
        rootDir.mkpath(_root + "/a1/b2/c1");
        rootDir.mkpath(_root + "/a1/b3/c3");
        rootDir.mkpath(_root + "/a2/b3/c3");

        _dm = new DirMonitor();
        _dm->addPath(_root);
    }

    void testMoni() {

        touch( _root + "/foobar");
        _dm->update();
        sleep(2);
    }

    void cleanupTestCase() {
        if( _root.startsWith(QDir::tempPath() )) {
           // system( QString("rm -rf %1").arg(_root).toLocal8Bit() );
        }
    }
};

#endif
