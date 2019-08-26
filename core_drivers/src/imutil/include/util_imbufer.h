#pragma once

#include <string.h>
#include <stdarg.h>


namespace util {

template <int t_size>
class IMBuffer
{
public:
	IMBuffer()
	{
		is_dymaic_malloc_	= false;
		size_				= 0;
		max_size_			= t_size;
		ptr_				= buf_;
		memset(buf_, 0, t_size);

	}
	~IMBuffer()
	{
		clear();
	}
	void append(const char* str, int size)
	{
		memcpy(append_ptr(size), str, size);
	}
	char* append_ptr(int in_bytes)
	{
		int pos = size_ ;
		return ptr(pos, in_bytes);
	}
	char* ptr(int pos, int in_bytes)
	{
		if(max_size_ - pos < in_bytes)
		{
			char* temp = ptr_;
			ptr_ = new char[pos + in_bytes + 1];
			memcpy(ptr_, temp, size_);
			if(is_dymaic_malloc_)
			{
				delete []temp;
			}
			is_dymaic_malloc_ = true;
			max_size_  = pos + in_bytes;
		}
		size_ += in_bytes;
		return ptr_ + pos;
	}
	const char*	ptr(int pos = 0) const
	{
		return ptr_ + pos;
	}

	const unsigned char& operator[](int pos) const
	{
		return (unsigned char&)(*ptr(pos));
	}

	unsigned char& operator[](int pos)
	{
		return (unsigned char&)(*ptr(pos, 1));
	}

	int size() const
	{
		return size_;
	}
	void trim_right(int  pos)
	{
		size_ = pos;
		memset(ptr_ + size_, 0, max_size_ - size_);
	}

	void clear()
	{
		if(is_dymaic_malloc_)
		{
			delete ptr_;
			ptr_ = 0;
		}

		is_dymaic_malloc_	= false;
		size_				= 0;
		max_size_			= t_size;
		ptr_				= buf_;
		memset(buf_, 0, t_size);

	}
	const char* format(const char *fmt, ...)
	{
		va_list ap;
		va_start(ap, fmt);
#ifdef _WIN32
		size_ = im_vsnprintf_s(buf_, max_size_, fmt, ap);
#else
		size_ = vsnprintf(buf_, sizeof(buf_), fmt, ap);
#endif
		va_end(ap);

		return buf_;
	}

	unsigned char asc2hex(char asccode)
	{
		unsigned char ret;
		if('0'<=asccode && asccode<='9')
			ret=asccode-'0';
		else if('a'<=asccode && asccode<='f')
			ret=asccode-'a'+10;
		else if('A'<=asccode && asccode<='F')
			ret=asccode-'A'+10;
		else
			ret=0;
		return ret;
	}



	bool	fromHex(const char* ascii_str)
	{
		if(0 != strlen(ascii_str)%2)
		{
			return false;
		}

		int  i =0;
		while(true)
		{
			if('\0' == *(ascii_str+i) || '\0' == *(ascii_str+i+1))
			{
				break;
			}

			unsigned char hi = asc2hex(*(ascii_str+i));
			unsigned char lo = asc2hex(*(ascii_str+i+1));
			if(hi > 0x16 || lo > 0x16)
			{
				return false;
			}

			(*this)[i/2] = (hi<<4) + lo;
			i += 2;
		}

		return true;
	}
	bool	fromHex(const char* ascii_str, int len)
	{
		if(0 != len %2)
		{
			return false;
		}
	    for(int i=0 ; i<len; i+=2)
		{
			if('\0' == *(ascii_str+i) || '\0' == *(ascii_str+i+1))
			{
				break;
			}

			unsigned char hi = asc2hex(*(ascii_str+i));
			unsigned char lo = asc2hex(*(ascii_str+i+1));
			if(hi > 0x16 || lo > 0x16)
			{
				return false;
			}

			(*this)[i/2] = (hi<<4) + lo;
		}

		return true;
	}

	char hex2asc(unsigned char hex)
    {
        char result = '\0';
        if(hex >= 0 && hex <= 9)
        {
            result = (char)(hex + 48);
        }
        else if(hex >= 10 && hex <= 15)
        {
            result = (char)(hex - 10 + 65);
        }
        return result;
    }

	bool ToAsc(unsigned char *hex_buf, int buf_len)
    {
        unsigned char high,low;
        unsigned char tmp = 0;
        if(hex_buf == NULL)
        {
            return false;
        }

        int pos = 0;
        for(int i=0; i<buf_len;i++)
        {
            tmp = hex_buf[i];
            high = tmp >> 4;
            low = tmp & 15;

            (*this)[pos++] = hex2asc(high);
            (*this)[pos++] = hex2asc(low);
            (*this)[pos++] = ' ';

        }
        (*this)[pos] = '\0';
        return true;
    }

private:
	char	buf_[t_size+1];
	char*	ptr_;
	int		max_size_;
	int		size_;
	bool    is_dymaic_malloc_;

};



}
