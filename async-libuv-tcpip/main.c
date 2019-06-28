#define _POSIX_C_SOURCE (200809L)
#define _XOPEN_SOURCE (700)
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <uv.h>

#define BACKLOG (64)
#define SENDBUF_SIZE (1024)

typedef struct {
	uv_write_t req;
	uv_buf_t buf;
} write_req_t;

typedef struct {
	char 	  sendbuf[SENDBUF_SIZE];
	size_t 	  sendbuf_end;
	uv_tcp_t *peer;
} peer_state_t;

static void alloc_buffer(uv_handle_t *handle, size_t ssize, uv_buf_t *b);
static void read_stdin(uv_stream_t *stream, ssize_t nread, const uv_buf_t *b);
static void write_data(uv_stream_t *d, size_t size, uv_buf_t b, uv_write_cb cb);
static void on_stdout_write(uv_write_t *req, int status);
static void free_write_req(uv_write_t *req);
static void usage(char **arguments);

static void on_peer_connected(uv_stream_t *server_stream, int status);
static void on_peer_closed(uv_handle_t *handle);
static void on_peer_write(uv_write_t *req, int status);

static char *welcome_message = "Hello Dear Peer.\n";

uv_pipe_t stdin_pipe;
uv_pipe_t stdout_pipe;
uv_loop_t *loop;


int main(int argc, char **argv) {
	/* check server parameters */
	if (argc != 2) {
		usage(argv);
		exit(EXIT_FAILURE);
	}
	uintmax_t portnumber = strtoumax(argv[1], NULL, 10);
	if (portnumber == UINTMAX_MAX && errno == ERANGE) {
		fprintf(stderr, "failed to parse port\n");
		exit(EXIT_FAILURE);
	}
	if (portnumber >= 65536) {
		usage(argv);
		exit(EXIT_FAILURE);
	}
	setvbuf(stdout, NULL, _IONBF, 0);
	loop = uv_default_loop();
	uv_pipe_init(loop, &stdin_pipe, 0);
	uv_pipe_open(&stdin_pipe, 0);
	uv_pipe_init(loop, &stdout_pipe, 0);
	uv_pipe_open(&stdout_pipe, 1);

	// struct sockaddr_in6;
	struct sockaddr_in6 server_address;
	uv_tcp_t server_stream;
	int err;

	err = uv_tcp_init(loop, &server_stream);
	if (err < 0) {
		fprintf(stderr, "failed to init tcp server\n");
		exit(EXIT_FAILURE);
	}

	err = uv_ip6_addr("::", portnumber, (struct sockaddr_in6 *) &server_address);
	if (err < 0) {
		fprintf(stderr, "failed to get address structure\n");
		exit(EXIT_FAILURE);
	}

	err = uv_tcp_bind(&server_stream, (const struct sockaddr *) &server_address, 0);
	if (err < 0) {
		fprintf(stderr, "failed to bind\n");
		exit(EXIT_FAILURE);
	}

	err = uv_listen((uv_stream_t *) &server_stream, BACKLOG, on_peer_connected);
	if (err < 0) {
		fprintf(stderr, "failed to listen\n");
		exit(EXIT_FAILURE);
	}

	uv_read_start((uv_stream_t *) &stdin_pipe, alloc_buffer, read_stdin);
	uv_run(loop, UV_RUN_DEFAULT);

	return uv_loop_close(loop);
}

static void on_peer_connected(uv_stream_t *server_stream, int status) {
	if (status < 0) {
		fprintf(stderr, "peer connection err: %s\n", uv_strerror(status));
		return;
	}
	int err;
	uv_tcp_t *peer = malloc(sizeof(uv_tcp_t));
	if (!peer) {
		fprintf(stderr, "uv_tcp_t peer oom\n");
		exit(EXIT_FAILURE);
	}
	err = uv_tcp_init(loop, peer);
	if (err) {
		fprintf(stderr, "uv_tcp_init err: %s\n", uv_strerror(err));
		exit(EXIT_FAILURE);
	}
	peer->data = NULL;

	err = uv_accept(server_stream, (uv_stream_t *) peer);
	if (err)
		uv_close((uv_handle_t *) peer, on_peer_closed);

	peer_state_t *peer_state = malloc(sizeof(peer_state_t));
	if (!peer_state) {
		fprintf(stderr, "oom\n");
		exit(EXIT_FAILURE);
	}
	memcpy(peer_state->sendbuf, welcome_message, strlen(welcome_message));
	peer_state->sendbuf_end = strlen(welcome_message);
	peer_state->peer = peer;
	uv_buf_t writebuf = uv_buf_init(peer_state->sendbuf, peer_state->sendbuf_end);
	uv_write_t *req = malloc(sizeof(uv_write_t));
	req->data = peer_state;
	err = uv_write(req, (uv_stream_t *) peer, &writebuf, 1, on_peer_write);
	if (err) {
		fprintf(stderr, "uv_write err: %s\n", uv_strerror(err));
		exit(EXIT_FAILURE);
	}
}

static void on_peer_write(uv_write_t *req, int status) {
	uv_close((uv_handle_t *) ((peer_state_t *) req->data)->peer, on_peer_closed);

}

static void on_peer_closed(uv_handle_t *handle) {
	uv_tcp_t *peer = (uv_tcp_t *) handle;
	if (peer->data)
		free(peer->data);
	free(peer);
}

static void alloc_buffer(uv_handle_t *handle, size_t ssize, uv_buf_t *b) {
	*b = uv_buf_init(malloc(ssize), ssize);
}

static void read_stdin(uv_stream_t *stream, ssize_t nread, const uv_buf_t *b) {
	if (nread < 0) {
		if (nread == UV_EOF) {
			uv_close((uv_handle_t *) &stdin_pipe, NULL);
			uv_close((uv_handle_t *) &stdout_pipe, NULL);
		}
	} else if (nread > 0) {
		write_data((uv_stream_t *) &stdout_pipe, nread, *b, on_stdout_write);
	}

	if (b->base)
		free(b->base);
}

static void write_data(uv_stream_t *d, size_t size, uv_buf_t b, uv_write_cb cb) {
	write_req_t *req = malloc(sizeof(write_req_t));
	req->buf = uv_buf_init(malloc(size), size);
	memcpy(req->buf.base, b.base, size);

	/* quit server on "quit" command */
	if (memcmp(req->buf.base, "quit\n", size) == 0)
		exit(0);

	uv_write((uv_write_t *) req, (uv_stream_t *) d, &req->buf, 1, cb);
}

static void on_stdout_write(uv_write_t *req, int status) {
	free_write_req(req);
}

static void free_write_req(uv_write_t *req) {
	write_req_t *wr = (write_req_t *) req;
	free(wr->buf.base);
	free(wr);
}

static void usage(char **arguments) {
	fprintf(stderr, "error, usage: %s port, where port is < 65536\n", arguments[0]);
}
