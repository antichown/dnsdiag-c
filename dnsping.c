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

inline void ldns_check_for_error(ldns_status s)
{
	if (s == LDNS_STATUS_OK){
		return;
	}
	printf("error: %s\n",ldns_get_errorstr_by_id(s));
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
	int		opt;
	bool	quiet ,verbose;
	int		src_port, dst_port, count, timeout;
	char    *server, *host;
	struct  sigaction sig_action;
	sigset_t sig_set;
    struct timeval t1, t2;
    double 	elapsed;


	ldns_resolver  *res = NULL;
	ldns_rdf       *domain;
	ldns_rdf       *dnsserver;
	ldns_pkt       *p;
	// ldns_rr_list   *rr;
	ldns_status		s;
	ldns_rr_type  rtype;


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
	server = strdup("8.8.8.8");
	rtype = LDNS_RR_TYPE_A;

	while ((opt = getopt(argc, argv, "hqvs:c:")) != -1) {
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
		default:
			printf("invalid option: %c\n", opt);
			usage();
			exit(EXIT_FAILURE);
		}
	}

	argv += optind;
	argc -= optind;
	host = argv[0];

	if (argc < 1) { /* no hostname given */
		printf("error: please specify a host name\n");
		usage();
		exit(EXIT_SUCCESS);
	}

	/* cook ldns food */
	domain = ldns_dname_new_frm_str(host);
	res = ldns_resolver_new();
    dnsserver = ldns_rdf_new_frm_str(LDNS_RDF_TYPE_A,  server);
	s = ldns_resolver_push_nameserver(res, dnsserver);
	ldns_check_for_error(s);
	/*s = ldns_resolver_new_frm_file(&res, NULL);
	ldns_check_for_error(s);*/
	setbuf(stdout, NULL); /* flush every single line */
	for (int i=0; i < count; i++){

		gettimeofday(&t1, NULL);
		p = ldns_resolver_query(res,
	                                domain,
	                                rtype,
	                                LDNS_RR_CLASS_IN,
	                                LDNS_RD);
		gettimeofday(&t2, NULL);
/*		rr = ldns_pkt_rr_list_by_type(p,
	                                    rtype,
	                                    LDNS_SECTION_ANSWER);
*/
		elapsed = (t2.tv_sec - t1.tv_sec) * 1000.0;      // sec to ms
    	elapsed += (t2.tv_usec - t1.tv_usec) / 1000.0;   // us to ms
/*		ldns_rr_list_sort(rr); 
	    ldns_rr_list_print(stdout, rr);*/
		printf("%lu bytes from %s: seq=%-3d time=%3.3f ms\n", sizeof(*p), server, i, elapsed);
	}

    // ldns_rr_list_deep_free(rr);
    ldns_pkt_free(p);
    ldns_resolver_deep_free(res);
	exit(EXIT_SUCCESS);
}
