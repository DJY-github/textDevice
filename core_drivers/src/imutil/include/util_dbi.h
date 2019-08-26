#pragma once

#include "source_config.h"
#include "util_thread.h"

#ifdef IM_UTIL_EXPORTS
#define UTILSQL DLL_EXPORT
#else
#define UTILSQL DLL_IMPORT
#endif

namespace util {  namespace dbi {


class UTILSQL IMSqlSession
{
public:
	enum  SqlDriverType
	{
		kInvalid,
		kMysql,
		kSqlite
	};
	IMSqlSession();
	~IMSqlSession();
	IMSqlSession& operator= (const IMSqlSession& other);
private:
	IMSqlSession(const SqlDriverType driver_type, const char* session_name);
public:
    IMRecursiveMutex&	session_mutex();

	bool				open(const char* db_name, const char* db_user = NULL, const char* db_pwd = NULL, const char* db_host = NULL, unsigned int db_port = 0);
	void				close();
	bool				isOpen() const;
	const SqlDriverType	type() const {return driver_type_;}
	const char*			name() const {return session_name_.c_str();}

	bool                exec(const char* sql, unsigned int *affected_rows = NULL);
	bool                beginTransaction();
    bool                commit();
    bool                rollback();

	static IMSqlSession		getSession(const SqlDriverType driver_type, const char* session_name);
	static void				removeSession(const SqlDriverType driver_type, const char* session_name);
private:
	SqlDriverType		driver_type_;
	im_string			session_name_;

};

class UTILSQL IMSqlQuery
{
public:
	IMSqlQuery(const IMSqlSession& session, const char* sql);
	~IMSqlQuery();
	bool				exec(unsigned int *affected_rows = NULL);
	unsigned int		rowCount();
	bool				next();
	const char*			value(int field_index) const;
	const char*			lastError() const;
	bool				is_valid() const {return is_valid_;}

private:
	IMSqlQuery(const IMSqlQuery& ohter){}
	IMSqlQuery& operator= (const IMSqlQuery& other){return *this;}
private:
	bool				is_valid_;
	void*				sql_query_impl_;

};




}}
