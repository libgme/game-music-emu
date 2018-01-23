// File_Extractor 0.4.0. http://www.slack.net/~ant/

#include "Data_Reader.h"

#include "blargg_endian.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

/* Copyright (C) 2005-2006 Shay Green. This module is free software; you
can redistribute it and/or modify it under the terms of the GNU Lesser
General Public License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version. This
module is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
details. You should have received a copy of the GNU Lesser General Public
License along with this module; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA */

#include "blargg_source.h"

#ifdef HAVE_ZLIB_H
#include <zlib.h>
#include <stdlib.h>
#include <errno.h>

static const unsigned char gz_magic[2] = {0x1f, 0x8b}; /* gzip magic header */

#define Z_BUFSIZE 4096

#define ALLOC(size) malloc(size)
#define TRYFREE(p) {if (p) free(p);}

#define EXTRA_FIELD  0x04 /* bit 2 set: extra field present */
#define HEAD_CRC     0x02 /* bit 1 set: header CRC present */
#define EXTRA_FIELD  0x04 /* bit 2 set: extra field present */
#define ORIG_NAME    0x08 /* bit 3 set: original file name present */
#define COMMENT      0x10 /* bit 4 set: file comment present */
#define RESERVED     0xE0 /* bits 5..7: reserved */


#if MAX_MEM_LEVEL >= 8
#  define DEF_MEM_LEVEL 8
#else
#  define DEF_MEM_LEVEL  MAX_MEM_LEVEL
#endif

#if defined(MSDOS) || (defined(WINDOWS) && !defined(WIN32))
#  define OS_CODE  0x00
#endif

#ifdef AMIGA
#  define OS_CODE  1
#endif

#if defined(VAXC) || defined(VMS)
#  define OS_CODE  2
#endif

#ifdef __370__
#  if __TARGET_LIB__ < 0x20000000
#	define OS_CODE 4
#  elif __TARGET_LIB__ < 0x40000000
#	define OS_CODE 11
#  else
#	define OS_CODE 8
#  endif
#endif

#if defined(ATARI) || defined(atarist)
#  define OS_CODE  5
#endif

#ifdef OS2
#  define OS_CODE  6
#endif

#if defined(MACOS) || defined(TARGET_OS_MAC)
#  define OS_CODE  7
#endif

#ifdef __acorn
#  define OS_CODE 13
#endif

#if defined(WIN32) && !defined(__CYGWIN__)
#  define OS_CODE  10
#endif

#ifdef _BEOS_
#  define OS_CODE  16
#endif

#ifdef __TOS_OS400__
#  define OS_CODE 18
#endif

#ifdef __APPLE__
#  define OS_CODE 19
#endif

#ifndef OS_CODE
#  define OS_CODE  3	 /* assume Unix */
#endif

#endif /* HAVE_ZLIB_H */

const char Data_Reader::eof_error [] = "Unexpected end of file";

#define RETURN_VALIDITY_CHECK( cond ) \
	do { if ( unlikely( !(cond) ) ) return "Corrupt file"; } while(0)

blargg_err_t Data_Reader::read( void* p, long s )
{
	RETURN_VALIDITY_CHECK( s > 0 );

	long result = read_avail( p, s );
	if ( result != s )
	{
		if ( result >= 0 && result < s )
			return eof_error;

		return "Read error";
	}

	return 0;
}

blargg_err_t Data_Reader::skip( long count )
{
	RETURN_VALIDITY_CHECK( count >= 0 );

	char buf [512];
	while ( count )
	{
		long n = sizeof buf;
		if ( n > count )
			n = count;
		count -= n;
		RETURN_ERR( read( buf, n ) );
	}
	return 0;
}

long File_Reader::remain() const { return size() - tell(); }

blargg_err_t File_Reader::skip( long n )
{
	RETURN_VALIDITY_CHECK( n >= 0 );

	if ( !n )
		return 0;
	return seek( tell() + n );
}

// Subset_Reader

Subset_Reader::Subset_Reader( Data_Reader* dr, long size )
{
	in = dr;
	remain_ = dr->remain();
	if ( remain_ > size )
		remain_ = max( 0l, size );
}

long Subset_Reader::remain() const { return remain_; }

long Subset_Reader::read_avail( void* p, long s )
{
	s = max( 0l, s );
	if ( s > remain_ )
		s = remain_;
	remain_ -= s;
	return in->read_avail( p, s );
}

// Remaining_Reader

Remaining_Reader::Remaining_Reader( void const* h, long size, Data_Reader* r )
{
	header = (char const*) h;
	header_end = header + max( 0l, size );
	in = r;
}

long Remaining_Reader::remain() const { return header_end - header + in->remain(); }

long Remaining_Reader::read_first( void* out, long count )
{
	count = max( 0l, count );
	long first = header_end - header;
	if ( first )
	{
		if ( first > count )
			first = count;
		void const* old = header;
		header += first;
		memcpy( out, old, first );
	}
	return first;
}

long Remaining_Reader::read_avail( void* out, long count )
{
	count = max( 0l, count );
	long first = read_first( out, count );
	long second = max( 0l, count - first );
	if ( second )
	{
		second = in->read_avail( (char*) out + first, second );
		if ( second <= 0 )
			return second;
	}
	return first + second;
}

blargg_err_t Remaining_Reader::read( void* out, long count )
{
	count = max( 0l, count );
	long first = read_first( out, count );
	long second = max( 0l, count - first );
	if ( !second )
		return 0;
	return in->read( (char*) out + first, second );
}

// Mem_File_Reader

Mem_File_Reader::Mem_File_Reader( const void* p, long s ) :
	#ifdef HAVE_ZLIB_H
	m_z_err(Z_OK), /* error code for last stream operation */
	m_inbuf(0), /* output buffer */
	m_z_eof(0),
	m_transparent(0),
	m_gzip( (LPGZIP) p ),
	m_gzip_len( (size_t)max( 0l, s ) ),
	m_gzip_pos(0),
	m_begin(0),
	m_size(0),
	#else //HAVE_ZLIB_H
	m_begin( (const char*) p ),
	m_size( max( 0l, s ) ),
	#endif //HAVE_ZLIB_H
	m_pos(0)
{
	#ifdef HAVE_ZLIB_H
	if(m_gzip == 0)
		return;

	if ( m_gzip_len >= 2 && memcmp(m_gzip, gz_magic, 2) != 0 )
	{
		/* Don't try to decompress non-GZ files, just assign input pointer */
		m_begin = (const char*)p;
		m_size = s;
		return;
	}

	m_zstream.zalloc = (alloc_func)0;
	m_zstream.zfree = (free_func)0;
	m_zstream.opaque = (voidpf)0;
	m_zstream.next_in = m_inbuf = Z_NULL;
	m_zstream.next_out = Z_NULL;
	m_zstream.avail_in = m_zstream.avail_out = 0;

	m_inbuf = (Byte*)ALLOC(Z_BUFSIZE);
	m_zstream.next_in = m_inbuf;
	int err = inflateInit2(&(m_zstream), -MAX_WBITS);
	if ( err != Z_OK || m_inbuf == Z_NULL )
	{
		gz_destroy();
		return;
	}

	m_zstream.avail_out = Z_BUFSIZE;
	gz_check_head();
	char outbuf[Z_BUFSIZE];
	long nRead;
	while ( true )
	{
		nRead = gz_read(outbuf, Z_BUFSIZE);
		if(nRead <= 0)
			break;
		write(outbuf, (size_t)nRead);
	}
	gz_destroy();

	m_begin = (const char* const)m_raw_data.data();
	m_size = (long)m_raw_data.size();

	#endif //HAVE_ZLIB_H
}

long Mem_File_Reader::size() const { return m_size; }

long Mem_File_Reader::read_avail( void* p, long s )
{
	long r = remain();
	if ( s > r )
		s = r;
	memcpy( p, m_begin + m_pos, (size_t)s );
	m_pos += s;
	return s;
}

long Mem_File_Reader::tell() const { return m_pos; }

blargg_err_t Mem_File_Reader::seek( long n )
{
	RETURN_VALIDITY_CHECK( n >= 0 );
	if ( n > m_size )
		return eof_error;
	m_pos = n;
	return 0;
}

#ifdef HAVE_ZLIB_H

void Mem_File_Reader::gz_check_head()
{
	int method; /* method byte */
	int flags;  /* flags byte */
	uInt len;
	int c;

	/* Check the gzip magic header */
	for ( len = 0; len < 2; len++ )
	{
		c = gz_get_byte();
		if ( (unsigned char)c != gz_magic[len] )
		{
			if ( len != 0 )
			{
				m_zstream.avail_in++;
				m_zstream.next_in--;
			}
			if ( c != EOF )
			{
				m_zstream.avail_in++;
				m_zstream.next_in--;
				m_transparent = 1;
			}
			m_z_err = (m_zstream.avail_in != 0) ? Z_OK : Z_STREAM_END;
			return;
		}
	}

	method = gz_get_byte();
	flags = gz_get_byte();
	if ( method != Z_DEFLATED || (flags & RESERVED) != 0 )
	{
		m_z_err = Z_DATA_ERROR;
		return;
	}

	/* Discard time, xflags and OS code: */
	for (len = 0; len < 6; len++)
		(void)gz_get_byte();

	if ( (flags & EXTRA_FIELD) != 0 ) /* skip the extra field */
	{
		len  =  (uInt)gz_get_byte();
		len += ((uInt)gz_get_byte())<<8;
		/* len is garbage if EOF but the loop below will quit anyway */
		while (len-- != 0 && gz_get_byte() != EOF){}
	}
	if ( (flags & ORIG_NAME) != 0 ) /* skip the original file name */
	{
		while((c = gz_get_byte()) != 0 && c != EOF){}
	}

	if ( (flags & COMMENT) != 0 ) /* skip the .gz file comment */
	{
		while ((c = gz_get_byte()) != 0 && c != EOF){}
	}

	if ( (flags & HEAD_CRC) != 0 ) /* skip the header crc */
	{
		for (len = 0; len < 2; len++)
			(void)gz_get_byte();
	}

	m_z_err = m_z_eof ? Z_DATA_ERROR : Z_OK;
}

int Mem_File_Reader::gz_get_byte()
{
	if (m_z_eof) return EOF;
	if (m_zstream.avail_in == 0)
	{
		errno = 0;
		m_zstream.avail_in = gz_read_raw(m_inbuf, Z_BUFSIZE);
		if ( m_zstream.avail_in == 0 )
		{
			m_z_eof = 1;
			return EOF;
		}
		m_zstream.next_in = m_inbuf;
	}
	m_zstream.avail_in--;
	return *(m_zstream.next_in)++;
}

uInt Mem_File_Reader::gz_read_raw(LPGZIP buf, size_t size)
{
	size_t nRead = size;
	if ( m_gzip_pos + size >= m_gzip_len )
		nRead = m_gzip_len - m_gzip_pos;
	if ( nRead <= 0 )
		return 0;
	memcpy(buf, m_gzip + m_gzip_pos, nRead);
	m_gzip_pos += nRead;
	return (uInt)nRead;
}

long Mem_File_Reader::gz_read(char *buf, size_t len)
{
	//Bytef *start = (Bytef*)buf; /* starting point for crc computation */
	Byte  *next_out; /* == stream.next_out but not forced far (for MSDOS) */

	if (m_z_err == Z_DATA_ERROR || m_z_err == Z_ERRNO) return -1;
	if (m_z_err == Z_STREAM_END) return 0;  /* EOF */

	next_out = (Byte*)buf;
	m_zstream.next_out = (Bytef*)buf;
	m_zstream.avail_out = (uInt)len;
	while ( m_zstream.avail_out != 0 )
	{
		if ( m_transparent )
		{
			/* Copy first the lookahead bytes: */
			uInt n = m_zstream.avail_in;
			if (n > m_zstream.avail_out) n = m_zstream.avail_out;
			if (n > 0)
			{
				memcpy(m_zstream.next_out,m_zstream.next_in, n);
				next_out += n;
				m_zstream.next_out = next_out;
				m_zstream.next_in   += n;
				m_zstream.avail_out -= n;
				m_zstream.avail_in  -= n;
			}
			if ( m_zstream.avail_out > 0 )
			{
				m_zstream.avail_out -= gz_read_raw(next_out, m_zstream.avail_out);
			}
			len -= m_zstream.avail_out;
			m_zstream.total_in  += (uLong)len;
			m_zstream.total_out += (uLong)len;
			if(len == 0)
				m_z_eof = 1;
			return (int)len;
		}

		if ( m_zstream.avail_in == 0 && !m_z_eof )
		{
			errno = 0;
			m_zstream.avail_in = gz_read_raw(m_inbuf, Z_BUFSIZE);
			if ( m_zstream.avail_in == 0 )
			{
				m_z_eof = 1;
			}
			m_zstream.next_in = m_inbuf;
		}

		m_z_err = inflate(&(m_zstream), Z_NO_FLUSH);
		if(m_z_err != Z_OK || m_z_eof)
			break;
	}

	return (int)(len - m_zstream.avail_out);
}

size_t Mem_File_Reader::write(char *buf, size_t count)
{
	if ( buf == 0 )
		return 0;
	size_t prev_end = m_raw_data.size();
	m_raw_data.resize(m_raw_data.size() + count);
	memcpy(m_raw_data.data() + prev_end, buf, count);
	if (m_raw_data.capacity() >= m_raw_data.size())
		m_raw_data.reserve(1024);
	return count;
}

int Mem_File_Reader::gz_destroy()
{
	int err = Z_OK;
	if ( m_zstream.state != NULL )
		err = inflateEnd(&(m_zstream));
	if ( m_z_err < 0 )
		err = m_z_err;
	TRYFREE(m_inbuf);
	return err;
}

#endif //HAVE_ZLIB_H


// Callback_Reader

Callback_Reader::Callback_Reader( callback_t c, long size, void* d ) :
	callback( c ),
	data( d )
{
	remain_ = max( 0l, size );
}

long Callback_Reader::remain() const { return remain_; }

long Callback_Reader::read_avail( void* out, long count )
{
	if ( count > remain_ )
		count = remain_;
	if ( count < 0 || Callback_Reader::read( out, count ) )
		count = -1;
	return count;
}

blargg_err_t Callback_Reader::read( void* out, long count )
{
	RETURN_VALIDITY_CHECK( count >= 0 );
	if ( count > remain_ )
		return eof_error;
	return callback( data, out, count );
}

// Std_File_Reader

#ifdef HAVE_ZLIB_H

static const char* get_gzip_eof( const char* path, long* eof )
{
	FILE* file = fopen( path, "rb" );
	if ( !file )
		return "Couldn't open file";

	unsigned char buf [4];
	if ( fread( buf, 2, 1, file ) > 0 && buf [0] == 0x1F && buf [1] == 0x8B )
	{
		fseek( file, -4, SEEK_END );
		fread( buf, 4, 1, file );
		*eof = get_le32( buf );
	}
	else
	{
		fseek( file, 0, SEEK_END );
		*eof = ftell( file );
	}
	const char* err = (ferror( file ) || feof( file )) ? "Couldn't get file size" : 0;
	fclose( file );
	return err;
}
#endif


Std_File_Reader::Std_File_Reader() :
	file_( 0 )
#ifdef HAVE_ZLIB_H
	, gzfile_(0)
	, size_(0)
#endif
{ }

Std_File_Reader::~Std_File_Reader() { close(); }

blargg_err_t Std_File_Reader::open( const char* path )
{
	file_ = fopen( path, "rb" );
	if ( !file_ )
		return "Couldn't open file";

#ifdef HAVE_ZLIB_H
	char in_magic[2];
	/* Detect GZip */
	if ( fread(in_magic, 1, 2, (FILE*)file_) != 2 )
	{
		close();
		return "File is too small";
	}
	fseek( (FILE*) file_, 0, SEEK_SET );

	if ( memcmp( in_magic, gz_magic, 2 ) == 0 )
	{
		close();

		RETURN_ERR( get_gzip_eof( path, &size_ ) );

		gzfile_ = gzopen( path, "rb" );
		if ( !gzfile_ )
			return "Couldn't open GZ file";
	}
#endif

	return 0;
}

long Std_File_Reader::size() const
{
#ifdef HAVE_ZLIB_H
	if ( gzfile_ )
		return size_;
#endif
	long pos = tell();
	fseek( (FILE*) file_, 0, SEEK_END );
	long result = tell();
	fseek( (FILE*) file_, pos, SEEK_SET );
	return result;
}

long Std_File_Reader::read_avail( void* p, long s )
{
#ifdef HAVE_ZLIB_H
	if ( gzfile_ )
		return gzread( gzfile_, p, (unsigned int)s );
#endif
	return fread( p, 1, max( 0l, s ), (FILE*) file_ );
}

blargg_err_t Std_File_Reader::read( void* p, long s )
{
	RETURN_VALIDITY_CHECK( s > 0 );
#ifdef HAVE_ZLIB_H
	if ( gzfile_ )
	{
		if ( s == (long) gzread( gzfile_, p, (unsigned int)s ) )
			return 0;
		if ( gzeof( gzfile_ ) )
			return eof_error;
		return "Couldn't read from GZ file";
	}
#endif
	if ( s == (long) fread( p, 1, (size_t)s, (FILE*) file_ ) )
		return 0;
	if ( feof( (FILE*) file_ ) )
		return eof_error;
	return "Couldn't read from file";
}

long Std_File_Reader::tell() const
{
#ifdef HAVE_ZLIB_H
	if ( gzfile_ )
		return gztell( gzfile_ );
#endif
	return ftell( (FILE*) file_ );
}

blargg_err_t Std_File_Reader::seek( long n )
{
#ifdef HAVE_ZLIB_H
	if ( gzfile_ )
	{
		if ( gzseek( gzfile_, n, SEEK_SET ) >= 0 )
			return 0;
		if ( n > size_ )
			return eof_error;
		return "Error seeking in GZ file";
	}
#endif
	if ( !fseek( (FILE*) file_, n, SEEK_SET ) )
		return 0;
	if ( n > size() )
		return eof_error;
	return "Error seeking in file";
}

void Std_File_Reader::close()
{
#ifdef HAVE_ZLIB_H
	if ( gzfile_ )
	{
		gzclose( gzfile_ );
		gzfile_ = 0;
	}
#endif
	if ( file_ )
	{
		fclose( (FILE*) file_ );
		file_ = 0;
	}
}

