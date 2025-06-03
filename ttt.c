#include "mongoose.h"

// V A R I A B L E S

struct mg_connection* pl1 = NULL; // player1
struct mg_connection* pl2 = NULL; // player2

// H A N D L E R S

// Messages from server:

// 's' - game started
// 'x'/'o' - who's turn

void ev_handle_http(struct mg_connection* c, int ev, struct mg_http_message* hm) {
	if (mg_strcmp(hm->uri, mg_str("/ws")) == 0) {
		mg_ws_upgrade(c, hm, NULL);
		if (pl1 == NULL) {
			pl1 = c;
			printf("WS: Player 1 connected\n");
		} else if (pl2 == NULL) {
			pl2 = c;
			printf("WS: Player 2 connected\n");
			if (pl1 && pl2) {
				printf("WS: Game started\n");
				mg_ws_send(pl1, "s", 1, WEBSOCKET_OP_TEXT);
				mg_ws_send(pl2, "s", 1, WEBSOCKET_OP_TEXT);
				mg_ws_send(pl1, "x", 1, WEBSOCKET_OP_TEXT);
				mg_ws_send(pl2, "x", 1, WEBSOCKET_OP_TEXT);
			}
		}
		return;
	}
	struct mg_http_serve_opts opts = { .root_dir = "./web" };
	mg_http_serve_dir(c, hm, &opts);
}

char poll_check_conn(struct mg_connection* c) {
	if (c == NULL)
		return 0;
	if (!c->is_websocket)
		return 0;
	return !c->is_closing && !c->is_draining;
}

void ev_handler(struct mg_connection* c, int ev, void* ev_data) {
	switch (ev) {
		case MG_EV_HTTP_MSG:
			struct mg_http_message* hm = (struct mg_http_message*)ev_data;
			ev_handle_http(c, ev, hm);
			break;
		case MG_EV_WS_MSG:
			struct mg_ws_message* wm = (struct mg_ws_message*)ev_data;
			printf("WS: '%.*s'\n", (int)wm->data.len, wm->data.buf);
			mg_ws_send(c, wm->data.buf, wm->data.len, WEBSOCKET_OP_TEXT);
			break;
		case MG_EV_POLL:
			if (pl1 && !poll_check_conn(pl1)) {
				pl1 = NULL;
				printf("WS: Player 1 disconnected\n");
				// TODO: Clear board
			}
			if (pl2 && !poll_check_conn(pl2)) {
				pl2 = NULL;
				printf("WS: Player 2 disconnected\n");
				// TODO: Clear board
			}
			if (pl2 && !pl1) {
				pl1 = pl2;
				printf("WS: pl2 -> pl1\n");
			}
			break;
	}
}

// M A I N

int main(void) {
	struct mg_mgr mgr; // Declare event manager
	mg_mgr_init(&mgr); // Initialise event manager
	mg_http_listen(&mgr, "http://0.0.0.0:6969", ev_handler, NULL); // Setup listener
	for (;;) { // Run an infinite event loop
		mg_mgr_poll(&mgr, 1000);
	}
	return 0;
}
