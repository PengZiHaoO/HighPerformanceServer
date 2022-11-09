#include "HTTPConnection.h"
#include <cerrno>
#include <cstdarg>
#include <cstring>
#include <sys/epoll.h>
#include <string>
#include <sys/uio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "utility.h"
#include "DBConnectionPool.h"

/*HTTP response state info*/
const char *ok_200_title	= "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form	= "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form	= "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form	= "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form	= "There was an unusual problem serving the request file.\n";

HTTPConnection::HTTPConnection() {
}

HTTPConnection::~HTTPConnection() {
}

int	  HTTPConnection::_epollfd	  = -1;
int	  HTTPConnection::_user_count = 0;
char *HTTPConnection::_doc_root	  = nullptr;

void HTTPConnection::process() {
	HTTP_CODE read_ret = process_request();
	if (read_ret == NO_REQUEST) {
		modfd(_epollfd, _sockfd, EPOLLIN);
		return;
	}
	printf("process request code %d \n", read_ret);
	bool write_ret = process_response(read_ret);
	if (!write_ret) {
		printf("close connection \n");
		close_connection();
	}
	modfd(_epollfd, _sockfd, EPOLLOUT);
}

/* nonblock read utill read all data */
bool HTTPConnection::read() {
	if (_read_index > MAX_READ_BUFFER_SIZE) {
		return false;
	}

	int bytes_read = 0;
	while (true) {
		bytes_read = recv(_sockfd, _read_buffer + _read_index, MAX_READ_BUFFER_SIZE, 0);

		if (bytes_read == -1) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				break;
			}
			return false;
		} else if (bytes_read == 0) {
			return true;
		}

		_read_index += bytes_read;
	}

	return true;
}

/*nonblock send utill send all data*/
bool HTTPConnection::write() {
	if (_write_index == 0) {
		modfd(_epollfd, _sockfd, EPOLLIN);
		init();
		return true;
	}
	//TODO : write vector to send file resourse
	int bytes_have_send = 0;
	int bytes_to_send	= _write_index;
	int bytes_write		= 0;
	while (true) {
		bytes_write = writev(_sockfd, _io_vector, _iovec_count);

		if (bytes_write == -1) {
			if (errno == EAGAIN) {
				modfd(_epollfd, _sockfd, EPOLLOUT);
				return true;
			}
			printf("write false \n");
			return false;
		}

		bytes_have_send += bytes_write;
		bytes_to_send -= bytes_write;

		if (bytes_have_send >= _io_vector[0].iov_len) {
			_io_vector[0].iov_len  = 0;
			_io_vector[0].iov_base = _file_address + (bytes_have_send - _write_index);
			_io_vector[1].iov_len  = bytes_to_send;
		} else {
			_io_vector[0].iov_base = _write_buffer + bytes_have_send;
			_io_vector[0].iov_len  = _io_vector[0].iov_len - bytes_have_send;
		}

		if (bytes_to_send <= 0) {
			unmap();
			modfd(_epollfd, _sockfd, EPOLLIN);

			if (_linger) {
				init();
				return true;
			} else {
				return false;
			}
		}
	}

	return true;
}

sockaddr_in *HTTPConnection::get_address() {
	return &_address;
}

void HTTPConnection::init(int sockfd, const sockaddr_in &addr) {
	_sockfd	 = sockfd;
	_address = addr;

	addfd_epollevent(_epollfd, _sockfd);
	++_user_count;

	init();
}

void HTTPConnection::init() {
	_check_state		= CHECK_STATE_REQUEST_LINE;
	_linger				= false;
	_method				= GET;
	_url				= nullptr;
	_version			= nullptr;
	_content_length		= 0;
	_host				= nullptr;
	_start_line			= 0;
	_have_checked_index = 0;
	_read_index			= 0;
	_cgi				= false;

	memset(_read_buffer, '\0', MAX_READ_BUFFER_SIZE);
	memset(_write_buffer, '\0', MAX_WRITE_BUFFER_SIZE);
	memset(_request_file_path, '\0', MAX_FILE_NAME_LENTH);
}

void HTTPConnection::close_connection() {
	if (_sockfd != -1) {
		//LOG_INFO("close %d", _sockfd);
		removfd(_epollfd, _sockfd);
		_sockfd = -1;
		--_user_count;
	}
}

/* parse request */
/* TODO: optimize*/
HTTPConnection::HTTP_CODE HTTPConnection::process_request() {
	LINE_STATE line_state = LINE_OK;
	HTTP_CODE  ret_code	  = NO_REQUEST;

	while (((line_state = parse_line()) == LINE_OK) || (_check_state == CHECK_STATE_CONTENT && line_state == LINE_OK)) {
		char *text	= get_line();
		_start_line = _have_checked_index;
		//LOG_INFO("%s", text);
		switch (_check_state) {
			case CHECK_STATE_REQUEST_LINE: {
				ret_code = parse_request_line(text);

				if (ret_code == BAD_REQUEST) {
					printf("return BAD_REQUEST \n");
					return BAD_REQUEST;
				}

				break;
			}
			case CHECK_STATE_HEADER: {
				ret_code = parse_headers(text);

				if (ret_code == BAD_REQUEST) {
					printf(" 2 return BAD_REQUEST \n");
					return BAD_REQUEST;
				}
				/*
					GET method without body 
					then do request
				*/
				else if (ret_code == GET_REQUEST) {
					HTTP_CODE ret = do_request();
					printf("return %d \n", ret);
					return ret;
				}

				break;
			}
			case CHECK_STATE_CONTENT: {
				ret_code = parse_content(text);

				if (ret_code == GET_REQUEST) {
					return do_request();
				} else if (ret_code == NO_REQUEST) {
					line_state = LINE_OPEN;
					return NO_REQUEST;
				}

				break;
			}
			default: {
				return INTERNAL_ERROR;
			}
		}
	}
	return NO_REQUEST;
}

char *HTTPConnection::get_line() {
	return _read_buffer + _start_line;
};

/* 
parse one line ;
when missing '\r''\n' means one line ended and convert them to '\0''\0'
*/
HTTPConnection::LINE_STATE HTTPConnection::parse_line() {
	for (; _have_checked_index < _read_index; ++_have_checked_index) {
		char temp = _read_buffer[_have_checked_index];

		if (temp == '\r') {
			if (_have_checked_index + 1 == _read_index) {
				return LINE_OPEN;
			} else if (_read_buffer[_have_checked_index + 1] == '\n') {
				_read_buffer[_have_checked_index++] = '\0';
				_read_buffer[_have_checked_index++] = '\0';

				return LINE_OK;
			}

			return LINE_BAD;
		} else if (temp == '\n') {
			if (_have_checked_index > 1 && _write_buffer[_have_checked_index - 1] == '\r') {
				_read_buffer[_have_checked_index - 1] = '\0';
				_read_buffer[_have_checked_index++]	  = '\0';

				return LINE_OK;
			}

			return LINE_BAD;
		}
	}

	return LINE_OPEN;
}

/*Currently, only support GET & POST */
/* TODO : add option method */
/*
	HTTP ewquest line 
	"HTTP_method url HTTP_version\r\n"
*/
HTTPConnection::HTTP_CODE HTTPConnection::parse_request_line(char *text) {
	_url = strpbrk(text, " \t");

	if (_url == nullptr) {
		return BAD_REQUEST;
	}

	char *method = text;

	//TODO : add cgi
	if (strcasecmp(method, "GET") == 0) {
		_method = GET;
	} else if (strcasecmp(method, "POST") == 0) {
		_method = POST;
		_cgi	= true;
	} else if (strcasecmp(method, "OPTIONS")) {
		_method = OPTIONS;
	} else {
		return BAD_REQUEST;
	}

	_url += strspn(_url, " \t");
	_version = strpbrk(_url, " \t");

	if (!_version) {
		return BAD_REQUEST;
	}

	*_version++ = '\0';
	_version += strspn(_version, " \t");

	if (strcasecmp(_version, "HTTP/1.1") != 0) {
		return BAD_REQUEST;
	}

	if (strncasecmp(_url, "http://", 7) == 0) {
		_url += 7;
		_url = strchr(_url, '/');
	}

	if (!_url || _url[0] != '/') {
		return BAD_REQUEST;
	}

	//url == '\' return something
	//append resourse to _url
	if (strlen(_url) == 1) {
		strcat(_url, "judge.html");
	}

	_check_state = CHECK_STATE_HEADER;

	return NO_REQUEST;
}

/*
parse HTTP header 
return GET_REQUEST until parse the blank line 	
Now only support Conection , Content_length and Host field
*/
HTTPConnection::HTTP_CODE HTTPConnection::parse_headers(char *text) {
	if (text[0] == '\0') {
		if (_content_length != 0) {
			_check_state = CHECK_STATE_CONTENT;

			return NO_REQUEST;
		}

		return GET_REQUEST;
	} else if (strncasecmp(text, "Connection:", 11) == 0) {
		text += 11;
		text += strspn(text, " \t");

		if (strcasecmp(text, "keep-alive") == 0) {
			_linger = true;
		}
	} else if (strncasecmp(text, "Content-length:", 15) == 0) {
		text += 15;
		text += strspn(text, " \t");
		_content_length = atol(text);
	} else if (strncasecmp(text, "Host:", 5) == 0) {
		text += 5;
		text += strspn(text, " \t");

		_host = text;
	} else {
		//LOG_INFO("oop! unknow header: %s", text);
	}

	return NO_REQUEST;
}

/*
Only POST method need this method and the content only includes user's name and password
*/
HTTPConnection::HTTP_CODE HTTPConnection::parse_content(char *text) {
	//make sure content that content really has content
	if (_read_index >= (_content_length + _have_checked_index)) {
		text[_content_length] = '\0';
		_string				  = text;

		return GET_REQUEST;
	}
	return NO_REQUEST;
}

/*

*/
HTTPConnection::HTTP_CODE HTTPConnection::do_request() {
	strcpy(_request_file_path, _doc_root);
	int length = strlen(_doc_root);

	//if want more level of dir, you can easy expend here
	char *option = strrchr(_url, '/');
	option += 1;
	//TODO:add db
	bool log_in	 = (strncasecmp(option, "log", 5) == 0);
	bool sign_up = (strncasecmp(option, "sign", 6) == 0);

	if (_cgi == true && (log_in || sign_up)) {
		//TODO: _string中提取出username & passwd and return the page
		char *send_resourse = new char[200];

		strcpy(send_resourse, "/");
		strcat(_request_file_path, _url + 2);
		strncpy(_request_file_path + length, send_resourse, MAX_FILE_NAME_LENTH - length - 1);

		delete[] send_resourse;

		std::string name;
		std::string passwd;
		for (int i = 0; _string[i] != '&'; ++i) {
			name += _string[i];
		}
		for (int i = name.size(); _string[i] != '\0'; ++i) {
			passwd += _string[i];
		}

		/*登录*/
		if (log_in) {
			//TODO:判断username & passwd是否正确并返回相应页面

		}
		/*注册*/
		else if (sign_up) {
			//TODO:判断username是否重复并并插入数据库
		}
	}

	if (strncasecmp(option, "picture", 8) == 0) {
		char *send_resourse = new char[200];

		strcpy(send_resourse, "/picture.html");
		strncpy(_request_file_path + length, send_resourse, strlen(send_resourse));

		delete[] send_resourse;
	} else if (strncasecmp(option, "video", 5) == 0) {
		char *send_resourse = new char[200];

		strcpy(send_resourse, "/video.html");
		strncpy(_request_file_path + length, send_resourse, strlen(send_resourse));

		delete[] send_resourse;
	} else if (strncasecmp(option, "subscribe", 9) == 0) {
		char *send_resourse = new char[200];

		strcpy(send_resourse, "/subscribe.html");
		strncpy(_request_file_path + length, send_resourse, strlen(send_resourse));

		delete[] send_resourse;
	} else {
		strncpy(_request_file_path + length, _url, MAX_FILE_NAME_LENTH - length - 1);
	}
	printf("request file path %s \n", _request_file_path);
	if (stat(_request_file_path, &_file_state) < 0) {
		printf("-------------------no resourse----------------\n");
		return NO_RESOURSE;
	}

	if (!(_file_state.st_mode & S_IROTH)) {
		return FORBIDDEN_REQUEST;
	}

	if (S_ISDIR(_file_state.st_mode)) {
		return BAD_REQUEST;
	}

	int fd = open(_request_file_path, O_RDONLY);
	if (fd < 0) {
		printf("opening file failed \n");
	}
	printf("file size %ld \n", _file_state.st_size);
	_file_address = (char *)mmap(0, _file_state.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);

	return FILE_REQUEST;
}

/*
process response buffer

	 state line		HTTP_version state_code state_code_description\r\n
response header		header_field:value\r\n
					\r\n
  response body		content
*/
bool HTTPConnection::process_response(HTTP_CODE http_code) {
	switch (http_code) {
		case INTERNAL_ERROR: {
			add_state_line(500, error_404_title);
			add_headers(strlen(error_500_form));
			if (!add_content(error_500_form)) {
				return false;
			}
			break;
		}
		case BAD_REQUEST: {
			add_state_line(404, error_400_title);
			add_headers(strlen(error_400_form));
			if (!add_content(error_400_form)) {
				return false;
			}
			break;
		}
		case FORBIDDEN_REQUEST: {
			add_state_line(403, error_403_title);
			add_headers(strlen(error_403_form));
			if (!add_content(error_404_form)) {
				return false;
			}
			break;
		}
		case FILE_REQUEST: {
			add_state_line(200, ok_200_title);

			if (_file_state.st_size != 0) {
				add_headers(_file_state.st_size);
				_io_vector[0].iov_base = _write_buffer;
				_io_vector[0].iov_len  = _write_index;
				_io_vector[1].iov_base = _file_address;
				_io_vector[1].iov_len  = _file_state.st_size;
				_iovec_count		   = 2;

				return true;
			} else {
				const char *okstring = "<html><body></body></html>";
				add_headers(strlen(okstring));
				if (!(add_content(okstring))) {
					return false;
				}
			}
		}
		default: {
			return false;
		}
	}

	_io_vector[0].iov_base = _write_buffer;
	_io_vector[0].iov_len  = _write_index;

	_iovec_count = 1;

	return true;
}

/*
offer basic function
*/
bool HTTPConnection::add_response(const char *format, ...) {
	if (_write_index >= MAX_WRITE_BUFFER_SIZE) {
		return false;
	}

	va_list arg_list;
	va_start(arg_list, format);

	int length = vsnprintf(_write_buffer + _write_index, MAX_WRITE_BUFFER_SIZE - _write_index - 1, format, arg_list);

	if (length >= (MAX_WRITE_BUFFER_SIZE - _write_index - 1) || length < 0) {
		va_end(arg_list);

		return false;
	}

	_write_index += length;
	va_end(arg_list);

	return true;
}

bool HTTPConnection::add_content(const char *content) {
	return add_response("%s", content);
}

bool HTTPConnection::add_state_line(int state, const char *title) {
	return add_response("%s %d %s\r\n", "HTTP/1.1", state, title);
}

bool HTTPConnection::add_headers(int content_length) {
	return add_content_length(content_length) && add_content_type() && add_linger() && add_blank_line();
}

bool HTTPConnection::add_content_type() {
	return add_response("Content-Type:%s\r\n", "text/html");
}

bool HTTPConnection::add_content_length(int content_length) {
	return add_response("Content-length:%d\r\n", content_length);
}

bool HTTPConnection::add_linger() {
	return add_response("Connection:%s\r\n", (_linger == true) ? "keep-alive" : "close");
}

bool HTTPConnection::add_blank_line() {
	return add_response("%s", "\r\n");
}

void HTTPConnection::unmap() {
	if (_file_address != nullptr) {
		munmap(_file_address, _file_state.st_size);
	}
}
