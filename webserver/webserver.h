#ifndef __WEBSERVER_H__
#define __WEBSERVER_H__

#include <sys/epoll.h>

#define ET 1
#define LT 0

class Server {
private:
	static const int MAX_EVENT_NUMBER	 = 1024;
	static const int RECEIVE_BUFFER_SIZE = 10;

public:
	/* init with listen port, backlog and work mode ET or LT */
	Server(int port, int backlog, int ET_LT);
	~Server();

	/* open listenfd */
	void event_listen();
	/* start servering */
	void event_loop();

private:
	void deal_event(int number);

private:
	int			_port;
	int			_listenfd;
	int			_sockfd;
	int			_backlog;
	int			_work_mode;
	int			_epollfd;
	epoll_event events[MAX_EVENT_NUMBER];
	char		receive_buffer[RECEIVE_BUFFER_SIZE];
};

#endif