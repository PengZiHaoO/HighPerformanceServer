#include <webserver.h>

int main() {
	Server server(12345, 5);

	server.event_listen();

	server.event_loop();

	return 0;
}