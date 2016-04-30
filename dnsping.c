/*
 * Copyright (c) 2016, Babak Farrokhi All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * 
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 * 
 */

#include <math.h>
#include <sys/socket.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <ldns/ldns.h>

#define VERSION 1.1

#ifdef __linux__
#define PROGNAME program_invocation_short_name
#else
#define PROGNAME getprogname()
#endif				/* __linux__ */

/* global vars */
bool		should_stop = false;

void
usage()
{
	const char     *progname = PROGNAME;
	printf("%s version %1.1f\n", progname, VERSION);
	printf("usage: %s [-h] [-q] [-v] [-s server] [-p port] [-P port] [-S address] [-c count] [-t type] [-w wait] hostname\n", progname);
	printf("  -h    Show this help\n");
	printf("  -q    Quiet\n");
	printf("  -v    Print actual dns response\n");
	printf("  -s    DNS server to use (default: 8.8.8.8)\n");
	printf("  -p    DNS server port number (default: 53)\n");
	printf("  -P    Query source port number (default: 0)\n");
	printf("  -S    Query source IP address (default: default interface address)\n");
	printf("  -c    Number of requests to send (default: 10)\n");
	printf("  -w    Maximum wait time for a reply (default: 5)\n");
	printf("  -t    DNS request record type (default: A)\n\n");
	/* make sure exit() is called properly after calling this function */
}

/*
 * Act upon receiving signals
 */
void
signal_handler(int sig)
{

	if (should_stop) {
		exit(EXIT_SUCCESS);
	}
	switch (sig) {

	case SIGHUP:
	case SIGINT:
	case SIGTERM:
		should_stop = true;
		break;
	default:
		break;
	}
}

inline void 
ldns_check_for_error(ldns_status s)
{
	if (s == LDNS_STATUS_OK) {
		return;
	}
	printf("error: %s\n", ldns_get_errorstr_by_id(s));
	exit(EXIT_FAILURE);
}

/* code from https://www.strchr.com/standard_deviation_in_one_pass */
double 
std_dev(double a[], int n)
{
	if (n == 0)
		return 0.0;
	double		sum = 0;
	for (int i = 0; i < n; ++i)
		sum += a[i];
	double		mean = sum / n;
	double		sq_diff_sum = 0;
	for (int i = 0; i < n; ++i) {
		double		diff = a[i] - mean;
		sq_diff_sum += diff * diff;
	}
	double		variance = sq_diff_sum / n;
	return sqrt(variance);
}

int
main(int argc, char *argv[])
{
	int		opt;
	bool	quiet, verbose;
	int		src_port, dst_port, count, timeout;
	char    *server, *host, *dnsrecord;
	struct sigaction sig_action;
	sigset_t	sig_set;
	struct timeval	t1, t2;
	double		elapsed;
	double		r_min  , r_max, r_avg, r_sum;
	u_int		received;


	ldns_resolver  *res = NULL;
	ldns_rdf       *domain;
	ldns_rdf       *dnsserver;
	ldns_pkt       *p;
	ldns_rr_list   *rr;
	ldns_status		s;
	ldns_rr_type	rtype;
	ldns_pkt_type	reply_type;


	/* Block unnecessary signals */
	sigemptyset(&sig_set);
	sigaddset(&sig_set, SIGTSTP);	/* ignore tty stop signals */
	sigaddset(&sig_set, SIGTTOU);	/* ignore tty background writes */
	sigaddset(&sig_set, SIGTTIN);	/* ignore tty background reads */
	sigprocmask(SIG_BLOCK, &sig_set, NULL);	/* Block the above specified
						 * signals */

	/* Catch necessary signals */
	sig_action.sa_handler = signal_handler;
	sigemptyset(&sig_action.sa_mask);
	sig_action.sa_flags = 0;

	sigaction(SIGTERM, &sig_action, NULL);
	sigaction(SIGHUP, &sig_action, NULL);
	sigaction(SIGINT, &sig_action, NULL);


	/* defaults */
	quiet = false;
	verbose = false;
	src_port = 0;
	dst_port = 53;
	count = 10;
	timeout = 5;
	server = NULL;
	dnsrecord = strdup("A");

	r_min = 0;
	r_max = 0;
	r_avg = 0;
	r_sum = 0;
	received = 0;

	while ((opt = getopt(argc, argv, "hqvs:c:t:")) != -1) {
		switch (opt) {
		case 'h':
			usage();
			exit(EXIT_SUCCESS);
			break;
		case 'q':
			quiet = true;
			verbose = false;
			break;
		case 'v':
			verbose = true;
			quiet = false;
			break;
		case 's':
			server = strdup(optarg);
			break;
		case 'c':
			count = atoi(optarg);
			break;
		case 't':
			dnsrecord = optarg;
			break;
		default:
			printf("invalid option: %c\n", opt);
			usage();
			exit(EXIT_FAILURE);
		}
	}

	argv += optind;
	argc -= optind;
	host = argv[0];

	if (argc < 1) {		/* no hostname given */
		printf("error: please specify a host name\n");
		usage();
		exit(EXIT_SUCCESS);
	}

	/* cook ldns food */
	rtype = ldns_get_rr_type_by_name(dnsrecord);
	res = ldns_resolver_new();
	domain = ldns_dname_new_frm_str(host);

	if (server == NULL) {	/* use system resolver */
		s = ldns_resolver_new_frm_file(&res, NULL);
		if (res->_nameserver_count < 1) {
			printf("no name servers found\n");
			exit(EXIT_FAILURE);
		}
		/* TODO: initialize 'server' with a sane value */
	} else {		/* use given nameserver */
		dnsserver = ldns_rdf_new_frm_str(LDNS_RDF_TYPE_A, server);
		s = ldns_resolver_push_nameserver(res, dnsserver);
	}
	ldns_check_for_error(s);

	setbuf(stdout, NULL);	/* flush every single line */

	printf("%s DNS: %s:%d, hostname: %s, rdatatype: %s\n", PROGNAME, server, dst_port, host, dnsrecord);

	int		i;
	double		rtimes  [count];
	bzero(&rtimes, sizeof(rtimes));

	for (i = 0; i < count; i++) {

		if (should_stop)
			break;	/* CTRL+C pressed once */

		gettimeofday(&t1, NULL);
		s = ldns_resolver_query_status(&p, res,
					       domain,
					       rtype,
					       LDNS_RR_CLASS_IN,
					       LDNS_RD);
		gettimeofday(&t2, NULL);
		reply_type = ldns_pkt_reply_type(p);
		if (s == LDNS_STATUS_OK) {
			received++;
		} else {
			printf("%s\n", ldns_get_errorstr_by_id(s));
		}

		elapsed = (t2.tv_sec - t1.tv_sec) * 1000.0;
		//sec to ms
			elapsed += (t2.tv_usec - t1.tv_usec) / 1000.0;
		//us to ms
			if (verbose) {
			rr = ldns_pkt_rr_list_by_type(p,
						      rtype,
						      LDNS_SECTION_ANSWER);
			ldns_rr_list_sort(rr);
			ldns_rr_list_print(stdout, rr);

		}
		r_sum += elapsed;
		rtimes[i] = elapsed;
		if (r_min == 0 || r_min > elapsed) {
			r_min = elapsed;
		}
		if (r_max == 0 || elapsed > r_max) {
			r_max = elapsed;
		}
		printf("%zu bytes from %s: seq=%-3d time=%3.3f ms\n", ldns_pkt_size(p), server, i, elapsed);
	}
	r_avg = r_sum / count;

	printf("--- %s %s statistics ---\n", server, PROGNAME);
	printf("%u requests transmitted, %u responses received,  %u%% lost\n", i, received, 0);
	printf("min=%.3f ms, avg=%.3f ms, max=%.3f ms, stddev=%.3f ms\n", r_min, r_avg, r_max, std_dev(rtimes, i));

	if (verbose) {
		ldns_rr_list_deep_free(rr);
	}
	ldns_pkt_free(p);
	ldns_resolver_deep_free(res);
	exit(EXIT_SUCCESS);
}
