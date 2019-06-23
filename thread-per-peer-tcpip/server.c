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

int server_attr_setport(server_attr_t *server_attr, char *port) {
	if (!server_attr)
		return -1;
	if (port == NULL)
		return -1;
	server_attr->port = port;
	return 0;
}

int server_attr_settransportproto(server_attr_t *server_attr, int protocol) {
	if (!server_attr)
		return -1;
	if (protocol != SOCK_STREAM)
		return -1;
	server_attr->transport_proto = protocol;
	return 0;
}

int server_attr_setinternetproto(server_attr_t *server_attr, int protocol) {
	if (!server_attr)
		return -1;
	if (protocol != AF_UNSPEC &&
	    protocol != AF_INET &&
	    protocol != AF_INET6)
		return -1;
	server_attr->internet_proto = protocol;
	return 0;
}

int server_attr_init(server_attr_t *server_attr) {
	if (!server_attr)
		return -1;
	server_attr->backlog = 20;
	server_attr->port = "3000";
	server_attr->internet_proto = AF_UNSPEC;
	return 0;
}

int server_listen(server_attr_t *server_attr) {
	if (!server_attr) {
		fprintf(stderr, "server->server_listen: server_attr == NULL\n");
		return -1;
	}

	if (server_attr->port == NULL) {
		fprintf(stderr, "server->server_listen: server_attr == NULL\n");
		return -1;
	}
	if (server_attr->transport_proto != SOCK_STREAM) {
		fprintf(stderr, "server->server_listen: only tcp supported\n");
		return -1;
	}
	if (server_attr->internet_proto != AF_UNSPEC &&
	    server_attr->internet_proto != AF_INET &&
	    server_attr->internet_proto != AF_INET6) {
		fprintf(stderr, "server->server_listen: invalid internet protocol\n");
		return -1;
	}

	struct addrinfo *server;
	struct addrinfo *cursor;
	struct addrinfo  hints;
	int		reuse_addr;
	int 		listener;
	int		err;

	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = server_attr->transport_proto;
	hints.ai_family   = server_attr->internet_proto;
	hints.ai_flags	  = AI_PASSIVE;

	err = getaddrinfo(NULL, server_attr->port, &hints, &server);
	if (err) {
		fprintf(stderr, "server->server_listen->getaddrinfo\n");
		return -1;
	}

	for (cursor = server;cursor != NULL; cursor = cursor->ai_next) {
		err = listener = socket(cursor->ai_family,
			     		cursor->ai_socktype,
			     		cursor->ai_protocol);
		if (err == -1) {
			fprintf(stderr, "server->server_listen->socket\n");
			continue;
		}

		err = setsockopt(listener,
				 SOL_SOCKET,
				 SO_REUSEADDR,
				 &reuse_addr,
				 sizeof(int));
		if (err == -1) {
			fprintf(stderr, "server->server_listen->settsockopt\n");
			freeaddrinfo(server);
			return -1;
		}

		err = bind(listener, cursor->ai_addr, cursor->ai_addrlen);
		if (!err)
			break;

		fprintf(stderr, "server->server_listen->bind, socket: %d\n", listener);
		close(listener);
	}

	if (!cursor) {
		fprintf(stderr, "server->server_listen->failed to create, configure, bind socket\n");
		freeaddrinfo(server);
		return -1;
	}

	err = listen(listener, server_attr->backlog);
	if (err == -1) {
		fprintf(stderr, "server->server_listen->listen\n");
		freeaddrinfo(server);
		return -1;
	}

	fprintf(stdout, "listening on %s\n", server_attr->port);

	freeaddrinfo(server);

	return listener;
}
