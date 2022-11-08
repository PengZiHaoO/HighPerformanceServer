#ifndef __DB_CONNECTION_POOL_H__
#define __DB_CONNECTION_POOL_H__

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <queue>

#include "DBConnection.h"

class DBConnectionPool {
public:
	DBConnectionPool &			  get_connection_pool();
	std::shared_ptr<DBConnection> get_connection();

	bool loadConfigFile();

private:
	DBConnectionPool();
	~DBConnectionPool();

	void produce_connection();
	void reclaim_connection();

private:
	//mysql server init info
	std::string _ip;
	uint16_t	_port;
	std::string _user;
	std::string _passwd;
	std::string _dbname;

	//connection pool description
	int _init_size;
	int _max_size;
	int _max_free_time;
	int _connection_timeout;

	//the queue to store mysql connection
	std::queue<DBConnection *> _connection_queue;
	std::mutex				   _queue_mutex;
	std::atomic_int			   _connection_count;

	std::condition_variable _cv;
	std::condition_variable _cv_close;

	bool _pool_alive;
	int	 _use_count;
	bool _is_closing;
};

#endif