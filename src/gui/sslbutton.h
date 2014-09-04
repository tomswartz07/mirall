/*
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

#ifndef SSLBUTTON_H
#define SSLBUTTON_H

#include <QToolButton>
#include <QPointer>
#include <QSsl>

class QAction;
class QSslCertificate;
class QSslConfiguration;

namespace Mirall {

class Account;

class SslButton : public QToolButton
{
    Q_OBJECT
public:
    explicit SslButton(QWidget *parent = 0);
    QString protoToString(QSsl::SslProtocol proto);
    void updateAccountInfo(Account *account);

private:
    QMenu* buildCertMenu(QMenu *parent, const QSslCertificate& cert,
                         const QList<QSslCertificate>& userApproved, int pos);
};

} // namespace Mirall

#endif // SSLBUTTON_H
