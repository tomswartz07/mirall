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

#include "clientproxy.h"

#include "mirallconfigfile.h"
#include <QUrl>

namespace Mirall {

ClientProxy::ClientProxy(QObject *parent) :
    QObject(parent)
{
}

QNetworkProxy ClientProxy::proxyFromConfig(const MirallConfigFile& cfg)
{
    QNetworkProxy proxy;

    if (cfg.proxyHostName().isEmpty())
        return QNetworkProxy();

    proxy.setHostName(cfg.proxyHostName());
    proxy.setPort(cfg.proxyPort());
    if (cfg.proxyNeedsAuth()) {
        proxy.setUser(cfg.proxyUser());
        proxy.setPassword(cfg.proxyPassword());
    }
    return proxy;
}

void ClientProxy::setupQtProxyFromConfig()
{
    Mirall::MirallConfigFile cfg;
    int proxyType = QNetworkProxy::DefaultProxy;
    QNetworkProxy proxy;

    // if there is no config file, default to system proxy.
    if( cfg.exists() ) {
        proxyType = cfg.proxyType();
        proxy  = proxyFromConfig(cfg);
    }

    switch(proxyType) {
    case QNetworkProxy::NoProxy:
        QNetworkProxyFactory::setUseSystemConfiguration(false);
        QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
        break;
    case QNetworkProxy::DefaultProxy:
        QNetworkProxyFactory::setUseSystemConfiguration(true);
        break;
    case QNetworkProxy::Socks5Proxy:
        proxy.setType(QNetworkProxy::Socks5Proxy);
        QNetworkProxyFactory::setUseSystemConfiguration(false);
        QNetworkProxy::setApplicationProxy(proxy);
        break;
    case QNetworkProxy::HttpProxy:
        proxy.setType(QNetworkProxy::HttpProxy);
        QNetworkProxyFactory::setUseSystemConfiguration(false);
        QNetworkProxy::setApplicationProxy(proxy);
        break;
    default:
        break;
    }
}

const char* ClientProxy::proxyTypeToCStr(QNetworkProxy::ProxyType type)
{
    switch (type) {
    case QNetworkProxy::NoProxy:
        return "NoProxy";
    case QNetworkProxy::DefaultProxy:
        return "DefaultProxy";
    case QNetworkProxy::Socks5Proxy:
        return "Socks5Proxy";
    case QNetworkProxy::HttpProxy:
        return "HttpProxy";
    case QNetworkProxy::HttpCachingProxy:
        return "HttpCachingProxy";
    case QNetworkProxy::FtpCachingProxy:
        return "FtpCachingProxy";
    default:
        return "NoProxy";
    }
}

void ClientProxy::setCSyncProxy( const QUrl& url, CSYNC *csync_ctx )
{
    /* Store proxy */
    QList<QNetworkProxy> proxies = QNetworkProxyFactory::proxyForQuery(QNetworkProxyQuery(url));
    // We set at least one in Application
    Q_ASSERT(proxies.count() > 0);
    QNetworkProxy proxy = proxies.first();
    if (proxy.type() == QNetworkProxy::NoProxy) {
        qDebug() << "Passing NO proxy to csync for" << url.toString();
    } else {
        qDebug() << "Passing" << proxy.hostName() << "of proxy type " << proxy.type()
                 << " to csync for" << url.toString();
    }

    csync_set_module_property( csync_ctx, "proxy_type", (void*)(proxyTypeToCStr(proxy.type())));
    csync_set_module_property( csync_ctx, "proxy_host", proxy.hostName().toUtf8().data());
    int proxy_port         = proxy.port();
    csync_set_module_property( csync_ctx, "proxy_port", &proxy_port );
    csync_set_module_property( csync_ctx, "proxy_user", proxy.user().toUtf8().data());
    csync_set_module_property( csync_ctx, "proxy_pwd",  proxy.password().toUtf8().data());

}

}
