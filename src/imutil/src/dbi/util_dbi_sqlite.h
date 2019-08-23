#pragma once

#ifdef _WIN32
	#include <WinSock2.h>
#endif // _WIN32

#include "sqlite3.h"

#include "../../include/util_imvariant.h"
#include "util_dbi_isql.h"


namespace util {  namespace dbi {


class SqliteSession : public ISqlSession
{
public:
	friend class SqliteQuery;
	SqliteSession();
	virtual ~SqliteSession();
	bool		open(const char* db_name, const char* db_user = NULL, const char* db_pwd = NULL, const char* db_host = NULL, unsigned int db_port = NULL);
	bool		reopen();
	void		close();

	bool        exec(const char* sql, unsigned int *affected_rows = NULL);
	bool        beginTransaction();
    bool        commit();
    bool        rollback();
private:
	sqlite3*		    connection_;
	std::string			db_name_;
    bool                use_nfs_;

};

class SqliteQuery : public ISqlQuery
{
public:
	SqliteQuery(ISqlSession* session, const char* sql);
	~SqliteQuery();
	bool				exec(unsigned int *affected_rows = NULL);
	unsigned int		rowCount();  // 不支持该函数
	bool				next();
	const char*			value(int field_index) const;
	const char*			lastError() const	{return last_error_.c_str();}

private:
	//SqliteQuery(const SqliteQuery& ohter){}
	SqliteQuery& operator= (const SqliteQuery& other){return *this;}

private:
	sqlite3_stmt*       stmt3_;
	int					filed_count_;
    int                 curr_row_;
    int                 first_step_ret_;

	std::string			last_error_;

};


}}
