#include "gme/gme.h"
#include <cstddef>
#include <stdint.h>

// GME_4CHAR('a','b','c','d') = 'abcd' (four character integer constant)
#define GME_4CHAR( a, b, c, d ) \
	((a&0xFF)*0x1000000 + (b&0xFF)*0x10000 + (c&0xFF)*0x100 + (d&0xFF))

extern gme_err_t const arc_eof; // indicates end of archive, not actually an error

struct arc_entry_t {
	const char* name;
	size_t size;
};

class Archive_Reader {
protected:
	int count_ = 0;
	long size_ = 0;
public:
	int count() const { return count_; }
	long size() const { return size_; }
	virtual gme_err_t open( const char* path ) = 0;
	virtual gme_err_t next( void* buf_ptr, arc_entry_t* entry ) = 0;
	virtual ~Archive_Reader() { }
};

#ifdef RARDLL

#ifndef _WIN32
# define PASCAL
# define CALLBACK
# define UINT unsigned int
# define LONG long
# define HANDLE void *
# define LPARAM intptr_t
#else
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif
# include <windows.h>
#endif

#if defined RAR_HDR_UNRAR_H
# include <unrar.h>
#elif defined RAR_HDR_DLL_HPP
# include <dll.hpp>
#endif

#ifndef ERAR_SUCCESS
# define ERAR_SUCCESS 0
#endif

class Rar_Reader : public Archive_Reader {
	RARHeaderData head;
	void* rar = nullptr;
	void* buf_ptr = nullptr;
public:
	static const uint32_t signature = GME_4CHAR( 'R', 'a', 'r', '!' );
	gme_err_t open( const char* path );
	gme_err_t next( void* buf_ptr, arc_entry_t* entry );
	~Rar_Reader();
};

#endif // RARDLL
