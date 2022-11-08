#include "DBConnection.h"

DBConnection::DBConnection() {
	mysql_init(_connection);

	if (_connection == nullptr) {
		//LOG_ERROR("Initializing connection failed");
	}
}

DBConnection::~DBConnection() {
	if (_connection != nullptr) {
		mysql_close(_connection);
	}
}

bool DBConnection::connect(std::string ip, uint16_t port, std::string user, std::string passwd, std::string dbname) {
	if (mysql_real_connect(_connection, ip.c_str(), user.c_str(), passwd.c_str(), dbname.c_str(), port, nullptr, 0) == nullptr) {
		//LOG_ERROR("connecting mysql failed!");
		return false;
	}
	//LOG_INFO("connecting mysql succeed!");
	return true;
}

bool DBConnection::update(std::string sql) {
	if (mysql_query(_connection, sql.c_str()) == 0) {
		//LOG_INFO("updating failed %s", sql.c_str());
		return false;
	}

	return true;
}

MYSQL_RES *DBConnection::query(std::string sql) {
	if (mysql_query(_connection, sql.c_str()) == 0) {
		//LOG_INFO("query failed %s", sql.c_str());
		return nullptr;
	}

	return mysql_use_result(_connection);
}

void DBConnection::refresh() {
	_last_used_time = clock();
}

clock_t DBConnection::get_free_time() const {
	return clock() - _last_used_time;
}