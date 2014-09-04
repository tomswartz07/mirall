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

#include <QtGui>
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
#include <QtWidgets>
#endif

#include "protocolwidget.h"
#include "mirallconfigfile.h"
#include "syncresult.h"
#include "logger.h"
#include "utility.h"
#include "theme.h"
#include "folderman.h"
#include "syncfileitem.h"
#include "folder.h"
#include "openfilemanager.h"

#include "ui_protocolwidget.h"

namespace Mirall {

ProtocolWidget::ProtocolWidget(QWidget *parent) :
    QWidget(parent),
    IgnoredIndicatorRole( Qt::UserRole +1 ),
    _ui(new Ui::ProtocolWidget)
{
    _ui->setupUi(this);

    connect(ProgressDispatcher::instance(), SIGNAL(progressInfo(QString,Progress::Info)),
            this, SLOT(slotProgressInfo(QString,Progress::Info)));

    connect(_ui->_treeWidget, SIGNAL(itemActivated(QTreeWidgetItem*,int)), SLOT(slotOpenFile(QTreeWidgetItem*,int)));

    // Adjust copyToClipboard() when making changes here!
    QStringList header;
    header << tr("Time");
    header << tr("File");
    header << tr("Folder");
    header << tr("Action");
    header << tr("Size");

    _ui->_treeWidget->setHeaderLabels( header );
    _ui->_treeWidget->setColumnWidth(1, 180);
    _ui->_treeWidget->setColumnCount(5);
    _ui->_treeWidget->setRootIsDecorated(false);
    _ui->_treeWidget->setTextElideMode(Qt::ElideMiddle);
    _ui->_treeWidget->header()->setObjectName("ActivityListHeader");
#if defined(Q_OS_MAC)
    _ui->_treeWidget->setMinimumWidth(400);
#endif

    connect(this, SIGNAL(guiLog(QString,QString)), Logger::instance(), SIGNAL(guiLog(QString,QString)));

    _clearBlacklistBtn = _ui->_dialogButtonBox->addButton(tr("Retry Sync"), QDialogButtonBox::ActionRole);
    _clearBlacklistBtn->setEnabled(false);
    connect(_clearBlacklistBtn, SIGNAL(clicked()), SLOT(slotClearBlacklist()));

    _copyBtn = _ui->_dialogButtonBox->addButton(tr("Copy"), QDialogButtonBox::ActionRole);
    _copyBtn->setToolTip( tr("Copy the activity list to the clipboard."));
    _copyBtn->setEnabled(false);
    connect(_copyBtn, SIGNAL(clicked()), SLOT(copyToClipboard()));

    MirallConfigFile cfg;
    cfg.restoreGeometryHeader(_ui->_treeWidget->header());
}

ProtocolWidget::~ProtocolWidget()
{
    MirallConfigFile cfg;
    cfg.saveGeometryHeader(_ui->_treeWidget->header() );

    delete _ui;
}

void ProtocolWidget::copyToClipboard()
{
    QString text;
    QTextStream ts(&text);

    int topLevelItems = _ui->_treeWidget->topLevelItemCount();
    for (int i = 0; i < topLevelItems; i++) {
        QTreeWidgetItem *child = _ui->_treeWidget->topLevelItem(i);
        ts << left
                // time stamp
            << qSetFieldWidth(10)
            << child->data(0,Qt::DisplayRole).toString()
                // file name
            << qSetFieldWidth(64)
            << child->data(1,Qt::DisplayRole).toString()
                // folder
            << qSetFieldWidth(15)
            << child->data(2, Qt::DisplayRole).toString()
                // action
            << qSetFieldWidth(15)
            << child->data(3, Qt::DisplayRole).toString()
                // size
            << qSetFieldWidth(10)
            << child->data(4, Qt::DisplayRole).toString()
            << qSetFieldWidth(0)
            << endl;
    }

    QApplication::clipboard()->setText(text);
    emit guiLog(tr("Copied to clipboard"), tr("The sync status has been copied to the clipboard."));
}

void ProtocolWidget::slotClearBlacklist()
{
    FolderMan *folderMan = FolderMan::instance();

    Folder::Map folders = folderMan->map();

    foreach( Folder *f, folders ) {
        int num = f->slotWipeBlacklist();
        qDebug() << num << "entries were removed from"<< f->alias() << "blacklist";
    }

    folderMan->slotScheduleAllFolders();
}

void ProtocolWidget::cleanIgnoreItems(const QString& folder)
{
    int itemCnt = _ui->_treeWidget->topLevelItemCount();

    // Limit the number of items
    while(itemCnt > 2000) {
        delete _ui->_treeWidget->takeTopLevelItem(itemCnt - 1);
        itemCnt--;
    }

    for( int cnt = itemCnt-1; cnt >=0 ; cnt-- ) {
        QTreeWidgetItem *item = _ui->_treeWidget->topLevelItem(cnt);
        bool isErrorItem = item->data(0, IgnoredIndicatorRole).toBool();
        QString itemFolder = item->data(2, Qt::DisplayRole).toString();
        if( isErrorItem && itemFolder == folder ) {
            delete item;
        }
    }
}

QString ProtocolWidget::timeString(QDateTime dt, QLocale::FormatType format) const
{
    QLocale loc = QLocale::system();
    QString timeStr;
    QDate today = QDate::currentDate();

    if( format == QLocale::NarrowFormat ) {
        if( dt.date().day() == today.day() ) {
            timeStr = loc.toString(dt.time(), QLocale::NarrowFormat);
        } else {
            timeStr = loc.toString(dt, QLocale::NarrowFormat);
        }
    } else {
        timeStr = loc.toString(dt, format);
    }
    return timeStr;
}

void ProtocolWidget::slotOpenFile( QTreeWidgetItem *item, int )
{
    QString folderName = item->text(2);
    QString fileName = item->text(1);

    Folder *folder = FolderMan::instance()->folder(folderName);
    if (folder) {
        // folder->path() always comes back with trailing path
        QString fullPath = folder->path() + fileName;
        if (QFile(fullPath).exists()) {
            showInFileManager(fullPath);
        }
    }
}

QTreeWidgetItem* ProtocolWidget::createCompletedTreewidgetItem(const QString& folder, const SyncFileItem& item)
{
    QStringList columns;
    QDateTime timestamp = QDateTime::currentDateTime();
    const QString timeStr = timeString(timestamp);
    const QString longTimeStr = timeString(timestamp, QLocale::LongFormat);
    QIcon icon;
    QString message;

    columns << timeStr;
    columns << item._file;
    columns << folder;
    if (Progress::isWarningKind(item._status)) {
        message= item._errorString;
        columns << message;
        if (item._status == SyncFileItem::NormalError || item._status == SyncFileItem::FatalError) {
            icon = Theme::instance()->syncStateIcon(SyncResult::Error);
        } else {
            icon = Theme::instance()->syncStateIcon(SyncResult::Problem);
        }

    } else {
        message = Progress::asResultString(item);
        columns << message;
        if (Progress::isSizeDependent(item._instruction)) {
            columns <<  Utility::octetsToString( item._size );
        }
    }

    QTreeWidgetItem *twitem = new QTreeWidgetItem(columns);
    if (item._status == SyncFileItem::FileIgnored) {
        // Tell that we want to remove it on the next sync.
        twitem->setData(0, IgnoredIndicatorRole, true);
    }

    twitem->setIcon(0, icon);
    twitem->setToolTip(0, longTimeStr);
    twitem->setToolTip(1, item._file);
    twitem->setToolTip(3, message );
    return twitem;
}

void ProtocolWidget::computeResyncButtonEnabled()
{
    FolderMan *folderMan = FolderMan::instance();
    Folder::Map folders = folderMan->map();

    int cnt = 0;
    foreach( Folder *f, folders ) {
        cnt += f->blackListEntryCount();
    }

    QString t = tr("Currently no files are ignored because of previous errors.");
    if(cnt > 0) {
        t = tr("%1 files are ignored because of previous errors.\n Try to sync these again.").arg(cnt);
    }

    _clearBlacklistBtn->setEnabled(cnt > 0);
    _clearBlacklistBtn->setToolTip(t);

}

void ProtocolWidget::slotProgressInfo( const QString& folder, const Progress::Info& progress )
{
    if( progress._completedFileCount == 0 ) {
        // The sync is restarting, clean the old items
        cleanIgnoreItems(folder);
        computeResyncButtonEnabled();
    } else if (progress._totalFileCount == progress._completedFileCount) {
        //Sync completed
        computeResyncButtonEnabled();
    }
    SyncFileItem last = progress._lastCompletedItem;
    if (last.isEmpty()) return;

    QTreeWidgetItem *item = createCompletedTreewidgetItem(folder, last);
    if(item) {
        _ui->_treeWidget->insertTopLevelItem(0, item);
        if (!_copyBtn->isEnabled()) {
            _copyBtn->setEnabled(true);
        }
    }
}


}
