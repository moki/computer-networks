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
#include "ptpool.h"

static void usage(char **arguments);

static char *welcome_message = "Hello Dear Peer.\n";

static void serve_peer(void *peer_state);

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

	ptpool_t *thread_pool;
	ptpool_attr_t *thread_pool_attr;

	fd_set _server_fds;
	fd_set server_fds;
	int listener;
	int err;

	server_attr_t server_attr;
	server_attr_init(&server_attr);
	server_attr_setport(&server_attr, argv[1]);
	server_attr_setbacklog(&server_attr, 100);
	server_attr_setinternetproto(&server_attr, AF_UNSPEC);
	server_attr_settransportproto(&server_attr, SOCK_STREAM);

	listener = server_listen(&server_attr);
	if (listener == -1) {
		fprintf(stderr, "server: failed start-up server\n");
		exit(EXIT_FAILURE);
	}

	FD_ZERO(&server_fds);
	FD_ZERO(&_server_fds);

	FD_SET(STDIN_FILENO, &_server_fds);
	FD_SET(listener, &_server_fds);

	thread_pool_attr = malloc(sizeof(ptpool_attr_t));
	err = ptpool_attr_init(thread_pool_attr);
	if (err) {
		fprintf(stderr, "server: failed to init attr of a thread pool\n");
		exit(EXIT_FAILURE);
	}
	err = ptpool_attr_setpoolsize(thread_pool_attr, 2);
	if (err) {
		fprintf(stderr, "server: failed to set thread pool size\n");
		exit(EXIT_FAILURE);
	}
	err = ptpool_attr_setqueuesize(thread_pool_attr, 2);
	if (err) {
		fprintf(stderr, "server: failed to set queue size\n");
		exit(EXIT_FAILURE);
	}
	thread_pool = malloc(sizeof(ptpool_t));
	err = ptpool_init(thread_pool, thread_pool_attr);
	if (err) {
		fprintf(stderr, "server: failed to init thread pool\n");
		exit(EXIT_FAILURE);
	}

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
			fprintf(stderr, "server->server_loop: failed to select socket that yield\n");
			exit(EXIT_FAILURE);
		}

		/* command */
		if (FD_ISSET(STDIN_FILENO, &server_fds)) {
			(void) getline(&cmd, &cmd_len, stdin);
			if (strcmp(cmd, "quit\n") == 0) {
				free(cmd);
				// goto exit_main_thread;
				err = ptpool_destroy(thread_pool, true);
				if (err)
					exit(EXIT_FAILURE);
				goto exit_main_thread;
			}
			free(cmd);
			continue;
		}

		/* peer connection */
		peer_addr_len = sizeof(struct sockaddr_storage);
		peer_socket = accept(listener, (struct sockaddr *) &peer_addr, &peer_addr_len);
		if (peer_socket == -1) {
			fprintf(stderr, "server->server_loop: failed to serve peer\n");
			continue;
		}

		struct peer_state_t *peer_state;
		peer_state = malloc(sizeof(struct peer_state_t));
		if (!peer_state) {
			fprintf(stderr, "server->server_loop: failed to allocate peer state\n");
			exit(EXIT_FAILURE);
		}
		peer_state->socket = peer_socket;

		err = ptpool_wqueue_add(thread_pool, serve_peer, peer_state);
		if (err == 2) {
			fprintf(stderr, "server->server_loop->thread_pool: queue is already closed\n");
			free(peer_state);
			continue;
		}
		if (err) {
			fprintf(stderr, "server->server_loop->thread_pool: failed to add work to queue\n");
			exit(EXIT_FAILURE);
		}
	}

exit_main_thread:
	close(listener);
	pthread_exit(NULL);
}

static void serve_peer(void *peer_state) {
	struct peer_state_t *state;
	size_t peer_msg_len;
	int err;

	state = (struct peer_state_t *) peer_state;
	char *peer_msg = NULL;

	FILE *stream = fdopen(state->socket, "r");
	if (!stream) {
		fprintf(stderr, "failed to read from peer\n");
		close(state->socket);
	}

	(void ) getline(&peer_msg, &peer_msg_len, stream);
	fprintf(stdout, "peer says:\n%s", peer_msg);
	free(peer_msg);

	err = send(state->socket, welcome_message, strlen(welcome_message), 0);
	if (err == -1)
		fprintf(stderr, "failed to send data to peer\n");

	fclose(stream);
	close(state->socket);

	free(state);
}

static void usage(char **arguments) {
	fprintf(stderr, "error, usage: %s port, where port is < 65536\n", arguments[0]);
}
