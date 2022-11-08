#include "DBConnectionPool.h"
#include <functional>
#include <thread>

DBConnectionPool &DBConnectionPool::get_connection_pool() {
	static DBConnectionPool _pool;

	return _pool;
}

std::shared_ptr<DBConnection> DBConnectionPool::get_connection() {
	std::unique_lock<std::mutex> ulock(_queue_mutex);

	while (_connection_queue.empty()) {
		if (std::cv_status::timeout == _cv.wait_for(ulock, std::chrono::microseconds(_connection_timeout))) {
			if (_connection_queue.empty()) {
				//LOG_ERROR("get connection filed !");
				return nullptr;
			}
		}
	}

	std::shared_ptr<DBConnection> sptr(_connection_queue.front(),
									   [&](DBConnection *pconn) {
										   std::unique_lock<std::mutex> ulock(_queue_mutex);
										   pconn->refresh();
										   _connection_queue.push(pconn);
										   --_use_count;

										   if (_is_closing && _use_count) {
											   _cv_close.notify_all();
										   }
									   });

	_connection_queue.pop();
	++_use_count;
	_cv.notify_all();

	return sptr;
}

bool DBConnectionPool::loadConfigFile() {
	FILE *pf = fopen("/root/connectionPool/connectionPool/src/mysql.cnf", "r");

	if (pf == nullptr) {
		//LOG_ERROR("mysql.cnf file is not exist!");
		return false;
	}

	while (!feof(pf)) {
		char line[1024] = {0};
		fgets(line, 1024, pf);
		std::string str = line;

		int idx = str.find('=', 0);
		if (idx == -1) { // 不是配置项
			continue;
		}
		int			endidx = str.find('\n', idx);
		std::string key	   = str.substr(0, idx);
		std::string value  = str.substr(idx + 1, endidx - idx - 1);

		if (key == "ip") {
			_ip = value;
		} else if (key == "port") {
			_port = atoi(value.c_str());
		} else if (key == "username") {
			_user = value;
		} else if (key == "password") {
			_passwd = value;
		} else if (key == "dbname") {
			_dbname = value;
		} else if (key == "initSize") {
			_init_size = atoi(value.c_str());
		} else if (key == "maxSize") {
			_max_size = atoi(value.c_str());
		} else if (key == "maxIdleTime") {
			_max_free_time = atoi(value.c_str());
		} else if (key == "connectionTimeout") {
			_connection_timeout = atoi(value.c_str());
		}
	}
	fclose(pf);

	return true;
}

DBConnectionPool::DBConnectionPool() :
	_pool_alive(true) {
	if (loadConfigFile() == false) {
		return;
	}
	for (int i = 0; i < _init_size; ++i) {
		DBConnection *p = new DBConnection;
		p->connect(_ip, _port, _user, _passwd, _dbname);
		p->refresh();
		_connection_queue.push(p);
		++_use_count;
	}

	std::thread produce(std::bind(&DBConnectionPool::produce_connection, this));
	produce.detach();

	std::thread scanner(std::bind(&DBConnectionPool::reclaim_connection, this));
	scanner.detach();
}

DBConnectionPool::~DBConnectionPool() {
	{
		std::unique_lock<std::mutex> lock(_queue_mutex);
		_is_closing = true;
		_cv_close.wait(lock, [this]() {
			return _use_count = 0;
		});
		while (!_connection_queue.empty()) {
			DBConnection *p = _connection_queue.front();
			_connection_queue.pop();
			delete p;
		}
		_pool_alive = false;
	}
	_cv.notify_all();
}

void DBConnectionPool::produce_connection() {
	for (;;) {
		std::unique_lock<std::mutex> ulock(_queue_mutex);
		_cv.wait(ulock, [this]() {
			return !this->_pool_alive || this->_connection_queue.empty();
		});
		if (!_pool_alive) break;
		// 连接数量没有到达上线，继续创建
		if (_connection_count < _max_size) {
			DBConnection *p = new DBConnection;
			p->connect(_ip, _port, _user, _passwd, _dbname);
			p->refresh(); // 更新开始空闲的起始时间
			_connection_queue.push(p);
			++_connection_count;
		}

		_cv.notify_all(); // 通知消费者线程可以消费了
	}
}

void DBConnectionPool::reclaim_connection() {
	for (;;) {
		// 模拟定时效果
		std::this_thread::sleep_for(std::chrono::seconds(_max_free_time));

		// 扫描整个队列，释放多余的连接
		std::unique_lock<std::mutex> ulock(_queue_mutex);
		while (_connection_count > _init_size && _pool_alive) {
			DBConnection *p = _connection_queue.front();
			if (p->get_free_time() >= (_max_free_time * 100)) {
				if (p->get_free_time() >= (_max_free_time * 1000)) {
					_connection_queue.pop();
					--_connection_count;
					delete p; // 释放连接
				} else {
					break; // 队头都没有超过，其他肯定没有
				}
			}
		}
	}
}