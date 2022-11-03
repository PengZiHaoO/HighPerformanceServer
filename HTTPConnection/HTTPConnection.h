#ifndef __HTTP_CONNECTION_H__
#define __HTTP_CONNECTION_H__

#include <netinet/in.h>
#include <fcntl.h>

class HTTPConnection {
public:
	static const size_t MAX_READ_BUFFER_SIZE  = 2048;
	static const size_t MAX_WRITE_BUFFER_SIZE = 1024;
	static const size_t MAX_FILE_NAME_LENTH	  = 200;
	static char *		_doc_root;

	/* HTTP request method */
	enum METHOD {
		GET,
		POST,
		HEAD,
		PUT,
		DELETE,
		CONNECT,
		OPTIONS,
		TRACE,
		PATCH
	};

	/* HTTP request state */
	enum HTTP_CODE {
		NO_REQUEST,
		GET_REQUEST,
		BAD_REQUEST,
		NO_RESOURSE,
		FILE_REQUEST,
		FORBIDDEN_REQUEST,
		INTERNAL_ERROR,
		CLOSED_CONNECTION
	};

	/* HTTP check state */
	enum HTTP_CHECK_STATE {
		CHECK_STATE_REQUEST_LINE,
		CHECK_STATE_HEADER,
		CHECK_STATE_CONTENT
	};

	/* line state*/
	enum LINE_STATE {
		LINE_BAD,
		LINE_OK,
		LINE_OPEN
	};

public:
	/* constructor and deconstructor*/
	HTTPConnection();
	~HTTPConnection();

public:
	/* read request from _sockfd */
	bool read();
	/* write response to _sockfd */
	bool write();

	void process();

	void init(int sockfd, const sockaddr_in &addr);

	sockaddr_in *get_address();

private:
	void init();

	/* parse http request */
	HTTP_CODE process_request();
	/* add content to response */
	bool process_response(HTTP_CODE http_code);

	void close_connection();

	void unmap();

	/* return the start index of parsing content of current line */
	char *get_line();
	/* parse */
	LINE_STATE parse_line();
	HTTP_CODE  parse_request_line(char *text);
	HTTP_CODE  parse_headers(char *text);
	HTTP_CODE  parse_content(char *text);
	HTTP_CODE  do_request();

	/* add ressponse content */
	bool add_response(const char *format, ...);
	bool add_content(const char *content);
	bool add_state_line(int state, const char *title);
	bool add_headers(int content_length);
	bool add_content_type();
	bool add_content_length(int content_length);
	bool add_linger();
	bool add_blank_line();

public:
	static int _epollfd;
	/* record the number of client */
	static int _user_count;

private:
	/* client sockfd */
	int _sockfd;
	/* client socket address */
	struct sockaddr_in _address;

	/* buffer loop consisits request buffer*/
	char _read_buffer[MAX_READ_BUFFER_SIZE];
	/* index refer to the next index of the byte to read */
	int _read_index;
	/* the index of byte that has been already checked */
	int _have_checked_index;
	/* buffer loop consists response buffer */
	char _write_buffer[MAX_WRITE_BUFFER_SIZE];
	/* the index of byte that needs to be sended */
	int _write_index;
	/* io vector to store data to send */
	struct iovec _io_vector[2];
	/* file state */
	struct stat _file_state;
	/* request resourse file address */
	char *_file_address;
	/* record nums of io vector */
	int _iovec_count;
	/* fd trigger mode on epoll event */
	int _trig_mode;

	/* HTTP requset check state */
	int _check_state;

	/* HTTP requset content */
	char * _url;
	METHOD _method;
	char * _version;
	bool   _linger;
	int	   _content_length;
	char * _host;
	//request content
	//here just restor users' name and passwd
	char *_string;

	//request seourse file path
	char _request_file_path[MAX_FILE_NAME_LENTH];

	/* process read start line */
	int _start_line;
	/*turn on cgi or not*/
	bool _cgi;
};

#endif