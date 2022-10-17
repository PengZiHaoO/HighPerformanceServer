#ifndef __UTILITY_H__
#define __UTILITY_H__

/* set file none blocking */
int set_nonblocking(int fd);

/* add fd to epoll event */
void addfd_epollevent(int epolled, int fd, bool enable_ET = true);

#endif