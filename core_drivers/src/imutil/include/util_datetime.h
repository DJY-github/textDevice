#ifndef _UTIL_DATETIME_H_
#define _UTIL_DATETIME_H_

#include <time.h>
#include <sys/timeb.h>
#include "source_config.h"


#ifdef IM_UTIL_EXPORTS
#define UTILDATETIME DLL_EXPORT
#else
#define UTILDATETIME DLL_IMPORT
#endif




namespace util {


enum Timezone
{
    kGMT = 0,
    kLocal
};

class UTILDATETIME IMDateTime
{
public:
	enum Format
	{
		kFormatISO, 	 // like "2012-11-01 12:00:00"
		kFormatShort
	};
	IMDateTime();
	explicit IMDateTime(Time utc, Timezone tz = kGMT);
	IMDateTime(tm sys_tm, timeb sys_timeb);
	IMDateTime(const IMDateTime& other);
	~IMDateTime();
	IMDateTime& operator= (const IMDateTime& other);

	im_string	toString(Format format = kFormatISO);
	im_string	toString(const char* format);
	Time		toUTC();
	int			week();

	const tm&   sys_tm(){return sys_tm_;}

	static IMDateTime  fromString(const char* format_str, Format format = kFormatISO);
	static IMDateTime  fromWeek(int year, int weeks, int week_day);

private:
	tm			sys_tm_;
	timeb		sys_timeb_;

public:
	static IMDateTime currentDateTime(Timezone tz = kGMT);
};

class UTILDATETIME IMTime
{
public:
	void	start();
	void	restart();
	UInt32	elapsed();
private:
	timeb	sys_timeb_;
};

}

#endif
