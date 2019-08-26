#pragma once


#include "../../include/util_thread.h"

namespace util {  namespace dbi {

class ISqlSession
{
public:
	ISqlSession(){}
	virtual ~ISqlSession(){}
	virtual bool		open(const char* db_name, const char* db_user = NULL, const char* db_pwd = NULL, const char* db_host = NULL, unsigned int db_port = NULL) = 0;
	virtual void		close() = 0;
	bool				is_open() const {return is_open_;}
	IMRecursiveMutex&	session_mutex()    {return session_mutex_;}

	virtual bool        exec(const char* sql, unsigned int *affected_rows = NULL) = 0;
	virtual bool        beginTransaction() {return true;}
    virtual bool        commit() {return true;}
    virtual bool        rollback(){return true;}

protected:
	bool				is_open_;
	IMRecursiveMutex	session_mutex_;
};


class ISqlQuery
{
public:
	ISqlQuery(ISqlSession* session, const char* sql):sql_(sql),session_(session){}
	virtual ~ISqlQuery(){}
	virtual	bool			exec(unsigned int *affected_rows = NULL) = 0;
	virtual	unsigned int	rowCount() = 0;
	virtual	bool			next() = 0;
	virtual	const char*		value(int field_index) const = 0;
	virtual const char*		lastError() const = 0;
protected:
	std::string				sql_;
	ISqlSession*			session_;
};

#define  SQLMODULE	"sql"

}}
