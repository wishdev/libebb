/*
 * plain simple TCP director, to demonstrate libebb
 * as an high-level IO library/
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include <ev.h>
#include "ebb.h"

#define zalloc(s) { s = malloc(sizeof(*s)); memset(s, 0, sizeof(*s)); }

/* finished writing; free the buffer */
static	void write_done(ebb_connection *c)
{
	/* free this buffer */
	free((void*)c->to_write);
	/* and request another read */
	ebb_connection_read(c->data);
}

/* got data for the other peer. */
int	on_data(ebb_connection *c, char *buf, int count)
{
	char *wbuf;

	wbuf = malloc(count);
	memcpy(wbuf, buf, count);
	assert(ebb_connection_write(c->data, wbuf, count, write_done));

	/* will re-issue after write is done */
	return EBB_STOP;
}

/* will release connection and its associated peer */
void	on_close(ebb_connection *c)
{
	if (c->data) {
		ebb_connection *peer = c->data;
		/* clear the reference to us */
		peer->data = NULL;
		assert(peer->loop);
		ebb_connection_schedule_close(c->data);
	}
	/* kill write buffer */
	if (c->to_write)
		free((void*)c->to_write);
	free(c);
}

/* process new connection to the server */
ebb_connection *new_connection(ebb_server *server, struct sockaddr_in *addr)
{
	ebb_connection *conn, *client;
	const char **argv = server->data;

	/* alloc structures */
	zalloc(conn);
	zalloc(client);

	/* init this */
	ebb_connection_init(conn);
	conn->data = client;
	conn->on_data = on_data;
	conn->on_close = on_close;

	/* and associate with new client connection */
	ebb_connection_init(client);
	client->data = conn;
	client->on_data = on_data;
	client->on_close = on_close;
	client->loop = server->loop;
	assert(server->loop);
	ebb_tcp_client(client, argv[0], atol(argv[1]));

	return conn;
}

int	main(int argc, char *argv[])
{
	struct ev_loop *loop = ev_default_loop(0);
	static ebb_server server;

	assert(argc == 4 && "Expected 3 arguments: listenport fwdhost fwdport");

	ebb_server_init(&server, loop);
	server.new_connection = new_connection;
	server.data = argv+2;
	ebb_server_set_secure(&server, "ca-cert.pem", "ca-key.pem");
	ebb_tcp_server(&server, NULL, atol(argv[1]));

	ev_loop(loop, 0);
	return 0;
}

