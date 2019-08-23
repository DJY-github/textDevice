#pragma once

#include "source_config.h"
#include "util_imdata.h"
#include "util_imbufer.h"

#ifdef IM_UTIL_EXPORTS
#define UTILIMDIR DLL_EXPORT
#else
#define UTILIMDIR DLL_IMPORT
#endif


namespace util {

static const int kMaxPath = 256;
class UTILIMDIR IMDir
{
public:
	IMDir(const im_char* path);
	IMDir(const IMDir& other);
	~IMDir();

	IMDir&				cd(const im_char* dir);
	bool				createPath(const im_char* path, bool fore = true);
	const char*			path() const {return path_;}

	/*
		inc_suffix: 文件后缀名称，使用类似".htm"格式，表示只查找".htm"文件。 支持多类型,";"号隔开,如".htm;.html"
		rid_name: 排除的文件或文件夹名称。 字符末尾带"/"表示文件夹。
		以上参数 输入 0或nullptr, 表示不过滤
		*/
	void				setFilter(const char* inc_suffix, const char* rid_name = nullptr, bool dir_only = false, bool need_md5 = false);
	void                setFileInfo(bool is_only_file_name) {is_only_file_name_ = is_only_file_name;}
	void				entry(IMData* imdata);

	static IMDir		applicationDir();
	static im_string	applicationName();
	static int          sync(const char* path);

	static bool			removeDir(const char* dir);
private:
	bool				findFile(const im_char* path, IMData* imdata);
	void				setFileInfo(const im_string& file_name, const im_string& file_path, const bool is_dir,
										const im_string& file_suffix, IMData* imdata);

	bool				isFileSuffixInFilter(const im_string& file_suffix);
	bool				isRidFileNameInFilter(const im_string& file_name);
	bool				isDotDirAndInFilter(const im_string& file_name);
	const im_string		getFileSuffix(const im_string& file_name);
	const im_string		getFileNameWithoutSuffix(const im_string& file_name);

private:
	static const char*	trimParentPath(const char* path, const char* parent_path);

private:
	im_char				path_[kMaxPath+1];
	bool                   is_only_file_name_;

	struct Filter
	{
		typedef std::vector<im_string> StringVector;
		StringVector		inc_suffix_vector_;
		StringVector		rid_name_vector_;
		bool				need_md5_;
		bool				dir_only_;
	};

	Filter*				filter_;
};

class UTILIMDIR IMFile
{
public:
	enum RWMode
	{
		kRead		= 0,
		kWrite		= (1 << 0),
	};
	enum CreateMode
	{
		kCreateNew			= 1,		// 创建文件,如文件存在则会出错
		kCreateAlways		= 2,		// 创建文件，会改写前一个文件
		kOpenExisting		= 3,		// 文件必须已经存在
		kOpenAlways			= 4,		// 如文件不存在则创建它
		kTruncateExisting	= 5			// 将现有文件缩短为零长度
	};

	typedef IMBuffer<102400>  FileData;
	IMFile();
	~IMFile();
	bool		open(const char* file_name, int rw_mode, CreateMode create_mode);
	void		close();
	bool		isOpen();
	int			read(char* data, int len);
	void		setFilePointer(int distance_to_move, int move_method);
	int			write(const char* data, int len);
	int         syncWrite(const char* data, int len);
	int			readAll(FileData& data);

	unsigned long	getFileSize();

	static bool			rename(const char* existing_file_name, const char* new_file_name);
	static bool			removeFile(const char* filename);
	static bool			isFileExist(const char* filename);
	static im_string	lastNameOfPath(const char* path);
	static im_string	removeLastNameOfPath(const char* path);
	static im_string    getFileNameWithoutSuffix(const im_string& file_name);

private:
	void*		fp_;
	int			rw_mode_;
	CreateMode	create_mode_;
	bool		is_open_;
	im_string   filename_;
};

}
