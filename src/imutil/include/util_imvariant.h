#pragma once

#include "../include/source_config.h"


#ifdef IM_UTIL_EXPORTS
#define UTILVARIANT DLL_EXPORT
#else
#define UTILVARIANT DLL_IMPORT
#endif


namespace util {

//注意：当IMVariant构造函数参数为非基本类型数据结构时（非int,bool,double），该参数需支持拷贝构造函数

class IMVariantPrivate;
class UTILVARIANT IMVariant
{
public:
	enum Type
	{
		kInvalid,
		kBool,
		kInt16,
		kInt32,
        kUint16,
		kUint32,
		kUint64,
		kFloat,
		kString,
		kStringRef,
		kInt64,
		kDouble
	};
	IMVariant();
	explicit IMVariant(Int16 s);
	explicit IMVariant(UInt16 us);
	explicit IMVariant(Int32 i);
	explicit IMVariant(UInt32 u);
	explicit IMVariant(UInt64 u64);
	explicit IMVariant(Int64 i64);
	explicit IMVariant(bool b);
	explicit IMVariant(Float f);
	explicit IMVariant(double df);
	explicit IMVariant(const im_char* str);     // 注意：该函数未对str进行拷贝，只引用参数值
	IMVariant(const im_char* str, Int32 size);  // 该函数对拷贝str
	IMVariant(const IMVariant& other);
	~IMVariant();
	IMVariant& operator= (const IMVariant& other);

	// 以下运算符函数原理：根据IMVariant本身的type, 将参数str转换为其对应type后，进行运输符判// 以下运算符函数原理：根据IMVariant本身的type, 将参数str转换为其对应type后，进行运输符判断// 以下运算符函数原理：根据IMVariant本身的type, 将参数str转换为其对应type后，进行运输符判断		// �����������ԭ�?���IMVariant�����type, ������strת��Ϊ���Ӧtype�㬽���������ж�
	bool operator <  ( const char* str ) const;
	bool operator >  ( const char* str ) const;
	bool operator == ( const char* str ) const;

	bool operator == ( const IMVariant &other ) const;
	bool operator != ( const IMVariant &other ) const;
	/* 暂未实现
	bool operator <  ( const IMVariant &other ) const;
	bool operator <= ( const IMVariant &other ) const;
	bool operator >= ( const IMVariant &other ) const;
	bool operator >  ( const IMVariant &other ) const;
	*/

	inline Type			type() const {return type_;}

    Int16				toInt16(bool* ok=0) const;
	UInt16				toUInt16(bool* ok=0) const;
	Int32				toInt(bool* ok=0) const;
	UInt32				toUInt(bool* ok=0) const;
	UInt64				toUInt64(bool* ok=0) const;
	Float				toFloat(bool* ok=0) const;
	Int64				toInt64(bool* ok=0) const;
	double				toDouble(bool* ok=0) const;
	bool				toBool() const;
	im_string			toString() const;

	std::vector<IMVariant> splitString(const char spliter);

	static std::vector<im_string> splitString(const char* str_with_spliter, const char spliter);

private:
	void				assign(const IMVariant& other);
	void				clear();


private:
	Type				type_;
	union BasicValue
	{
	    Int16          s;
	    UInt16         us;
		Int32			i;
		UInt32			u;
		UInt64			u64;
		Float			f;
		bool			b;
		const char*		str;
		Int64           i64;
        double          df;
	}					value_;

	void*				object_value_;
};

}
