#include "gme/gme.h"
#include <cassert>
#include <cstdint>
#include <cstdlib>

// GME_4CHAR('a','b','c','d') = 'abcd' (four character integer constant)
#define GME_4CHAR( a, b, c, d ) \
	((a&0xFF)*0x1000000 + (b&0xFF)*0x10000 + (c&0xFF)*0x100 + (d&0xFF))

// GME_2CHAR('a','b') = 'ab' (two character integer constant)
#define GME_2CHAR( a, b ) \
	((a&0xFF)*0x100 + (b&0xFF))

// gme_vector - very lightweight vector of POD types (no constructor/destructor)
template<class T>
class gme_vector {
	T* begin_;
	size_t size_;
public:
	gme_vector() : begin_( 0 ), size_( 0 ) { }
	~gme_vector() { free( begin_ ); }
	size_t size() const { return size_; }
	T* begin() const { return begin_; }
	T* end() const { return begin_ + size_; }
	gme_err_t resize( size_t n )
	{
		void* p = realloc( begin_, n * sizeof (T) );
		if ( !p && n )
			return "Out of memory";
		begin_ = (T*) p;
		size_ = n;
		return 0;
	}
	void clear() { free( begin_ ); begin_ = nullptr; size_ = 0; }
	T& operator [] ( size_t n ) const
	{
		assert( n <= size_ ); // <= to allow past-the-end value
		return begin_ [n];
	}
};

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


#ifdef HAVE_LIBARCHIVE

#include <archive.h>
#include <archive_entry.h>

class Zip_Reader : public Archive_Reader {
	archive* zip = nullptr;
	archive_entry* head = nullptr;
public:
	static const uint32_t signature = GME_4CHAR( 'P', 'K', 0x3, 0x4 );
	gme_err_t open_zip( const char* path );
	gme_err_t open( const char* path );
	gme_err_t next( void* buf_ptr, arc_entry_t* entry );
	~Zip_Reader();
};

#endif // HAVE_LIBARCHIVE
