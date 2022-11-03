#include <WebServer.h>
#include <HTTPConnection.h>

int main() {
	Server server(9006, 5, 4);

	server.event_listen();

	server.event_loop();

	return 0;
}