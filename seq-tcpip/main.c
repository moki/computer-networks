#define _POSIX_C_SOURCE (200809L)
#define _XOPEN_SOURCE (1)
#include <sys/socket.h>
#include <sys/types.h>
#include <inttypes.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <netdb.h>
#include <errno.h>

#define BACKLOG (20)

static void usage(char **arguments);

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

	/* bootstrap server */
	struct addrinfo *server;
	struct addrinfo *cursor;
	struct addrinfo  hints;
	int		reuse_addr;
	int 		listener;
	int		err;

	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family   = AF_UNSPEC;
	hints.ai_flags	  = AI_PASSIVE | AI_NUMERICSERV;

	err = getaddrinfo(NULL, argv[1], &hints, &server);
	if (err != 0) {
		fprintf(stderr, "getaddrinfo: failed to configure host\n");
		return EXIT_FAILURE;
	}

	for (cursor = server;cursor != NULL; cursor = cursor->ai_next) {
		err = listener = socket(cursor->ai_family,
			     		cursor->ai_socktype,
			     		cursor->ai_protocol);
		if (err == -1) {
			fprintf(stderr, "failed to create socket\n");
			continue;
		}

		err = setsockopt(listener,
				 SOL_SOCKET,
				 SO_REUSEADDR,
				 &reuse_addr,
				 sizeof(int));
		if (err == -1) {
			fprintf(stderr, "failed to configure socket\n");
			exit(EXIT_FAILURE);
		}

		err = bind(listener, cursor->ai_addr, cursor->ai_addrlen);
		if (!err)
			break;

		fprintf(stderr, "failed to bind socket: %d\n", listener);
		close(listener);
	}

	if (!cursor) {
		fprintf(stderr, "failed to create, configure, bind socket\n");
		exit(EXIT_FAILURE);
	}

	err = listen(listener, BACKLOG);
	if (err == -1) {
		fprintf(stderr, "failed to listen for a connection\n");
		exit(EXIT_FAILURE);
	}

	freeaddrinfo(server);

	return EXIT_SUCCESS;
}

static void usage(char **arguments) {
	fprintf(stderr, "error, usage: %s port, where port is < 65536\n", arguments[0]);
}
