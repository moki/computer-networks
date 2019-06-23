#define _POSIX_C_SOURCE (200809L)
#define _XOPEN_SOURCE (700)
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <inttypes.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <errno.h>

#include "server.h"

static void usage(char **arguments);

static char *welcome_message = "Hello Dear Peer.\n";

int main(int argc, char **argv) {
	/* check server parameters */
	if (argc != 2) {
		usage(argv);
		exit(EXIT_FAILURE);
	}

	uintmax_t portnumber = strtoumax(argv[1], NULL, 10);
	if (portnumber == UINTMAX_MAX && errno == ERANGE) {
		fprintf(stderr, "could not convert port nubmer\n");
		exit(EXIT_FAILURE);
	}

	if (portnumber >= 65536) {
		usage(argv);
		exit(EXIT_FAILURE);
	}

	int listener;

	server_attr_t server_config;
	server_attr_init(&server_config);
	server_attr_setport(&server_config, argv[1]);
	server_attr_setbacklog(&server_config, 100);
	server_attr_setinternetproto(&server_config, AF_UNSPEC);
	server_attr_settransportproto(&server_config, SOCK_STREAM);

	listener = server_listen(&server_config);
	if (listener == -1) {
		fprintf(stderr, "failed start-up server\n");
		exit(EXIT_FAILURE);
	}

	close(listener);
	return EXIT_SUCCESS;
}

static void usage(char **arguments) {
	fprintf(stderr, "error, usage: %s port, where port is < 65536\n", arguments[0]);
}
