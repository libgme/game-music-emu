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
		memcpy( out, old, (size_t) first );
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
	m_begin( (const char*) p ),
	m_size( max( 0l, s ) ),
	m_pos(0)
{
	#ifdef HAVE_ZLIB_H
	if(m_begin == 0)
		return;

	if ( gz_decompress() )
	{
		m_begin = (const char* const)m_raw_data.data();
		m_size = (long)m_raw_data.size();
	}
	#endif /* HAVE_ZLIB_H */
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

bool Mem_File_Reader::gz_decompress()
{
	if ( m_size >= 2 && memcmp(m_begin, gz_magic, 2) != 0 )
	{
		/* Don't try to decompress non-GZ files, just assign input pointer */
		return false;
	}

	m_raw_data.clear();
	uInt full_length = (uInt) m_size;
	uInt half_length = (uInt) m_size / 2;

	uInt uncompLength = (uInt) full_length ;
	m_raw_data.resize(uncompLength, '\0');
	char* uncomp = &m_raw_data[0];

	z_stream strm;
	strm.next_in   = (Bytef *)m_begin;
	strm.avail_in  = (uInt) m_size;
	strm.total_out = 0;
	strm.zalloc    = Z_NULL;
	strm.zfree     = Z_NULL;

	bool done = false;

	if ( inflateInit2(&strm, (16 + MAX_WBITS)) != Z_OK )
	{
		m_raw_data.clear();
		return false;
	}

	while ( !done )
	{
		/* If our output buffer is too small */
		if ( strm.total_out >= uncompLength )
		{
			/* Increase size of output buffer */
			m_raw_data.resize(uncompLength + half_length, '\0');
			uncomp = &m_raw_data[0];
			uncompLength += half_length;
		}

		strm.next_out  = (Bytef *) (uncomp + strm.total_out);
		strm.avail_out = uncompLength - (uInt) strm.total_out;

		/* Inflate another chunk. */
		int err = inflate (&strm, Z_SYNC_FLUSH);
		if ( err == Z_STREAM_END )
			done = true;
		else if ( err != Z_OK )
			break;
	}

	if ( inflateEnd(&strm) != Z_OK )
	{
		m_raw_data.clear();
		return false;
	}
	m_raw_data.resize(strm.total_out);

	return true;
}

#endif /* HAVE_ZLIB_H */


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
	return callback( data, out, (int) count );
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
	return (long)fread( p, 1, (size_t) max( 0l, s ), (FILE*) file_ );
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

