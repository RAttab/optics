/* opticsd.c
   Rémi Attab (remi.attab@gmail.com), 15 Mar 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "optics.h"
#include "utils/errors.h"
#include "utils/time.h"
#include "utils/crest/crest.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include <unistd.h>
#include <stdatomic.h>

#include <libdaemon/dfork.h>
#include <libdaemon/dpid.h>


// -----------------------------------------------------------------------------
// sigint
// -----------------------------------------------------------------------------

static atomic_uint_fast8_t sigint = 0;

static void sigint_handler(int signal)
{
    optics_assert(signal == SIGINT, "unexpected signal: %d", signal);
    atomic_store(&sigint, true);
}

static void install_sigint()
{
    struct sigaction sa = { .sa_handler = sigint_handler };
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        optics_fail_errno("unable to install SIGINT handler\n");
        optics_error_exit();
    }
}


// -----------------------------------------------------------------------------
// utils
// -----------------------------------------------------------------------------

#ifndef OPTICS_VERSION
# error "missing OPTICS_VERSION"
#endif

#define stringify_impl(x) #x
#define stringify(x) stringify_impl(x)
#define optics_version_str() stringify(OPTICS_VERSION)

static void parse_carbon(struct optics_poller *poller, const char *args)
{
    char *buffer = strdup(args);

    const char *host = buffer;
    const char *port = "2003";

    char *sep = strchr(buffer, ':');
    if (sep) {
        *sep = '\0';
        port = sep + 1;
    }

    optics_dump_carbon(poller, host, port);

    free(buffer);
}

static const char *daemon_pid_file(void)
{
    return "/var/run/opticsd.pid";
}

static void daemonize(void)
{
    daemon_pid_file_proc = daemon_pid_file;

    pid_t pid = daemon_pid_file_is_running();
    if (pid > 0) {
        optics_fail_errno("daemon is already running on pid: %d", pid);
        optics_error_exit();
    }

    pid = daemon_fork();
    if (pid > 0) exit(0);
    if (pid < 0) {
        optics_fail_errno("unable to fork daemon");
        optics_error_exit();
    }

    if (daemon_pid_file_create() < 0) {
        optics_fail_errno("unable to create pid file");
        optics_error_exit();
    }

    optics_syslog();
}

static void run_poller(struct optics_poller *poller, optics_ts_t freq)
{
    if (!optics_poller_poll(poller)) optics_abort();

    while (!atomic_load(&sigint)) {
        if (usleep(freq * 1000 * 1000) == -1 && errno != EINTR) {
            optics_fail_errno("unable to sleep for %lu seconds", freq);
            optics_abort();
        }

        if (!optics_poller_poll(poller)) optics_abort();
    }
}


// -----------------------------------------------------------------------------
// main
// -----------------------------------------------------------------------------

static void print_usage(void)
{
    fprintf(stdout,
            "Usage: \n"
            "  opticsd [--stdout] [--carbon=<host:port>]\n"
            "\n"
            "Options:\n"
            "  --dump-stdout              Dumps metrics to stdout\n"
            "  --dump-carbon=<host:port>  Dumps metrics to the given carbon host:port\n"
            "  --freq=<n>                 Number of seconds between each polling attempt [10]\n"
            "  --http-port=<port>         Port for HTTP server [3002]\n"
            "  --hostname=<hostname>      Hostname to include in the key [gethostname()]\n"
            "  --daemon                   Daemonizes the process\n"
            "  -v --version               Optics verison\n"
            "  -h --help                  Prints this message\n");
}


int main(int argc, char **argv)
{
    struct optics_poller *poller = optics_poller_alloc();
    optics_ts_t freq = 10;
    unsigned http_port = 3002;
    bool backend_selected = false;
    bool daemon = false;

    struct crest *crest = crest_new();
    optics_dump_rest(poller, crest);

    while (true) {
        static struct option options[] = {
            {"dump-carbon", required_argument, 0, 'c'},
            {"dump-stdout", no_argument, 0, 's'},
            {"freq", required_argument, 0, 'f'},
            {"http-port", required_argument, 0, 'H'},
            {"hostname", required_argument, 0, 'n'},
            {"daemon", no_argument, 0, 'd'},
            {"version", no_argument, 0, 'v'},
            {"help", no_argument, 0, 'h'},
            {0}
        };

        int opt_char = getopt_long(argc, argv, "vh", options, NULL);
        if (opt_char < 0) break;

        switch (opt_char) {
        case 'f':
            freq = atol(optarg);
            if (!freq) {
                optics_fail("invalid freq argument: %s", optarg);
                optics_error_exit();
            }
            break;

        case 'H':
            http_port = atol(optarg);
            if (!http_port) {
                optics_fail("invalid http argument: %s", optarg);
                optics_error_exit();
            }
            break;

        case 's':
            backend_selected = true;
            optics_dump_stdout(poller);
            break;

        case 'c':
            backend_selected = true;
            parse_carbon(poller, optarg);
            break;

        case 'n':
            if (!optics_poller_set_host(poller, optarg))
                optics_error_exit();
            break;

        case 'd':
            daemon = true;
            break;

        case 'v':
            fprintf(stdout,"opticsd v%s\n", optics_version_str());
            return EXIT_SUCCESS;

        case 'h':
            print_usage();
            return EXIT_SUCCESS;

        default:
            optics_fail("unknown argument: %s", optarg);
            optics_error_exit();
        }
    }

    if (!backend_selected) {
        optics_fail("No dump option selected");
        optics_error_exit();
    }

    if (daemon) daemonize();

    if (!crest_bind(crest, http_port)) optics_abort();
    install_sigint();

    run_poller(poller, freq);

    return EXIT_SUCCESS;
}
