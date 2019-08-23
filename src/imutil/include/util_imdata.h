#pragma once

#include "source_config.h"
#include "util_imvariant.h"

#ifdef IM_UTIL_EXPORTS
#define UTILIMDATA DLL_EXPORT
#else
#define UTILIMDATA DLL_IMPORT
#endif


namespace util {


class UTILIMDATA IMData
{
public:
	typedef std::vector<IMData*> Children;
	typedef std::vector<im_string>	Keys;

	IMData();
	virtual ~IMData();
	IMData& operator= (const IMData& other);
private:
	IMData(void* d, IMData* parent);
public:
	im_string			ident_str(const char* key) const;
	IMVariant			ident(const char* key) const;
	IMVariant			attrs(const char* domain, const char*key) const;
	const IMData*		child(unsigned int index) const;
	IMData*			    parent() const;

	void				setIdent(const char* key, const std::string& value);
	void				setAttrs(const char* domain, const char* key ,const std::string& value);
	void				setIdent(const char* key, const IMVariant& value);
	void				setAttrs(const char* domain, const char* key, const IMVariant& value);

    bool                isExistIdent(const char* key) const;
	bool                isExistDomain(const char* domain) const;
	bool                isExistDomainAttr(const char* domain, const char* key) const;

	void				setIdent(const IMData& imdata);
	void				setAttrs(const IMData& imdata);
	void				setAttrs(const char* domain, const IMData& imdata);

	void				removeMember(const char* key);

	IMData*			    newChild();
	IMData*			    child(unsigned int index);
	const UInt32 		childCount() const;

	Keys				identKeys() const;
	Keys				domainKeys() const;
	Keys				attrsKeys(const char* domain) const;

	bool				fromJson(const char* json_str);
	bool				fromJson(const char* domain, const char* domain_json_str);
	bool				fromJson(const im_string& json_str);
	bool				fromJson(const char* domain, const im_string&  domain_json_str);

	std::string		    toJson(bool style = false) const;
	std::string		    toJson(const char* domain, bool style = false) const;
	void				clear();

	int					error(im_string& error_dec);
	void				setError(int error_code, const std::string& error_desc);

public:
    void*               jsonValue()  {return d_;}  // 获取json库的Value对象。用于非IMData的数据格式
    const void*         jsonValue() const  {return d_;}

private:
	bool				parseToIMData(IMData* imdata,  void* d);

private:				// not use
	IMData(const IMData& other);
private:
	void*				d_;
	Children*			children_;
	IMData*				parent_;
};

}
