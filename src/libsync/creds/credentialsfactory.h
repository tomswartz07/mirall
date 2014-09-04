/*
 * Copyright (C) by Krzesimir Nowak <krzesimir@endocode.com>
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

#ifndef MIRALL_CREDS_CREDENTIALS_FACTORY_H
#define MIRALL_CREDS_CREDENTIALS_FACTORY_H

#include "owncloudlib.h"

class QString;

namespace Mirall
{
class AbstractCredentials;

namespace CredentialsFactory
{

OWNCLOUDSYNC_EXPORT AbstractCredentials* create(const QString& type);

} // ns CredentialsFactory

} // ns Mirall

#endif
