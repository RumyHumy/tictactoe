#include "mongoose.h"

// H A N D L E R S

void ev_handle_http(struct mg_connection* c, int ev, struct mg_http_message* hm) {
	//if (mg_strcmp(hm->uri, mg_str("/add")) == 0) {
	//	return;
	//}
	struct mg_http_serve_opts opts = { .root_dir = "./web" };
	mg_http_serve_dir(c, hm, &opts);
}

void ev_handler(struct mg_connection* c, int ev, void* ev_data) {
	if (ev == MG_EV_HTTP_MSG) {
		struct mg_http_message* hm = (struct mg_http_message*)ev_data;
		ev_handle_http(c, ev, hm);
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
