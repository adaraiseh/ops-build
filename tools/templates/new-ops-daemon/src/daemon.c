/*
 * Copyright (C) ${{year}} ${{author}} <${{email}}>
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License. You may obtain
 * a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#include <stdio.h>
#include <getopt.h>

#include "config.h"
#include <openvswitch/vlog.h>
#include <openvswitch/vconn.h>
#include <daemon.h>
#include <fatal-signal.h>
#include <unixctl.h>
#include <command-line.h>
#include <stream.h>
#include <stream-ssl.h>
#include <ovsdb-idl.h>
#include <vswitch-idl.h>
#include <poll-loop.h>
#include <util.h>
#include <dirs.h>
#include <simap.h>

#include "${{name}}.h"

VLOG_DEFINE_THIS_MODULE(ops_${{name}});

static struct ovsdb_idl *idl;
static unsigned int idl_seqno;


static void
usage(void)
{
    printf("%s: OpenSwitch ${{name}} daemon\n"
           "usage: %s [OPTIONS] [DATABASE]\n"
           "where DATABASE is a socket on which ovsdb-server is listening\n"
           "      (default: \"unix:%s/db.sock\").\n",
           program_name, program_name, ovs_rundir());
    stream_usage("DATABASE", true, false, true);
    daemon_usage();
    vlog_usage();
    printf("\nOther options:\n"
           "  --unixctl=SOCKET        override default control socket name\n"
           "  -h, --help              display this help message\n"
           "  -V, --version           display version information\n");
    exit(EXIT_SUCCESS);
}

static char *
parse_options(int argc, char *argv[], char **unixctl_pathp)
{
    enum {
        OPT_PEER_CA_CERT = UCHAR_MAX + 1,
        OPT_UNIXCTL,
        VLOG_OPTION_ENUMS,
        OPT_BOOTSTRAP_CA_CERT,
        OPT_ENABLE_DUMMY,
        OPT_DISABLE_SYSTEM,
        DAEMON_OPTION_ENUMS,
        OPT_DPDK,
    };
    static const struct option long_options[] = {
        {"help",        no_argument, NULL, 'h'},
        {"version",     no_argument, NULL, 'V'},
        {"unixctl",     required_argument, NULL, OPT_UNIXCTL},
        DAEMON_LONG_OPTIONS,
        VLOG_LONG_OPTIONS,
        STREAM_SSL_LONG_OPTIONS,
        {"peer-ca-cert", required_argument, NULL, OPT_PEER_CA_CERT},
        {"bootstrap-ca-cert", required_argument, NULL, OPT_BOOTSTRAP_CA_CERT},
        {NULL, 0, NULL, 0},
    };
    char *short_options = long_options_to_short_options(long_options);

    for (;;) {
        int c;

        c = getopt_long(argc, argv, short_options, long_options, NULL);
        if (c == -1) {
            break;
        }

        switch (c) {
        case 'h':
            usage();

        case 'V':
            ovs_print_version(OFP10_VERSION, OFP10_VERSION);
            exit(EXIT_SUCCESS);

        case OPT_UNIXCTL:
            *unixctl_pathp = optarg;
            break;

        VLOG_OPTION_HANDLERS
        DAEMON_OPTION_HANDLERS
        STREAM_SSL_OPTION_HANDLERS

        case OPT_PEER_CA_CERT:
            stream_ssl_set_peer_ca_cert_file(optarg);
            break;

        case OPT_BOOTSTRAP_CA_CERT:
            stream_ssl_set_ca_cert_file(optarg, true);
            break;

        case '?':
            exit(EXIT_FAILURE);

        default:
            abort();
        }
    }
    free(short_options);

    argc -= optind;
    argv += optind;

    switch (argc) {
    case 0:
        return xasprintf("unix:%s/db.sock", ovs_rundir());

    case 1:
        return xstrdup(argv[0]);

    default:
        VLOG_FATAL("at most one non-option argument accepted; "
                   "use --help for usage");
    }
}

static void
${{name}}_init(const char *remote)
{

    /* Create connection to OVSDB. */
    idl = ovsdb_idl_create(remote, &ovsrec_idl_class, false, true);
    idl_seqno = ovsdb_idl_get_seqno(idl);
    ovsdb_idl_set_lock(idl, "ops_${{name}}");
    ovsdb_idl_verify_write_only(idl);

    /* TODO Register for daemon table. */

    /* TODO Register unixctl commands. */
}

/* Process any changes to cached data. */
static void
daemon_reconfigure(struct ovsdb_idl *idl)
{
    unsigned int new_idl_seqno = ovsdb_idl_get_seqno(idl);

    if (new_idl_seqno == idl_seqno)
        return;

    idl_seqno = new_idl_seqno;

    /* TODO: Handle any added or deleted subsystems. */

    /* TODO: Remove any subsystems that are no longer present in the db. */
}

/* Perform all of the per-loop processing. */
static void
daemon_run(void)
{
    ovsdb_idl_run(idl);

    if (ovsdb_idl_is_lock_contended(idl)) {
        static struct vlog_rate_limit rl = VLOG_RATE_LIMIT_INIT(1, 1);
        VLOG_ERR_RL(&rl, "another ops-${{name}} process is running, "
                    "disabling this process until it goes away");
        return;
    } else if (!ovsdb_idl_has_lock(idl)) {
        return;
    }

    daemon_reconfigure(idl);

    ${{name}}_run();

    daemonize_complete();
    vlog_enable_async();
    VLOG_INFO_ONCE("%s (OpenSwitch ${{name}}) %s", program_name, VERSION);
}

static void
${{name}}_wait(void)
{
    ovsdb_idl_wait(idl);
    poll_timer_wait(POLLING_PERIOD_MS);
}

static void
${{name}}_exit(void)
{
    ovsdb_idl_destroy(idl);
}

static void
ops_${{name}}_exit(struct unixctl_conn *conn, int argc OVS_UNUSED,
                  const char *argv[] OVS_UNUSED, void *exiting_)
{
    bool *exiting = exiting_;
    *exiting = true;
    unixctl_command_reply(conn, NULL);
}

int
main(int argc, char *argv[])
{
    char *unixctl_path = NULL;
    struct unixctl_server *unixctl;
    char *remote;
    bool exiting;
    int retval;

    set_program_name(argv[0]);

    proctitle_init(argc, argv);
    remote = parse_options(argc, argv, &unixctl_path);
    fatal_ignore_sigpipe();

    ovsrec_init();

    daemonize_start();

    retval = unixctl_server_create(unixctl_path, &unixctl);
    if (retval) {
        exit(EXIT_FAILURE);
    }
    unixctl_command_register("exit", "", 0, 0, ops_${{name}}_exit, &exiting);

    ${{name}}_init(remote);
    free(remote);

    exiting = false;
    while (!exiting) {
        daemon_run();
        unixctl_server_run(unixctl);

        ${{name}}_wait();
        unixctl_server_wait(unixctl);
        if (exiting) {
            poll_immediate_wake();
        }
        poll_block();
    }
    ${{name}}_exit();
    unixctl_server_destroy(unixctl);

    return 0;
}
