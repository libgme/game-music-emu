#include "Archive_Reader.h"

#include <string.h>

blargg_err_t const arc_eof = "Archive: End of file";

#ifdef RARDLL

static const int erar_offset = 10;
static blargg_err_t const erar_handle = "Failed to instantiate RAR handle";
static blargg_err_t const erars[] = {
	arc_eof,                      // ERAR_END_ARCHIVE        10
	"RAR: Out of memory",         // ERAR_NO_MEMORY          11
	"RAR: Bad data",              // ERAR_BAD_DATA           12
	"RAR: Bad archive",           // ERAR_BAD_ARCHIVE        13
	"RAR: Unknown format",        // ERAR_UNKNOWN_FORMAT     14
	"RAR: Could not open",        // ERAR_EOPEN              15
	"RAR: Could not create",      // ERAR_ECREATE            16
	"RAR: Could not close",       // ERAR_ECLOSE             17
	"RAR: Could not read",        // ERAR_EREAD              18
	"RAR: Could not write",       // ERAR_EWRITE             19
	"RAR: Buffer too small",      // ERAR_SMALL_BUF          20
	"RAR: Unknown error",         // ERAR_UNKNOWN            21
	"RAR: Missing password",      // ERAR_MISSING_PASSWORD   22
	"RAR: Bad reference record",  // ERAR_EREFERENCE         23
	"RAR: Bad password",          // ERAR_BAD_PASSWORD       24
	"RAR: Dictionary too large",  // ERAR_LARGE_DICT         25
};

static int CALLBACK call_rar( UINT msg, LPARAM UserData, LPARAM P1, LPARAM P2 )
{
	uint8_t **bp = (uint8_t **)UserData;
	uint8_t *addr = (uint8_t *)P1;
	memcpy( *bp, addr, P2 );
	*bp += P2;
	(void) msg;
	return 0;
}

blargg_err_t Rar_Reader::open( const char* path )
{
	RAROpenArchiveData data;
	memset( &data, 0, sizeof data );
	data.ArcName = (char *)path;
	data.OpenMode = RAR_OM_LIST;
	if ( !(rar = RAROpenArchive( &data )) )
		return erar_handle;

	// determine space needed for the unpacked size and file count.
	int res;
	while ( (res = RARReadHeader( rar, &head )) == ERAR_SUCCESS )
	{
		if ( (res = RARProcessFile( rar, RAR_SKIP, nullptr, nullptr )) != ERAR_SUCCESS )
			break;
		size_ += head.UnpSize;
		count_ += 1;
	}
	if ( res != ERAR_END_ARCHIVE || (res = RARCloseArchive( rar )) != ERAR_SUCCESS )
		return erars[res - erar_offset];

	// prepare for extraction
	data.OpenMode = RAR_OM_EXTRACT;
	if ( !(rar = RAROpenArchive( &data )) )
		return erar_handle;
	RARSetCallback( rar, call_rar, (LPARAM)&buf_ptr );
	return nullptr;
}

blargg_err_t Rar_Reader::next( void* bp, arc_entry_t* entry )
{
	// if prev entry was not a music emu file, buf_ptr returns to prev position
	buf_ptr = bp;
	int res;
	if ( (res = RARReadHeader( rar, &head )) != ERAR_SUCCESS
	|| (res = RARProcessFile( rar, -1, nullptr, nullptr )) != ERAR_SUCCESS )
		return erars[res - erar_offset];

	entry->name = head.FileName;
	entry->size = head.UnpSize;
	return nullptr;
}

Rar_Reader::~Rar_Reader() {
	RARCloseArchive( rar );
}

#endif // RARDLL


#ifdef HAVE_LIBARCHIVE

const char* zip_err_struct = "Failed to create archive struct";

#if ARCHIVE_VERSION_NUMBER < 3000000
#define archive_read_free archive_read_finish
#define archive_read_support_filter_all archive_read_support_compression_all
#endif

#ifdef HAVE_ZLIB_H

#include <zlib.h>

static const uint16_t gz_signature = BLARGG_2CHAR( 0x1f, 0x8b );

static const int zerr_offset = -6;
static blargg_err_t const zerrs[] = {
	"GZ: Bad version",       // Z_VERSION_ERROR (-6)
	"GZ: Buffer too small",  // Z_BUF_ERROR    (-5)
	"GZ: Out of memory",     // Z_MEM_ERROR    (-4)
	"GZ: Bad Data",          // Z_DATA_ERROR   (-3)
	"GZ: Stream error",      // Z_STREAM_ERROR (-2)
};
#endif // HAVE_ZLIB_H

blargg_err_t Zip_Reader::open_zip( const char* path ) {
	if ( !(zip = archive_read_new()) )
		return zip_err_struct;
	if ( archive_read_support_filter_all( zip ) != ARCHIVE_OK
	|| archive_read_support_format_zip( zip ) != ARCHIVE_OK
	|| archive_read_open_filename( zip, path, 10240 ) != ARCHIVE_OK )
		return archive_error_string( zip );
	return nullptr;
}

blargg_err_t Zip_Reader::open( const char* path )
{
	blargg_err_t err;
	if ( (err = open_zip( path )) )
		return err;

	// determine space needed for the unpacked size and file count.
	int res;
	while ( (res = archive_read_next_header( zip, &head )) == ARCHIVE_OK )
	{
#ifdef HAVE_ZLIB_H
		char h[3];
		archive_read_data( zip, &h, 3 );
		if ( BLARGG_2CHAR( h[0], h[1] ) == gz_signature && h[2] == 8 )
		{
			// gzip puts its uncompressed file size in the footer
			blargg_vector<uint8_t> buf;
			if ( (err = buf.resize( archive_entry_size( head ) - 3 )) )
				return err;
			archive_read_data( zip, buf.begin(), buf.size() );
			const uint8_t* b = buf.end() - 4;
			size_ += BLARGG_4CHAR(b[3], b[2], b[1], b[0]);
		}
		else
#endif // HAVE_ZLIB_H
		{
			size_ += archive_entry_size( head );
		}
		count_ += 1;
	}
	if ( res != ARCHIVE_EOF || archive_read_free( zip ) != ARCHIVE_OK )
		return archive_error_string( zip );
	return (err = open_zip( path )) ? err : nullptr;
}

blargg_err_t Zip_Reader::next( void* buf_ptr, arc_entry_t* entry )
{
	int res;
	if ( (res = archive_read_next_header( zip, &head )) != ARCHIVE_OK )
		return (res == ARCHIVE_EOF) ? arc_eof : archive_error_string( zip );

	uint8_t* bp = (uint8_t*)buf_ptr;
	int64_t siz = archive_entry_size( head );
	ssize_t pos = 0;
#ifdef HAVE_ZLIB_H
	pos += archive_read_data( zip, bp, 3 );
	if ( BLARGG_2CHAR( bp[0], bp[1] ) == gz_signature && bp[2] == 8 )
	{
		// load the gzip file into a separate buffer
		blargg_err_t err;
		blargg_vector<uint8_t> buf;
		if ( (err = buf.resize( siz )) )
			return err;
		memcpy( &buf[0], bp, pos );
		archive_read_data( zip, &buf[0] + pos, buf.size() - pos );
		const uint8_t* b = buf.end() - 4;
		siz = BLARGG_4CHAR(b[3], b[2], b[1], b[0]);

		z_stream stream;
		memset( &stream, 0, sizeof stream );
		stream.next_in = buf.begin();
		stream.avail_in = buf.size();
		stream.next_out = bp;
		stream.avail_out = siz;

		// 15 window bits, and the +32 tells zlib to to detect if using gzip or zlib
		if ( (res = inflateInit2( &stream, 15 + 32 )) != Z_OK
		|| (res = inflate( &stream, Z_FINISH )) != Z_STREAM_END )
		{
			inflateEnd( &stream );
			return zerrs[res - zerr_offset];
		}
		pos = stream.total_out;
		inflateEnd( &stream );
	}
	else
#endif // HAVE_ZLIB_H
	{
		pos += archive_read_data( zip, bp + pos, siz - pos );
	}
	if ( pos != siz )
		return "ZIP: header size does not match total bytes read";

	entry->name = archive_entry_pathname( head );
	entry->size = siz;
	return nullptr;
}

Zip_Reader::~Zip_Reader()
{
	archive_read_free( zip );
}

#endif // HAVE_LIBARCHIVE
