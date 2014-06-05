/*
 * libcsync -- a library to sync a directory with another
 *
 * Copyright (c) 2011      by Andreas Schneider <asn@cryptomilk.org>
 * Copyright (c) 2012      by Klaas Freitag <freitag@owncloud.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "csync_owncloud.h"
#include "csync_owncloud_private.h"

#include <inttypes.h>


/*
 * helper method to build up a user text for SSL problems, called from the
 * verify_sslcert callback.
 */
static void addSSLWarning( char *ptr, const char *warn, int len )
{
    char *concatHere = ptr;
    int remainingLen = 0;

    if( ! (warn && ptr )) return;
    remainingLen = len - strlen(ptr);
    if( remainingLen <= 0 ) return;
    concatHere = ptr + strlen(ptr);  /* put the write pointer to the end. */
    strncpy( concatHere, warn, remainingLen );
}

/*
 * Callback to verify the SSL certificate, called from libneon.
 * It analyzes the SSL problem, creates a user information text and passes
 * it to the csync callback to ask the user.
 */
#define LEN 4096
static int ssl_callback_by_neon(void *userdata, int failures,
                          const ne_ssl_certificate *certificate)
{
    char problem[LEN];
    char buf[MAX(NE_SSL_DIGESTLEN, NE_ABUFSIZ)];
    int ret = -1;
    const ne_ssl_certificate *cert = certificate;
    csync_auth_callback authcb = NULL;
    csync_owncloud_ctx_t *ctx = (csync_owncloud_ctx_t*) userdata;

    memset( problem, 0, LEN );

    while( cert ) {

      addSSLWarning( problem, "There are problems with the SSL certificate:\n", LEN );
      if( failures & NE_SSL_NOTYETVALID ) {
        addSSLWarning( problem, " * The certificate is not yet valid.\n", LEN );
      }
      if( failures & NE_SSL_EXPIRED ) {
        addSSLWarning( problem, " * The certificate has expired.\n", LEN );
      }

      if( failures & NE_SSL_UNTRUSTED ) {
        addSSLWarning( problem, " * The certificate is not trusted!\n", LEN );
      }
      if( failures & NE_SSL_IDMISMATCH ) {
        addSSLWarning( problem, " * The hostname for which the certificate was "
                       "issued does not match the hostname of the server\n", LEN );
      }
      if( failures & NE_SSL_BADCHAIN ) {
        addSSLWarning( problem, " * The certificate chain contained a certificate other than the server cert\n", LEN );
      }
      if( failures & NE_SSL_REVOKED ) {
        addSSLWarning( problem, " * The server certificate has been revoked by the issuing authority.\n", LEN );
      }

      if (ne_ssl_cert_digest(cert, buf) == 0) {
        addSSLWarning( problem, "Certificate fingerprint: ", LEN );
        addSSLWarning( problem, buf, LEN );
        addSSLWarning( problem, "\n", LEN );
      }
      cert = ne_ssl_cert_signedby( cert );
    }
    addSSLWarning( problem, "Do you want to accept the certificate chain anyway?\nAnswer yes to do so and take the risk: ", LEN );

    if( ctx->csync_ctx ) {
        authcb = csync_get_auth_callback( ctx->csync_ctx );
    }
    if( authcb ){
        /* call the csync callback */
        DEBUG_WEBDAV("Call the csync callback for SSL problems");
        memset( buf, 0, NE_ABUFSIZ );
        (*authcb) ( problem, buf, NE_ABUFSIZ-1, 1, 0, csync_get_userdata(ctx->csync_ctx) );
        if( buf[0] == 'y' || buf[0] == 'Y') {
            ret = 0;
        } else {
            DEBUG_WEBDAV("Authentication callback replied %s", buf );

        }
    }
    DEBUG_WEBDAV("## VERIFY_SSL CERT: %d", ret  );
      return ret;
}

/*
 * Authentication callback. Is set by ne_set_server_auth to be called
 * from the neon lib to authenticate a request.
 */
static int authentication_callback_by_neon( void *userdata, const char *realm, int attempt,
                    char *username, char *password)
{
    char buf[NE_ABUFSIZ];
    csync_auth_callback authcb = NULL;
    int re = attempt;
    csync_owncloud_ctx_t *ctx = (csync_owncloud_ctx_t*) userdata;

    (void) realm;

    /* DEBUG_WEBDAV( "Authentication required %s", realm ); */
    if( username && password ) {
        DEBUG_WEBDAV( "Authentication required %s", username );
        if( ctx->dav_session.user ) {
            /* allow user without password */
            if( strlen( ctx->dav_session.user ) < NE_ABUFSIZ ) {
                strcpy( username, ctx->dav_session.user );
            }
            if( ctx->dav_session.pwd && strlen( ctx->dav_session.pwd ) < NE_ABUFSIZ ) {
                strcpy( password, ctx->dav_session.pwd );
            }
        } else {
               authcb = csync_get_auth_callback( ctx->csync_ctx );
            if( authcb != NULL ){
                /* call the csync callback */
                DEBUG_WEBDAV("Call the csync callback for %s", realm );
                memset( buf, 0, NE_ABUFSIZ );
                (*authcb) ("Enter your username: ", buf, NE_ABUFSIZ-1, 1, 0, csync_get_userdata(ctx->csync_ctx) );
                if( strlen(buf) < NE_ABUFSIZ ) {
                    strcpy( username, buf );
                }
                memset( buf, 0, NE_ABUFSIZ );
                (*authcb) ("Enter your password: ", buf, NE_ABUFSIZ-1, 0, 0, csync_get_userdata(ctx->csync_ctx) );
                if( strlen(buf) < NE_ABUFSIZ) {
                    strcpy( password, buf );
                }
            } else {
                re = 1;
            }
        }
    }
    return re;
}

/*
 * Authentication callback. Is set by ne_set_proxy_auth to be called
 * from the neon lib to authenticate against a proxy. The data to authenticate
 * against comes from mirall throught vio_module_init function.
 */
static int proxy_authentication_callback_by_neon( void *userdata, const char *realm, int attempt,
                          char *username, char *password)
{
    csync_owncloud_ctx_t *ctx = (csync_owncloud_ctx_t*) userdata;
    (void) realm;
    if( ctx->dav_session.proxy_user && strlen( ctx->dav_session.proxy_user ) < NE_ABUFSIZ) {
        strcpy( username, ctx->dav_session.proxy_user );
        if( ctx->dav_session.proxy_pwd && strlen( ctx->dav_session.proxy_pwd ) < NE_ABUFSIZ) {
            strcpy( password, ctx->dav_session.proxy_pwd );
        }
    }
    /* NTLM needs several attempts */
    return (attempt < 3) ? 0 : -1;
}

/* Configure the proxy depending on the variables */
static int configureProxy( csync_owncloud_ctx_t *ctx, ne_session *session )
{
    int port = 8080;
    int re = -1;

    if( ! session ) return -1;
    if( ! ctx->dav_session.proxy_type ) return 0; /* Go by NoProxy per default */

    if( ctx->dav_session.proxy_port > 0 ) {
        port = ctx->dav_session.proxy_port;
    }

    if( c_streq(ctx->dav_session.proxy_type, "NoProxy" )) {
        DEBUG_WEBDAV("No proxy configured.");
        re = 0;
    } else if( c_streq(ctx->dav_session.proxy_type, "DefaultProxy") ||
               c_streq(ctx->dav_session.proxy_type, "HttpProxy")    ||
               c_streq(ctx->dav_session.proxy_type, "HttpCachingProxy") ||
               c_streq(ctx->dav_session.proxy_type, "Socks5Proxy")) {

        if( ctx->dav_session.proxy_host ) {
            DEBUG_WEBDAV("%s at %s:%d", ctx->dav_session.proxy_type, ctx->dav_session.proxy_host, port );
            if (c_streq(ctx->dav_session.proxy_type, "Socks5Proxy")) {
                ne_session_socks_proxy(session, NE_SOCK_SOCKSV5, ctx->dav_session.proxy_host, port,
                                       ctx->dav_session.proxy_user, ctx->dav_session.proxy_pwd);
            } else {
                ne_session_proxy(session, ctx->dav_session.proxy_host, port );
            }
            re = 2;
        } else {
            DEBUG_WEBDAV("%s requested but no proxy host defined.", ctx->dav_session.proxy_type );
	    /* we used to try ne_system_session_proxy here, but we should rather err out
	       to behave exactly like the caller. */
        }
    } else {
        DEBUG_WEBDAV( "Unsupported Proxy: %s", ctx->dav_session.proxy_type );
    }

    return re;
}

/*
 * This hook is called for with the response of a request. Here its checked
 * if a Set-Cookie header is there for the PHPSESSID. The key is stored into
 * the webdav session to be added to subsequent requests.
 */
static void post_request_hook(ne_request *req, void *userdata, const ne_status *status)
{
    const char *set_cookie_header = NULL;
    const char *sc  = NULL;
    char *key = NULL;

    csync_owncloud_ctx_t *ctx = (csync_owncloud_ctx_t*) userdata;

    if (ctx->dav_session.session_key)
        return; /* We already have a session cookie, and we should ignore other ones */

    if(!(status && req)) return;
    if( status->klass == 2 || status->code == 401 ) {
        /* successful request */
        set_cookie_header =  ne_get_response_header( req, "Set-Cookie" );
        if( set_cookie_header ) {
            DEBUG_WEBDAV(" Set-Cookie found: %s", set_cookie_header);
            /* try to find a ', ' sequence which is the separator of neon if multiple Set-Cookie
             * headers are there.
             * The following code parses a string like this:
             * Set-Cookie: 50ace6bd8a669=p537brtt048jh8srlp2tuep7em95nh9u98mj992fbqc47d1aecp1;
             */
            sc = set_cookie_header;
            while(sc) {
                const char *sc_val = sc;
                const char *sc_end = sc_val;
                int cnt = 0;
                int len = strlen(sc_val); /* The length of the rest of the header string. */

                while( cnt < len && *sc_end != ';' && *sc_end != ',') {
                    cnt++;
                    sc_end++;
                }
                if( cnt == len ) {
                    /* exit: We are at the end. */
                    sc = NULL;
                } else if( *sc_end == ';' ) {
                    /* We are at the end of the session key. */
                    int keylen = sc_end-sc_val;
                    if( key ) {
                        int oldlen = strlen(key);
                        key = c_realloc(key, oldlen + 2 + keylen+1);
                        strcpy(key + oldlen, "; ");
                        strncpy(key + oldlen + 2, sc_val, keylen);
                        key[oldlen + 2 + keylen] = '\0';
                    } else {
                        key = c_malloc(keylen+1);
                        strncpy( key, sc_val, keylen );
                        key[keylen] = '\0';
                    }

                    /* now search for a ',' to find a potential other header entry */
                    while(cnt < len && *sc_end != ',') {
                        cnt++;
                        sc_end++;
                    }
                    if( cnt < len )
                        sc = sc_end+2; /* mind the space after the comma */
                    else
                        sc = NULL;
                } else if( *sc_end == ',' ) {
                    /* A new entry is to check. */
                    if( *(sc_end + 1) == ' ') {
                        sc = sc_end+2;
                    } else {
                        /* error condition */
                        sc = NULL;
                    }
                }
            }
        }
    } else {
        DEBUG_WEBDAV("Request failed, don't take session header.");
    }
    if( key ) {
        DEBUG_WEBDAV("----> Session-key: %s", key);
        SAFE_FREE(ctx->dav_session.session_key);
        ctx->dav_session.session_key = key;
    }
}

/*
 * this hook is called just after a request has been created, before its sent.
 * Here it is used to set the proxy connection header if available.
 */
static void request_created_hook(ne_request *req, void *userdata,
                                 const char *method, const char *requri)
{
    // FIXME Can possibly be merged with pre_send_hook
    csync_owncloud_ctx_t *ctx = (csync_owncloud_ctx_t *) userdata;
    (void) method;
    (void) requri;

    if( !req ) return;

    if(ctx->dav_session.proxy_type) {
        /* required for NTLM */
        ne_add_request_header(req, "Proxy-Connection", "Keep-Alive");
    }
}

/*
 * this hook is called just before a request has been sent.
 * Here it is used to set the session cookie if available.
 */
static void pre_send_hook(ne_request *req, void *userdata,
                          ne_buffer *header)
{
    csync_owncloud_ctx_t *ctx = (csync_owncloud_ctx_t *) userdata;

    if( !req ) return;

    if(ctx->dav_session.session_key) {
        ne_buffer_concat(header, "Cookie: ", ctx->dav_session.session_key, "\r\n", NULL);
    } else {
        DEBUG_WEBDAV("csync pre_send_hook We don't have a Auth Cookie (session_key), this is wrong!");
    }
}

static int post_send_hook(ne_request *req, void *userdata,
                          const ne_status *status)
{
    const char *location;
    csync_owncloud_ctx_t *ctx = (csync_owncloud_ctx_t *) userdata;
    (void) status;

    location = ne_get_response_header(req, "Location");

    if( !location ) return NE_OK;

    if( ctx->dav_session.redir_callback ) {
        if( ctx->dav_session.redir_callback( ctx->csync_ctx, location ) ) {
            return NE_REDIRECT;
        } else {
            return NE_RETRY;
        }
    }

    return NE_REDIRECT;
}

/*
 * Connect to a DAV server
 * This function sets the flag _connected if the connection is established
 * and returns if the flag is set, so calling it frequently is save.
 */
static int dav_connect(csync_owncloud_ctx_t *ctx,  const char *base_url) {
    int useSSL = 0;
    int rc;
    char protocol[6] = {'\0'};
    char uaBuf[256];
    char *path = NULL;
    char *scheme = NULL;
    char *host = NULL;
    unsigned int port = 0;
    int proxystate = -1;

    if (ctx->_connected) {
        return 0;
    }

    rc = c_parse_uri( base_url, &scheme,
                      &ctx->dav_session.user,
                      &ctx->dav_session.pwd,
                      &host, &port, &path );
    if( rc < 0 ) {
        DEBUG_WEBDAV("Failed to parse uri %s", base_url );
        goto out;
    }

    DEBUG_WEBDAV("* scheme %s", scheme );
    DEBUG_WEBDAV("* host %s", host );
    DEBUG_WEBDAV("* port %u", port );
    DEBUG_WEBDAV("* path %s", path );

    if( strcmp( scheme, "owncloud" ) == 0 || strcmp( scheme, "http" ) == 0 ) {
        strcpy( protocol, "http");
    } else if( strcmp( scheme, "ownclouds" ) == 0 || strcmp( scheme, "https") == 0 ) {
        strcpy( protocol, "https");
        useSSL = 1;
    } else {
        DEBUG_WEBDAV("Invalid scheme %s, go out here!", scheme );
        rc = -1;
        goto out;
    }

    DEBUG_WEBDAV("* user %s", ctx->dav_session.user ? ctx->dav_session.user : "");

    if (port == 0) {
        port = ne_uri_defaultport(protocol);
    }

    ctx->dav_session.ctx = ne_session_create( protocol, host, port);

    if (ctx->dav_session.ctx == NULL) {
        DEBUG_WEBDAV("Session create with protocol %s failed", protocol );
        rc = -1;
        goto out;
    }

    if (ctx->dav_session.read_timeout != 0) {
        ne_set_read_timeout(ctx->dav_session.ctx, ctx->dav_session.read_timeout);
        DEBUG_WEBDAV("Timeout set to %u seconds", ctx->dav_session.read_timeout );
    }

    snprintf( uaBuf, sizeof(uaBuf), "Mozilla/5.0 (%s) csyncoC/%s",
              csync_owncloud_get_platform(), CSYNC_STRINGIFY( LIBCSYNC_VERSION ));
    ne_set_useragent( ctx->dav_session.ctx, uaBuf);
    ne_set_server_auth(ctx->dav_session.ctx, authentication_callback_by_neon, ctx);

    if( useSSL ) {
        if (!ne_has_support(NE_FEATURE_SSL)) {
            DEBUG_WEBDAV("Error: SSL is not enabled.");
            rc = -1;
            goto out;
        }

        ne_ssl_trust_default_ca( ctx->dav_session.ctx );
        ne_ssl_set_verify( ctx->dav_session.ctx, ssl_callback_by_neon, ctx);
    }

    /* Hook called when a request is created. It sets the proxy connection header. */
    ne_hook_create_request( ctx->dav_session.ctx, request_created_hook, ctx );
    /* Hook called after response headers are read. It gets the Session ID. */
    ne_hook_post_headers( ctx->dav_session.ctx, post_request_hook, ctx );
    /* Hook called before a request is sent. It sets the cookies. */
    ne_hook_pre_send( ctx->dav_session.ctx, pre_send_hook, ctx );
    /* Hook called after request is dispatched. Used for handling possible redirections. */
    ne_hook_post_send( ctx->dav_session.ctx, post_send_hook, ctx );

    /* Proxy support */
    proxystate = configureProxy( ctx, ctx->dav_session.ctx );
    if( proxystate < 0 ) {
        DEBUG_WEBDAV("Error: Proxy-Configuration failed.");
    } else if( proxystate > 0 ) {
        ne_set_proxy_auth( ctx->dav_session.ctx, proxy_authentication_callback_by_neon, 0 );
    }

    ctx->_connected = 1;
    rc = 0;
out:
    SAFE_FREE(path);
    SAFE_FREE(host);
    SAFE_FREE(scheme);
    return rc;
}

/*
 * result parsing list.
 * This function is called to parse the result of the propfind request
 * to list directories on the WebDAV server. I takes a single resource
 * and fills a resource struct and stores it to the result list which
 * is stored in the listdir_context.
 */
static void propfind_results_callback(void *userdata,
                    const ne_uri *uri,
                    const ne_prop_result_set *set)
{
    struct listdir_context *fetchCtx = userdata;
    struct resource *newres = 0;
    const char *clength, *modtime = NULL;
    const char *resourcetype = NULL;
    const char *md5sum = NULL;
    const char *file_id = NULL;
    const char *directDownloadUrl = NULL;
    const char *directDownloadCookies = NULL;
    const ne_status *status = NULL;
    char *path = ne_path_unescape( uri->path );

    (void) status;
    if( ! fetchCtx ) {
        DEBUG_WEBDAV("No valid fetchContext");
        return;
    }

    if( ! fetchCtx->target ) {
        DEBUG_WEBDAV("error: target must not be zero!" );
        return;
    }

    /* Fill the resource structure with the data about the file */
    newres = c_malloc(sizeof(struct resource));
    ZERO_STRUCTP(newres);
    newres->uri =  path; /* no need to strdup because ne_path_unescape already allocates */
    newres->name = c_basename( path );

    modtime      = ne_propset_value( set, &ls_props[0] );
    clength      = ne_propset_value( set, &ls_props[1] );
    resourcetype = ne_propset_value( set, &ls_props[2] );
    md5sum       = ne_propset_value( set, &ls_props[3] );
    file_id      = ne_propset_value( set, &ls_props[4] );
    directDownloadUrl = ne_propset_value( set, &ls_props[5] );
    directDownloadCookies = ne_propset_value( set, &ls_props[6] );

    newres->type = resr_normal;
    if( clength == NULL && resourcetype && strncmp( resourcetype, "<DAV:collection>", 16 ) == 0) {
        newres->type = resr_collection;
    }

    if (modtime) {
        newres->modtime = oc_httpdate_parse(modtime);
    }

    /* DEBUG_WEBDAV("Parsing Modtime: %s -> %llu", modtime, (unsigned long long) newres->modtime ); */
    newres->size = 0;
    if (clength) {
        newres->size = atoll(clength);
        /* DEBUG_WEBDAV("Parsed File size for %s from %s: %lld", newres->name, clength, (long long)newres->size ); */
    }

    if( md5sum ) {
        newres->md5 = csync_normalize_etag(md5sum);
    }

    csync_vio_set_file_id(newres->file_id, file_id);

    if (directDownloadUrl) {
        newres->directDownloadUrl = c_strdup(directDownloadUrl);
    }
    if (directDownloadCookies) {
        newres->directDownloadCookies = c_strdup(directDownloadCookies);
    }

    /* prepend the new resource to the result list */
    newres->next   = fetchCtx->list;
    fetchCtx->list = newres;
    fetchCtx->result_count = fetchCtx->result_count + 1;
    /* DEBUG_WEBDAV( "results for URI %s: %d %d", newres->name, (int)newres->size, (int)newres->type ); */
}



/*
 * fetches a resource list from the WebDAV server. This is equivalent to list dir.
 */
static struct listdir_context *fetch_resource_list(csync_owncloud_ctx_t *ctx, const char *uri, int depth)
{
    struct listdir_context *fetchCtx;
    int ret = 0;
    ne_propfind_handler *hdl = NULL;
    ne_request *request = NULL;
    const char *content_type = NULL;
    char *curi = NULL;
    const ne_status *req_status = NULL;

    curi = _cleanPath( uri );

    /* The old legacy one-level PROPFIND cache. Also gets filled
       by the recursive cache if 'infinity' did not suceed. */
    if (ctx->propfind_cache) {
        if (c_streq(curi, ctx->propfind_cache->target)) {
            DEBUG_WEBDAV("fetch_resource_list Using simple PROPFIND cache %s", curi);
            ctx->propfind_cache->ref++;
            SAFE_FREE(curi);
            return ctx->propfind_cache;
        }
    }

    fetchCtx = c_malloc( sizeof( struct listdir_context ));
    if (!fetchCtx) {
        errno = ENOMEM;
        SAFE_FREE(curi);
        return NULL;
    }
    fetchCtx->list = NULL;
    fetchCtx->target = curi;
    fetchCtx->currResource = NULL;
    fetchCtx->ref = 1;

    /* do a propfind request and parse the results in the results function, set as callback */
    hdl = ne_propfind_create(ctx->dav_session.ctx, curi, depth);

    if(hdl) {
        ret = ne_propfind_named(hdl, ls_props, propfind_results_callback, fetchCtx);
        request = ne_propfind_get_request( hdl );
        req_status = ne_get_status( request );
    }

    if( ret == NE_OK ) {
        fetchCtx->currResource = fetchCtx->list;
        /* Check the request status. */
        if( req_status && req_status->klass != 2 ) {
            set_errno_from_http_errcode(req_status->code);
            DEBUG_WEBDAV("ERROR: Request failed: status %d (%s)", req_status->code,
                         req_status->reason_phrase);
            ret = NE_CONNECT;
            set_error_message(ctx, req_status->reason_phrase);
        }
        DEBUG_WEBDAV("Simple propfind result code %d.", req_status->code);
    } else {
        if( ret == NE_ERROR && req_status->code == 404) {
            errno = ENOENT;
        } else {
            set_errno_from_neon_errcode(ctx, ret);
        }
    }

    if( ret == NE_OK ) {
        /* Check the content type. If the server has a problem, ie. database is gone or such,
         * the content type is not xml but a html error message. Stop on processing if it's
         * not XML.
         * FIXME: Generate user error message from the reply content.
         */
        content_type =  ne_get_response_header( request, "Content-Type" );
        if( !(content_type && c_streq(content_type, "application/xml; charset=utf-8") ) ) {
            DEBUG_WEBDAV("ERROR: Content type of propfind request not XML: %s.",
                         content_type ?  content_type: "<empty>");
            errno = ERRNO_WRONG_CONTENT;
            set_error_message(ctx, "Server error: PROPFIND reply is not XML formatted!");
            ret = NE_CONNECT;
        }
    }

    if( ret != NE_OK ) {
        const char *err = NULL;
        set_errno_from_neon_errcode(ctx, ret);

        err = ne_get_error( ctx->dav_session.ctx );
        if(err) {
            set_error_message(ctx, err);
        }
        DEBUG_WEBDAV("WRN: propfind named failed with %d, request error: %s", ret, err ? err : "<nil>");
    }

    if( hdl )
        ne_propfind_destroy(hdl);

    if( ret != NE_OK ) {
        free_fetchCtx(fetchCtx);
        return NULL;
    }

    free_fetchCtx(ctx->propfind_cache);
    ctx->propfind_cache = fetchCtx;
    ctx->propfind_cache->ref++;
    return fetchCtx;
}

static struct listdir_context *fetch_resource_list_attempts(csync_owncloud_ctx_t *ctx, const char *uri, int depth)
{
    int i;

    struct listdir_context *fetchCtx = NULL;
    for(i = 0; i < 10; ++i) {
        fetchCtx = fetch_resource_list(ctx, uri, depth);
        if(fetchCtx) break;
        /* only loop in case the content is not XML formatted. Otherwise for every
         * non successful stat (for non existing directories) its tried 10 times. */
        if( errno != ERRNO_WRONG_CONTENT ) break;

        DEBUG_WEBDAV("=> Errno after fetch resource list for %s: %d", uri, errno);
        DEBUG_WEBDAV("   New attempt %i", i);
    }
    return fetchCtx;
}


/*
 * directory functions
 */
csync_vio_handle_t *owncloud_opendir(CSYNC *ctx, const char *uri) {
    struct listdir_context *fetchCtx = NULL;
    char *curi = NULL;

    DEBUG_WEBDAV("opendir method called on %s", uri );

    if (dav_connect( ctx->owncloud_context, uri ) < 0) {
        DEBUG_WEBDAV("connection failed");
        return NULL;
    }

    curi = _cleanPath( uri );
    if (ctx->owncloud_context->is_first_propfind && !ctx->owncloud_context->dav_session.no_recursive_propfind) {
        ctx->owncloud_context->is_first_propfind = false;
        // Try to fill it
        fill_recursive_propfind_cache(ctx->owncloud_context, uri, curi);
    }
    if (ctx->owncloud_context->propfind_recursive_cache) {
        // Try to fetch from recursive cache (if we have one)
        fetchCtx = get_listdir_context_from_recursive_cache(ctx->owncloud_context, curi);
    }
    SAFE_FREE(curi);
    ctx->owncloud_context->is_first_propfind = false;
    if (fetchCtx) {
        return fetchCtx;
    }

    /* fetchCtx = fetch_resource_list( uri, NE_DEPTH_ONE ); */
    fetchCtx = fetch_resource_list_attempts( ctx->owncloud_context, uri, NE_DEPTH_ONE);
    if( !fetchCtx ) {
        /* errno is set properly in fetch_resource_list */
        DEBUG_WEBDAV("Errno set to %d", errno);
        return NULL;
    } else {
        fetchCtx->currResource = fetchCtx->list;
        DEBUG_WEBDAV("opendir returning handle %p (count=%d)", (void*) fetchCtx, fetchCtx->result_count );
        return fetchCtx;
    }
    /* no freeing of curi because its part of the fetchCtx and gets freed later */
}

int owncloud_closedir(CSYNC *ctx, csync_vio_handle_t *dhandle) {
    struct listdir_context *fetchCtx = dhandle;
    free_fetchCtx(fetchCtx);
    (void)ctx;
    return 0;
}

csync_vio_file_stat_t *owncloud_readdir(CSYNC *ctx, csync_vio_handle_t *dhandle) {
    struct listdir_context *fetchCtx = dhandle;
    (void)ctx;

//    DEBUG_WEBDAV("owncloud_readdir" );
//    DEBUG_WEBDAV("owncloud_readdir %s ", fetchCtx->target);
//    DEBUG_WEBDAV("owncloud_readdir %d", fetchCtx->result_count );
//    DEBUG_WEBDAV("owncloud_readdir %p %p", fetchCtx->currResource, fetchCtx->list );

    if( fetchCtx == NULL) {
        /* DEBUG_WEBDAV("An empty dir or at end"); */
        return NULL;
    }

    while( fetchCtx->currResource ) {
        resource* currResource = fetchCtx->currResource;
        char *escaped_path = NULL;

        /* set pointer to next element */
        fetchCtx->currResource = fetchCtx->currResource->next;

        /* It seems strange: first uri->path is unescaped to escape it in the next step again.
         * The reason is that uri->path is not completely escaped (ie. it seems only to have
         * spaces escaped), while the fetchCtx->target is fully escaped.
         * See http://bugs.owncloud.org/thebuggenie/owncloud/issues/oc-613
         */
        escaped_path = ne_path_escape( currResource->uri );
        if (ne_path_compare(fetchCtx->target, escaped_path) != 0) {
            // Convert the resource for the caller
            csync_vio_file_stat_t* lfs = csync_vio_file_stat_new();
            resourceToFileStat(lfs, currResource);

            SAFE_FREE( escaped_path );
            return lfs;
        }

        /* This is the target URI */
        SAFE_FREE( escaped_path );
    }

    return NULL;
}

char *owncloud_error_string(CSYNC* ctx)
{
    return ctx->owncloud_context->dav_session.error_string;
}

int owncloud_commit(CSYNC* ctx) {
    if (!ctx->owncloud_context) {
        return 0;
    }

    clear_propfind_recursive_cache(ctx->owncloud_context);

    free_fetchCtx(ctx->owncloud_context->propfind_cache);
    ctx->owncloud_context->propfind_cache = NULL;

    if( ctx->owncloud_context->dav_session.ctx ) {
        ne_forget_auth(ctx->owncloud_context->dav_session.ctx);
        ne_session_destroy(ctx->owncloud_context->dav_session.ctx );
        ctx->owncloud_context->dav_session.ctx = 0;
    }

    ctx->owncloud_context->is_first_propfind = true;
  /* DEBUG_WEBDAV( "********** vio_module_shutdown" ); */

  ctx->owncloud_context->dav_session.ctx = 0;

  // ne_sock_exit();
  ctx->owncloud_context->_connected = 0;  /* triggers dav_connect to go through the whole neon setup */

  SAFE_FREE( ctx->owncloud_context->dav_session.user );
  SAFE_FREE( ctx->owncloud_context->dav_session.pwd );
  SAFE_FREE( ctx->owncloud_context->dav_session.session_key);
  SAFE_FREE( ctx->owncloud_context->dav_session.error_string );

  return 0;
}

void owncloud_destroy(CSYNC* ctx)
{
    owncloud_commit(ctx);
    SAFE_FREE(ctx->owncloud_context);
    ctx->owncloud_context = 0;
}

int owncloud_set_property(CSYNC* ctx, const char *key, void *data) {
#define READ_STRING_PROPERTY(P) \
    if (c_streq(key, #P)) { \
        SAFE_FREE(ctx->owncloud_context->dav_session.P); \
        ctx->owncloud_context->dav_session.P = c_strdup((const char*)data); \
        return 0; \
    }
    READ_STRING_PROPERTY(session_key)
    READ_STRING_PROPERTY(proxy_type)
    READ_STRING_PROPERTY(proxy_host)
    READ_STRING_PROPERTY(proxy_user)
    READ_STRING_PROPERTY(proxy_pwd)
#undef READ_STRING_PROPERTY

    if (c_streq(key, "proxy_port")) {
        ctx->owncloud_context->dav_session.proxy_port = *(int*)(data);
        return 0;
    }
    if (c_streq(key, "read_timeout") || c_streq(key, "timeout")) {
        ctx->owncloud_context->dav_session.read_timeout = *(int*)(data);
        return 0;
    }
    if( c_streq(key, "get_dav_session")) {
        /* Give the ne_session to the caller */
        *(ne_session**)data = ctx->owncloud_context->dav_session.ctx;
        return 0;
    }
    if( c_streq(key, "no_recursive_propfind")) {
        ctx->owncloud_context->dav_session.no_recursive_propfind = *(bool*)(data);
        return 0;
    }
    if( c_streq(key, "redirect_callback")) {
        if (data) {
            csync_owncloud_redirect_callback_t* cb_wrapper = data;

            ctx->owncloud_context->dav_session.redir_callback = *cb_wrapper;
        } else {
            ctx->owncloud_context->dav_session.redir_callback = NULL;
        }
    }

    return -1;
}

void owncloud_init(CSYNC* ctx) {

    ctx->owncloud_context = c_malloc( sizeof( struct csync_owncloud_ctx_s ));
    ctx->owncloud_context->csync_ctx = ctx; // back reference
    ctx->owncloud_context->is_first_propfind = true;

    /* Disable it, Mirall can enable it for the first sync (= no DB)*/
    ctx->owncloud_context->dav_session.no_recursive_propfind = true;
}

/* vim: set ts=4 sw=4 et cindent: */
