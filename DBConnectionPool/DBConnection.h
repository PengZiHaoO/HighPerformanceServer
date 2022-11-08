#ifndef __DB_CONNECTION_H__
#define __DB_CONNECTION_H__

#include <mysql/mysql.h>
#include <string>
#include <ctime>

class DBConnection {
public:
	DBConnection();
	~DBConnection();

public:
	//create conection
	bool connect(std::string ip, uint16_t port, std::string user, std::string passwd, std::string dbname);
	//DML
	bool update(std::string sql);
	//query
	MYSQL_RES *query(std::string sql);

	//refresh this connection last used time
	void refresh();

	//get this connection how long it has not been used
	clock_t get_free_time() const;

private:
	MYSQL * _connection;
	clock_t _last_used_time;
};

#endif