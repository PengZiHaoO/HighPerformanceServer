#include "webserver.h"
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

Server::Server(int port, int backlog, int ET_LT) :
	_port(port), _backlog(backlog), _work_mode(ET_LT) {
}

Server::~Server() {
	close(_listenfd);
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
		printf("epoll_create failed! errno is %d \n", errno);
		throw std::exception();
	}

	addfd_epollevent(_epollfd, _listenfd, _work_mode);
}

void Server::event_loop() {
	int			filefd = open("/root/projects/HighPerformanceServer/root/filetosend.txt", O_RDONLY);
	struct stat stat_buf;

	fstat(filefd, &stat_buf);

	while (1) {
		printf("waiting connection \n");

		int ret = epoll_wait(_epollfd, events, MAX_EVENT_NUMBER, -1);

		if (ret < 0) {
			//LOG_WARING("epoll failure and errno is %d", errno);
			printf("epoll failure and errno is %d", errno);
			throw std::exception();
		}

		deal_event(ret);
	}

	close(_listenfd);
}

void Server::deal_event(int number) {
	for (int i = 0; i < number; ++i) {
		int sockfd = events[i].data.fd;

		/* event happen */
		if (sockfd == _listenfd) {
			struct sockaddr_in client_address;
			socklen_t		   client_address_length = sizeof(client_address);

			int connfd = accept(_listenfd, (struct sockaddr *)&client_address, &client_address_length);

			if (-1 == connfd) {
				//LOG_WARING("accepting a connection failed and errno is %d", errno);
				printf("accepting a connection failed and errno is %d", errno);
				close(connfd);
				continue;
			}

			addfd_epollevent(_epollfd, connfd, _work_mode);
		}
		/* receive data */
		else if (events[i].events & EPOLLIN) {
			//LOG_INFO("event trigger once");
			printf("event trigger once \n");

			if (_work_mode) {
				for (;;) {
					memset(receive_buffer, '\0', RECEIVE_BUFFER_SIZE);

					int ret = recv(sockfd, receive_buffer, RECEIVE_BUFFER_SIZE - 1, 0);

					if (ret < 0) {
						printf("sending data failed");

						if ((errno = EAGAIN) || (errno == EWOULDBLOCK)) {
							//LOG_INFO("read later");
							break;
						}
					} else if (0 == ret) {
						//LOG_ERROR("sending data failed and errno is %d", errno);
						close(sockfd);
					} else {
						//LOG_INFO("get %d bytes of content", ret);
						printf("get %d bytes of content : %s \n", ret, receive_buffer);
					}
				}

			} else {
				memset(receive_buffer, '\0', RECEIVE_BUFFER_SIZE);

				int ret = recv(sockfd, receive_buffer, RECEIVE_BUFFER_SIZE - 1, 0);

				if (ret <= 0) {
					close(sockfd);
					continue;
				}
				//LOG_INFO("get %d bytes of content", ret);
				printf("get %d bytes of content : %s \n", ret, receive_buffer);
			}
		}
		/* something unexpected happen */
		else {
			//LOG_WARING("something unexpected happened");
			printf("something unexpected happened\n");
		}
	}
}
