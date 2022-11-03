#include "WebServer.h"
#include "utility.h"

#include <cstdio>
#include <cstring>
#include <netinet/in.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <exception>

Server::Server(int port, int backlog, int thread_num /*= 8 */) :
	_port(port), _backlog(backlog), _pool(8) {
	_users = new HTTPConnection[MAX_FD_NUMBER];

	char  server_path[300];
	char *ret = getcwd(server_path, 300);
	if (ret == nullptr) {
		//LOG_ERROR("Can not get cwd");
	}
	char root[9] = "/../root";
	_root		 = new char[strlen(server_path) + strlen(root) + 1];
	strcpy(_root, server_path);
	strcat(_root, root);

	HTTPConnection::_doc_root = _root;
}

Server::~Server() {
	close(_listenfd);
	delete[] _users;
	delete[] _root;
}

void Server::event_listen() {
	/* Create sockket */
	_listenfd = socket(PF_INET, SOCK_STREAM, 0);
	if (_listenfd < 0) {
		//LOG_ERROR("Create socket error!");
		printf("Create socket error!\n");
		throw std::exception();
	}

	/* Name the socket and listen */
	struct sockaddr_in address;
	bzero(&address, sizeof(address));
	address.sin_family		= AF_INET;
	address.sin_addr.s_addr = htonl(INADDR_ANY);
	address.sin_port		= htons(_port);
	/* name */
	int ret = bind(_listenfd, (struct sockaddr *)&address, sizeof(address));
	if (-1 == ret) {
		//LOG_ERROR("Naming the socket failed!errno is %d", errno);
		printf("Naming the socket failed!errno is %d \n", errno);
		throw std::exception();
	}
	/* listen */
	ret = listen(_listenfd, _backlog);
	if (-1 == ret) {
		//LOG_ERROR("Listening the socket failed! errno is %d", errno);
		printf("Listening the socket failed!\n");
		throw std::exception();
	}

	_epollfd = epoll_create(5);
	if (-1 == _epollfd) {
		//LOG_ERROR("epoll_create failed! errno is %d", errno);
		printf("epoll_create failed! errno is %d\n", errno);
		throw std::exception();
	}

	HTTPConnection::_epollfd = _epollfd;
	addfd_epollevent(_epollfd, _listenfd, ET, false);
}

void Server::event_loop() {
	while (true) {
		int number = epoll_wait(_epollfd, events, MAX_EVENT_NUMBER, -1);
		if (number < 0 && errno != EINTR) {
			//LOG_ERROR("%s", "epoll failure");
			break;
		}

		for (int i = 0; i < number; ++i) {
			int sockfd = events[i].data.fd;
			/* new connection */
			if (sockfd == _listenfd) {
				bool ret = deal_connection();
				printf("a new connection coming \n");
				if (!ret) {
					printf("deal connection failed \n");
					continue;
				}
			}
			/* something unexpected happened */
			else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
				printf("somthing unexpected happeded \n");
			}
			/* request */
			else if (events[i].events & EPOLLIN) {
				printf("request \n");
				read_data(sockfd);
			}
			/* response */
			else if (events[i].events & EPOLLOUT) {
				printf("write data \n");
				write_data(sockfd);
			}
		}
	}

	printf("exit \n");
}

bool Server::deal_connection() {
	printf("deal connection \n");
	struct sockaddr_in client_address;
	socklen_t		   client_address_length = sizeof(client_address);
	int				   connfd				 = accept(_listenfd, (struct sockaddr *)&client_address, &client_address_length);
	if (connfd < 0) {
		//LOG_ERROR("%s:errno is:%d", "accept error", errno);
		printf("accept failed \n");
		return false;
	}
	printf("------------\n");
	if (HTTPConnection::_user_count >= MAX_FD_NUMBER) {
		//LOG_ERROR("%s", "Internal server busy");
		printf("Internal server busy\n");
		return false;
	}
	_users[connfd].init(connfd, client_address);
	return true;
}

void Server::read_data(int sockfd) {
	if (_users[sockfd].read() == true) {
		printf("receive data from client\n");
		//LOG_INFO("receive data from client : %s", inet_ntoa(users[sockfd].get_address()->sin_addr));
		_pool.enqueue([&] { _users[sockfd].process(); });
	} else {
		printf("read data failed \n");
	}
}

void Server::write_data(int sockfd) {
	if (_users[sockfd].write() == true) {
		printf("send data to the client\n");
		//LOG_INFO("send data to the client(%s)", inet_ntoa(_users[sockfd].get_address()->sin_addr));
	} else {
		printf("write data failed \n");
	}
}
