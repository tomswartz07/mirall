/*
 * libcsync -- a library to sync a directory with another
 *
 * Copyright (c) 2008-2013 by Andreas Schneider <asn@cryptomilk.org>
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
#include <string.h>
#include <unistd.h>

#include "torture.h"

#define CSYNC_TEST 1
#include "csync_statedb.c"

#define TESTDB "/tmp/check_csync1/test.db"

static void setup(void **state) {
    CSYNC *csync;
    int rc;

    rc = system("rm -rf /tmp/check_csync1");
    assert_int_equal(rc, 0);

    rc = system("mkdir -p /tmp/check_csync1");
    assert_int_equal(rc, 0);

    rc = csync_create(&csync, "/tmp/check_csync1", "/tmp/check_csync2");
    assert_int_equal(rc, 0);

    csync->statedb.file = c_strdup( TESTDB );
    *state = csync;
}

static void teardown(void **state) {
    CSYNC *csync = *state;
    int rc;

    rc = csync_destroy(csync);
    assert_int_equal(rc, 0);

    rc = system("rm -rf /tmp/check_csync1");
    assert_int_equal(rc, 0);

    *state = NULL;
}

static void check_csync_statedb_check(void **state)
{
    int rc;

    (void) state; /* unused */

    rc = system("mkdir -p /tmp/check_csync1");

    /* old db */
    rc = system("echo \"SQLite format 2\" > /tmp/check_csync1/test.db");
    assert_int_equal(rc, 0);
    rc = _csync_statedb_check(TESTDB);
    assert_int_equal(rc, 1);

    /* db already exists */
    rc = _csync_statedb_check(TESTDB);
    assert_int_equal(rc, 1);

    /* no db exists */
    rc = system("rm -f /tmp/check_csync1/test.db");
    assert_int_equal(rc, 0);
    rc = _csync_statedb_check(TESTDB);
    assert_int_equal(rc, 1);

    rc = _csync_statedb_check("/tmp/check_csync1/");
    assert_int_equal(rc, -1);

    rc = system("rm -rf /tmp/check_csync1");
    assert_int_equal(rc, 0);
}

static void check_csync_statedb_load(void **state)
{
    CSYNC *csync = *state;
    int rc;

    rc = csync_statedb_load(csync, TESTDB, &csync->statedb.db);
    assert_int_equal(rc, 0);

    sqlite3_close(csync->statedb.db);
}

static void check_csync_statedb_close(void **state)
{
    CSYNC *csync = *state;
    csync_stat_t sb;
    time_t modtime;
    mbchar_t *testdb = c_utf8_to_locale(TESTDB);
    int rc;

    /* statedb not written */
    csync_statedb_load(csync, TESTDB, &csync->statedb.db);

    rc = _tstat(testdb, &sb);
    assert_int_equal(rc, 0);
    modtime = sb.st_mtime;

    rc = csync_statedb_close(csync);
    assert_int_equal(rc, 0);

    rc = _tstat(testdb, &sb);
    assert_int_equal(rc, 0);
    assert_int_equal(modtime, sb.st_mtime);

    csync_statedb_load(csync, TESTDB, &csync->statedb.db);

    rc = _tstat(testdb, &sb);
    assert_int_equal(rc, 0);
    modtime = sb.st_mtime;

    /* wait a sec or the modtime will be the same */
    sleep(1);

    /* statedb written */
    rc = csync_statedb_close(csync);
    assert_int_equal(rc, 0);

    rc = _tstat(testdb, &sb);
    assert_int_equal(rc, 0);

    c_free_locale_string(testdb);
}

int torture_run_tests(void)
{
    const UnitTest tests[] = {
        unit_test_setup_teardown(check_csync_statedb_check, setup, teardown),
        unit_test_setup_teardown(check_csync_statedb_load, setup, teardown),
        unit_test_setup_teardown(check_csync_statedb_close, setup, teardown),
    };

    return run_tests(tests);
}

