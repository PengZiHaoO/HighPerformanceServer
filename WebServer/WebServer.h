#ifndef __WEBSERVER_H__
#define __WEBSERVER_H__

#include <sys/epoll.h>
#include <string>

#include "../ThreadPool/ThreadPool.h"
#include "HTTPConnection.h"

#define ET 1
#define LT 0

class Server {
private:
	static const int MAX_EVENT_NUMBER = 1024;
	static const int MAX_FD_NUMBER	  = 65536;

public:
	/* init with listen port */
	Server(int port, int backlog, int thread_num = 8);
	~Server();

	/* open listenfd */
	void event_listen();
	/* start servering */
	void event_loop();

private:
	bool deal_connection();

	void read_data(int sockfd);

	void write_data(int sockfd);

	void deal_event(int number);

private:
	//EPOLL
	int			_port;
	int			_listenfd;
	int			_sockfd;
	int			_backlog;
	int			_epollfd;
	epoll_event events[MAX_EVENT_NUMBER];

	//HTTPCONN
	HTTPConnection *_users;
	char *			_root;

	//threadpool
	ThreadPool _pool;
};

#endif