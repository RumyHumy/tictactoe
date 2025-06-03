#include "mongoose.h"

// V A R I A B L E S

struct mg_connection* pl1 = NULL; // player1
struct mg_connection* pl2 = NULL; // player2
#define BOARD_SIZE 11
char board[BOARD_SIZE] = "b000000000";
int turn = -1; // -1 - if game is stopped
int w_index[8][3] = {
	{ 1, 2, 3 }, { 4, 5, 6 }, { 7, 8, 9 }, // Horizontal
	{ 1, 4, 7 }, { 2, 5, 8 }, { 3, 6, 9 }, // Vertical
	{ 1, 5, 9 }, { 3, 5, 7 } // Diagonal
};

// H A N D L E R S

// Messages from server:
// Statusbar change:
// 's' - game started
// 'w' - waiting for players
// 'd' - player disconnected
// Logic:
// 'c' - clear board 
// 'x'/'o' - who's turn
// 'X'/'O' - somebody won
// 'b100020000' - board update

void send_all(struct mg_mgr *mgr, char* buf, size_t len) {
	printf("Sending all: '%.*s'\n", len, buf);
	struct mg_connection *c;
	for (c = mgr->conns; c != NULL; c = c->next) {
		if (c->is_websocket) {
			mg_ws_send(c, buf, len, WEBSOCKET_OP_TEXT);
		}
	}
}

void ev_handle_http(struct mg_connection* c, int ev, struct mg_http_message* hm) {
	if (mg_strcmp(hm->uri, mg_str("/ws")) == 0) {
		mg_ws_upgrade(c, hm, NULL);
		// Players connecting
		if (pl1 == NULL) {
			pl1 = c;
			printf("WS: Player 1 connected\n");
			send_all(c->mgr, "c", 1);
		} else if (pl2 == NULL) {
			pl2 = c;
			printf("WS: Player 2 connected\n");
			send_all(c->mgr, "c", 1);
			if (pl1 && pl2) {
				printf("WS: Game started\n");
				send_all(c->mgr, "s", 1);
				send_all(c->mgr, "x", 1);
				send_all(c->mgr, board, BOARD_SIZE);
				turn = 0;
			}
		}
		// For spectators
		if (turn != -1) {
			send_all(c->mgr, board, BOARD_SIZE);
			send_all(c->mgr, (turn%2 ? "o" : "x"), 1);
		}
		return;
	}
	struct mg_http_serve_opts opts = { .root_dir = "./web" };
	mg_http_serve_dir(c, hm, &opts);
}

// Indexes begin from 1, 0 - error
int get_board_index(char ch) {
	return (ch >= '0' && ch <= '8') ? ch-'0'+1 : 0;
}

// -1, 0, 1, 2 - error, good, Xs won, Os won
char board_put(int i, char p) {
	if (turn%2 != p-1)
		return -1;
	if (board[i] != '0')
		return -1;
	board[i] = p+'0';
	for (int j = 0; j < 8; j++) {
		char flag = 1;
		printf("board '%c', ", board[w_index[j][0]]);
		printf("p '%d'\n", p);
		flag &= board[w_index[j][0]]-'0' == p;
		flag &= board[w_index[j][1]]-'0' == p;
		flag &= board[w_index[j][2]]-'0' == p;
		if (flag)
			return p;
	}
	turn++;
	return 0;
}

void board_clear() {
	for (int i = 1; i < 10; i++)
		board[i] = '0';
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
			if (wm->data.len == 0)
				return;
			if (c != pl1 && c != pl2)
				return;
			char i = get_board_index(wm->data.buf[0]);
			char p = (c == pl1 ? 1 : 2);
			char result = board_put(i, p);
			if (result == -1) {
				printf("Illegal move from player %d\n", p);
				return;
			}
			send_all(c->mgr, (turn%2 ? "o" : "x"), 1);
			send_all(c->mgr, board, BOARD_SIZE);
			if (result == p) {
				send_all(c->mgr, (p == 1 ? "X" : "O"), 1);
				board_clear();
				turn = -1;
				return;
			}
			break;
		case MG_EV_CLOSE:
			if (c == pl1) {
				pl1 = NULL;
				printf("WS: Player 1 disconnected\n");
				board_clear();
				send_all(c->mgr, "c", 1);
				send_all(c->mgr, "d", 1);
				turn = -1;
			}
			if (c == pl2) {
				pl2 = NULL;
				printf("WS: Player 2 disconnected\n");
				board_clear();
				send_all(c->mgr, "c", 1);
				send_all(c->mgr, "d", 1);
				turn = -1;
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
