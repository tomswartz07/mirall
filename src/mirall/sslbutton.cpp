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

#include "mirall/sslbutton.h"
#include "mirall/account.h"
#include "mirall/utility.h"

#include <QMenu>
#include <QUrl>
#include <QtNetwork>
#include <QSslConfiguration>
#include <QWidgetAction>
#include <QLabel>

namespace Mirall {

SslButton::SslButton(QWidget *parent) :
    QToolButton(parent)
{
    setPopupMode(QToolButton::InstantPopup);
    setAutoRaise(true);
}

QString SslButton::protoToString(QSsl::SslProtocol proto)
{
    switch(proto) {
        break;
    case QSsl::SslV2:
        return QLatin1String("SSL v2");
    case QSsl::SslV3:
        return QLatin1String("SSL v3");
    case QSsl::TlsV1:
        return QLatin1String("TLS");
    default:
        return QString();
    }
}

static QString addCertDetailsField(const QString &key, const QString &value)
{
    if (value.isEmpty())
        return QString();

    return QString::fromLatin1("<tr><td style=\"vertical-align: top;\"><b>%1</b></td><td style=\"vertical-align: bottom;\">%2</td></tr>").arg(key).arg(value);
}


// necessary indication only, not sufficient for primary validation!
static bool isSelfSigned(const QSslCertificate &certificate)
{
    return certificate.issuerInfo(QSslCertificate::CommonName) == certificate.subjectInfo(QSslCertificate::CommonName) &&
           certificate.issuerInfo(QSslCertificate::OrganizationalUnitName) == certificate.subjectInfo(QSslCertificate::OrganizationalUnitName);
}

QMenu* SslButton::buildCertMenu(QMenu *parent, const QSslCertificate& cert,
                                const QList<QSslCertificate>& userApproved, int pos)
{
    QString cn = QStringList(cert.subjectInfo(QSslCertificate::CommonName)).join(QChar(';'));
    QString ou = QStringList(cert.subjectInfo(QSslCertificate::OrganizationalUnitName)).join(QChar(';'));
    QString org = QStringList(cert.subjectInfo(QSslCertificate::Organization)).join(QChar(';'));
    QString country = QStringList(cert.subjectInfo(QSslCertificate::CountryName)).join(QChar(';'));
    QString state = QStringList(cert.subjectInfo(QSslCertificate::StateOrProvinceName)).join(QChar(';'));
    QString issuer = QStringList(cert.issuerInfo(QSslCertificate::CommonName)).join(QChar(';'));
    if (issuer.isEmpty())
        issuer = QStringList(cert.issuerInfo(QSslCertificate::OrganizationalUnitName)).join(QChar(';'));
    QString sha1 = Utility::formatFingerprint(cert.digest(QCryptographicHash::Sha1).toHex(), false);
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    QString md5 = Utility::formatFingerprint(cert.digest(QCryptographicHash::Md5).toHex(), false);
#else
    QByteArray sha265hash = cert.digest(QCryptographicHash::Sha256).toHex();
    QString sha256escaped =
            Utility::escape(Utility::formatFingerprint(sha265hash.left(sha265hash.length()/2), false)) +
            QLatin1String("<br/>") +
            Utility::escape(Utility::formatFingerprint(sha265hash.mid(sha265hash.length()/2), false));
#endif
    QString serial = QString::fromUtf8(cert.serialNumber());
    QString effectiveDate = cert.effectiveDate().date().toString();
    QString expiryDate = cert.expiryDate().date().toString();
    QString sna = QStringList(cert.alternateSubjectNames().values()).join(" ");

    QString details;
    QTextStream stream(&details);

    stream << QLatin1String("<html><body>");

    stream << tr("<h3>Certificate Details</h3>");

    stream << QLatin1String("<table>");
    stream << addCertDetailsField(tr("Common Name (CN):"), Utility::escape(cn));
    stream << addCertDetailsField(tr("Subject Alternative Names:"), Utility::escape(sna)
                                  .replace(" ", "<br/>"));
    stream << addCertDetailsField(tr("Organization (O):"), Utility::escape(org));
    stream << addCertDetailsField(tr("Organizational Unit (OU):"), Utility::escape(ou));
    stream << addCertDetailsField(tr("State/Province:"), Utility::escape(state));
    stream << addCertDetailsField(tr("Country:"), Utility::escape(country));
    stream << addCertDetailsField(tr("Serial:"), Utility::escape(serial));
    stream << QLatin1String("</table>");

    stream << tr("<h3>Issuer</h3>");

    stream << QLatin1String("<table>");
    stream << addCertDetailsField(tr("Issuer:"), Utility::escape(issuer));
    stream << addCertDetailsField(tr("Issued on:"), Utility::escape(effectiveDate));
    stream << addCertDetailsField(tr("Expires on:"), Utility::escape(expiryDate));
    stream << QLatin1String("</table>");

    stream << tr("<h3>Fingerprints</h3>");

    stream << QLatin1String("<table>");
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    stream << addCertDetailsField(tr("MD 5:"), Utility::escape(md5));
#else
    stream << addCertDetailsField(tr("SHA-256:"), sha256escaped);
#endif
    stream << addCertDetailsField(tr("SHA-1:"), Utility::escape(sha1));
    stream << QLatin1String("</table>");

    if (userApproved.contains(cert)) {
        stream << tr("<p><b>Note:</b> This certificate was manually approved</p>");
    }
    stream << QLatin1String("</body></html>");

    QString txt;
    if (pos > 0) {
        txt += QString(2*pos, ' ');
        if (!Utility::isWindows()) {
            // doesn't seem to work reliably on Windows
            txt += QChar(0x21AA); // nicer '->' symbol
            txt += QChar(' ');
        }
    }

    QString certId = cn.isEmpty() ? ou : cn;

    if (QSslSocket::systemCaCertificates().contains(cert)) {
        txt += certId;
    } else {
        if (isSelfSigned(cert)) {
            txt += tr("%1 (self-signed)").arg(certId);
        } else {
            txt += tr("%1").arg(certId);
        }
    }

    // create label first
    QLabel *label = new QLabel(parent);
    label->setStyleSheet(QLatin1String("QLabel { padding: 8px; background-color: #fff; }"));
    label->setText(details);
    // plug label into widget action
    QWidgetAction *action = new QWidgetAction(parent);
    action->setDefaultWidget(label);
    // plug action into menu
    QMenu *menu = new QMenu(parent);
    menu->menuAction()->setText(txt);
    menu->addAction(action);

    return menu;

}

void SslButton::updateAccountInfo(Account *account)
{
    if (!account || account->state() != Account::Connected) {
        setVisible(false);
        return;
    } else {
        setVisible(true);
    }
    if (account->url().scheme() == QLatin1String("https")) {
        setIcon(QIcon(QPixmap(":/mirall/resources/lock-https.png")));
        QSslCipher cipher = account->sslConfiguration().sessionCipher();
        setToolTip(tr("This connection is encrypted using %1 bit %2.\n").arg(cipher.usedBits()).arg(cipher.name()));
        QMenu *menu = new QMenu(this);
        QList<QSslCertificate> chain = account->sslConfiguration().peerCertificateChain();
        menu->addAction(tr("Certificate information:"))->setEnabled(false);

        QList<QSslCertificate> tmpChain;
        foreach(QSslCertificate cert, chain) {
            tmpChain << cert;
            if (QSslSocket::systemCaCertificates().contains(cert))
                break;
        }
        chain = tmpChain;

        // find trust anchor (informational only, verification is done by QSslSocket!)
        foreach(QSslCertificate rootCA, QSslSocket::systemCaCertificates()) {
            if (rootCA.issuerInfo(QSslCertificate::CommonName) == chain.last().issuerInfo(QSslCertificate::CommonName) &&
                    rootCA.issuerInfo(QSslCertificate::Organization) == chain.last().issuerInfo(QSslCertificate::Organization)) {
                chain.append(rootCA);
                break;
            }
        }

        QListIterator<QSslCertificate> it(chain);
        it.toBack();
        int i = 0;
        while (it.hasPrevious()) {
            menu->addMenu(buildCertMenu(menu, it.previous(), account->approvedCerts(), i));
            i++;
        }
        setMenu(menu);
    } else {
        setIcon(QIcon(QPixmap(":/mirall/resources/lock-http.png")));
        setToolTip(tr("This connection is NOT secure as it is not encrypted.\n"));
        setMenu(0);
    }
}

} // namespace Mirall
