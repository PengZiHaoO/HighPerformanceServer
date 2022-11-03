#ifndef __UTILITY_H__
#define __UTILITY_H__

/* set file none blocking */
int set_nonblocking(int fd);

/* add fd to epoll event */
void addfd_epollevent(int epolled, int fd, bool enable_ET = true, bool enable_oneshot = true);

/* remove fd from epollfd */
void removfd(int epollfd, int fd);

/* modiify fd on epollfd */
void modfd(int epollfd, int fd, int ev);

#endif