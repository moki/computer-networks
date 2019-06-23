struct server_attr {
	int   transport_proto;
	int   internet_proto;
	int   backlog;
	char *port;
};

typedef struct server_attr server_attr_t;

extern int server_attr_setport(server_attr_t *server_attr, char *port);

extern int server_attr_settransportproto(server_attr_t *server_attr, int protocol);

extern int server_attr_setinternetproto(server_attr_t *server_attr, int protocol);

extern int server_attr_init(server_attr_t *server_attr);

extern int server_listen(server_attr_t *server_attr);
