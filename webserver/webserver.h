#ifndef __WEBSERVER_H__
#define __WEBSERVER_H__

#include <sys/socket.h>
#include <cassert>
#include <netinet/in.h>
#include <exception>

class Server {
public:
	Server(int port, int backlog);
	~Server();

	void event_listen();
	void event_loop();

private:
	int _port;
	int _listenfd;
	int _sockfd;
	int _backlog;
};

#endif