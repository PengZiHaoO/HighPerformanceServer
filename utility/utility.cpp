#include "utime.h"
#include <fcntl.h>
#include <sys/epoll.h>
#include <cstdio>
#include <errno.h>

int set_nonblocking(int fd) {
	int old_option = fcntl(fd, F_GETFL);

	int new_option = old_option | O_NONBLOCK;

	if (-1 == fcntl(fd, F_SETFL, new_option)) {
		//LOG_WARING("setting none blocking failed and errno is %d ", errno);
		printf("setting none blocking failed and errno is %d \n", errno);
	}

	return old_option;
}

void addfd_epollevent(int epollfd, int fd, bool enable_ET /* = true */) {
	epoll_event event;

	event.data.fd = fd;
	event.events  = EPOLLIN;

	if (enable_ET) {
		event.events = event.events | EPOLLET;
	}

	if (-1 == epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event)) {
		//LOG_WARING("epoll_ctl invoction failedand errno is %d ", errno);
		printf("epoll_ctl invoction failed and errno is %d \n", errno);
	}
}