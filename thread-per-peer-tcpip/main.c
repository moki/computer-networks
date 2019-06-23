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
#include <pthread.h>

#include "server.h"

static void usage(char **arguments);

static char *welcome_message = "Hello Dear Peer.\n";

static void *serve_peer(void *peer_state);

struct peer_state_t {
	int socket;
};

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

	fd_set _server_fds;
	fd_set server_fds;
	int listener;
	int err;

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

	FD_ZERO(&server_fds);
	FD_ZERO(&_server_fds);

	FD_SET(STDIN_FILENO, &_server_fds);
	FD_SET(listener, &_server_fds);

	/* server loop */
	for (;;) {
		struct sockaddr_storage peer_addr;
		socklen_t 		peer_addr_len;
		size_t 			cmd_len;
		int 			peer_socket;

		server_fds = _server_fds;
		char *cmd = NULL;

		/* stdin command or peer connection? */
		err = select(listener + 1, &server_fds, NULL, NULL, NULL);
		if (err == -1) {
			fprintf(stderr, "failed to select socket that yield\n");
			pthread_exit(NULL);
		}

		/* command */
		if (FD_ISSET(STDIN_FILENO, &server_fds)) {
			(void) getline(&cmd, &cmd_len, stdin);
			if (strcmp(cmd, "quit\n") == 0) {
				free(cmd);
				goto exit_main_thread;
			}
			free(cmd);
			continue;
		}

		/* peer connection */
		peer_addr_len = sizeof(struct sockaddr_storage);
		peer_socket = accept(listener, (struct sockaddr *) &peer_addr, &peer_addr_len);
		if (peer_socket == -1)
			fprintf(stderr, "failed to serve peer\n");

		pthread_t peer_thread;
		struct peer_state_t *peer_state;
		peer_state = malloc(sizeof(struct peer_state_t));
		if (!peer_state) {
			fprintf(stderr, "server->server_loop: failed to thread for a peer\n");
			pthread_exit(NULL);
		}
		peer_state->socket = peer_socket;

		pthread_create(&peer_thread, NULL, serve_peer, (void *) peer_state);
	}

exit_main_thread:
	close(listener);
	pthread_exit(NULL);
}

static void *serve_peer(void *peer_state) {
	pthread_detach(pthread_self());
	struct peer_state_t *state;
	size_t peer_msg_len;
	int err;

	state = (struct peer_state_t *) peer_state;
	char *peer_msg = NULL;

	FILE *stream = fdopen(state->socket, "r");
	if (!stream) {
		fprintf(stderr, "failed to read from peer\n");
		close(state->socket);
		goto exit_peer_thread;
	}

	(void ) getline(&peer_msg, &peer_msg_len, stream);
	fprintf(stdout, "peer says:\n%s", peer_msg);
	free(peer_msg);

	err = send(state->socket, welcome_message, strlen(welcome_message), 0);
	if (err == -1)
		fprintf(stderr, "failed to send data to peer\n");

	fclose(stream);
	close(state->socket);

exit_peer_thread:
	free(state);
	pthread_exit(NULL);
}

static void usage(char **arguments) {
	fprintf(stderr, "error, usage: %s port, where port is < 65536\n", arguments[0]);
}
