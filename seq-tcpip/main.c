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

#define BACKLOG (20)
#define STDIN_FD (0)

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

	/* bootstrap server */
	struct addrinfo *server;
	struct addrinfo *cursor;
	struct addrinfo  hints;
	fd_set		_server_fds;
	fd_set		server_fds;
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
	fprintf(stdout, "server: listening for incoming connections on port %s\n", argv[1]);

	FD_ZERO(&server_fds);
	FD_ZERO(&_server_fds);

	FD_SET(STDIN_FILENO, &_server_fds);
	FD_SET(listener, &_server_fds);

	/* server loop */
	for (;;) {
		struct sockaddr_storage peer_addr;
		socklen_t 		peer_addr_len;
		FILE 			*peer_stream;
		size_t 			peer_msg_len;
		size_t 			cmd_len;
		int 			peer;

		server_fds = _server_fds;
		char *peer_msg = NULL;
		char *cmd = NULL;

		err = select(listener + 1, &server_fds, NULL, NULL, NULL);
		if (err == -1) {
			fprintf(stderr, "failed to select socket that yield\n");
			exit(EXIT_FAILURE);
		}

		if (FD_ISSET(STDIN_FILENO, &server_fds)) {
			(void) getline(&cmd, &cmd_len, stdin);
			if (strcmp(cmd, "quit\n") == 0)
				exit(EXIT_SUCCESS);
			free(cmd);
			continue;
		}

		peer_addr_len = sizeof(struct sockaddr_storage);
		peer = accept(listener, (struct sockaddr *) &peer_addr, &peer_addr_len);
		if (peer == -1)
			fprintf(stderr, "failed to serve peer\n");

		peer_stream = fdopen(peer, "r");

		if (!peer_stream) {
			fprintf(stderr, "failed to read from peer\n");
			close(peer);
			continue;
		}

		(void ) getline(&peer_msg, &peer_msg_len, peer_stream);
		fprintf(stdout, "peer says:\n%s", peer_msg);
		free(peer_msg);

		err = send(peer, welcome_message, strlen(welcome_message), 0);
		if (err == -1)
			fprintf(stderr, "failed to send data to peer\n");

		fclose(peer_stream);
		close(peer);
	}

	close(listener);
	return EXIT_SUCCESS;
}

static void usage(char **arguments) {
	fprintf(stderr, "error, usage: %s port, where port is < 65536\n", arguments[0]);
}
