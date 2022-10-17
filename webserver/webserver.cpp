#include "webserver.h"
#include <cstdio>
#include <cstring>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/sendfile.h>
#include <fcntl.h>

Server::Server(int port, int backlog) :
	_port(port), _backlog(backlog) {
}

Server::~Server() {
	close(_sockfd);
}

void Server::event_listen() {
	/* Create sockket */
	_sockfd = socket(PF_INET, SOCK_STREAM, 0);
	if (_sockfd < 0) {
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
	int ret = bind(_sockfd, (struct sockaddr *)&address, sizeof(address));
	if (-1 == ret) {
		//LOG_ERROR("Naming the socket failed!");
		printf("Naming the socket failed!\n");
		throw std::exception();
	}
	/* listen */
	ret = listen(_sockfd, _backlog);
	sleep(5);
	if (-1 == ret) {
		//LOG_ERROR("Listening the socket failed!");
		printf("Listening the socket failed!\n");
		throw std::exception();
	}
}

void Server::event_loop() {
	struct sockaddr_in client;
	socklen_t		   client_address_length = sizeof(client);
	int				   connfd;
	int				   filefd = open("/root/projects/HighPerformanceServer/root/filetosend.txt", O_RDONLY);
	struct stat		   stat_buf;
	fstat(filefd, &stat_buf);
	while (1) {
		printf("waiting connection \n");
		connfd = accept(_sockfd, (sockaddr *)&client, &client_address_length);
		printf("connecting ... \n");
		if (connfd < 0) {
			printf("connecting failed..., errno is : %d\n", errno);
		} else {
			char remote[INET_ADDRSTRLEN];
			printf("connected with ip : %s and port %d \n", inet_ntop(AF_INET, &client.sin_addr, remote, INET_ADDRSTRLEN), ntohs(client.sin_port));
			/* do something instead of sleep */
			const char *common_data = "HELLO WORLD";
			int			ret			= sendfile(connfd, filefd, nullptr, stat_buf.st_size);

			if (ret < 0) {
				//LOG_ERROR("sending data failed!");
				printf("sending data failed");
			}
			close(connfd);
		}
	}
}