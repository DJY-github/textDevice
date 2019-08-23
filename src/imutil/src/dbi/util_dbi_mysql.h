#pragma once

#ifdef USE_MYSQL



#ifdef _WIN32
	#include <WinSock2.h>
#endif // _WIN32


#include "mysql.h"
#include "../../include/util_imvariant.h"
#include "util_dbi_isql.h"

namespace util {  namespace dbi {


class MysqlSession : public ISqlSession
{
public:
	friend class MysqlQuery;
	MysqlSession();
	virtual ~MysqlSession();
	bool		open(const char* db_name, const char* db_user = NULL, const char* db_pwd = NULL, const char* db_host = NULL, unsigned int db_port = NULL);
	bool		reopen();
	void		close();

	bool        exec(const char* sql, unsigned int *affected_rows = NULL);
	bool        beginTransaction();
    bool        commit();
    bool        rollback();
private:
	MYSQL				connection_;

	std::string			db_name_;
	std::string			db_user_;
	std::string			db_pwd_;
	std::string			db_host_;
	unsigned int		db_port_;

};

class MysqlQuery : public ISqlQuery
{
public:
	MysqlQuery(ISqlSession* session, const char* sql);
	~MysqlQuery();
	bool				exec(unsigned int *affected_rows = NULL);
	unsigned int		rowCount();
	bool				next();
	const char*			value(int field_index) const;
	const char*			lastError() const	{return last_error_.c_str();}

private:
	//MysqlQuery(const MysqlQuery& ohter){}
	MysqlQuery& operator= (const MysqlQuery& other){return *this;}

private:
	MYSQL_RES*			result_;
	MYSQL_ROW			current_sql_row_;
	int					filed_count_;

	std::string			last_error_;

};


}}

#endif
