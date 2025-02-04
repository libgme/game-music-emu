#include "blargg_common.h"

extern blargg_err_t const arc_eof; // indicates end of archive, not actually an error

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
	virtual blargg_err_t open( const char* path ) = 0;
	virtual blargg_err_t next( void* buf_ptr, arc_entry_t* entry ) = 0;
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
	static const uint32_t signature = BLARGG_4CHAR( 'R', 'a', 'r', '!' );
	blargg_err_t open( const char* path );
	blargg_err_t next( void* buf_ptr, arc_entry_t* entry );
	~Rar_Reader();
};

#endif // RARDLL


#ifdef HAVE_LIBARCHIVE

#include <archive.h>
#include <archive_entry.h>

class Zip_Reader : public Archive_Reader {
	archive* zip = nullptr;
	archive_entry* head = nullptr;
public:
	static const uint32_t signature = BLARGG_4CHAR( 'P', 'K', 0x3, 0x4 );
	blargg_err_t open_zip( const char* path );
	blargg_err_t open( const char* path );
	blargg_err_t next( void* buf_ptr, arc_entry_t* entry );
	~Zip_Reader();
};

#endif // HAVE_LIBARCHIVE
