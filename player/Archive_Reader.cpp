#include "Archive_Reader.h"

gme_err_t const arc_eof = "Archive: End of file";

#ifdef RARDLL

#include <string.h>

static const int erar_offset = 10;
static gme_err_t const erar_handle = "Failed to instantiate RAR handle";
static gme_err_t const erars[] = {
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

gme_err_t Rar_Reader::open( const char* path )
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

gme_err_t Rar_Reader::next( void* bp, arc_entry_t* entry )
{
	// if prev entry was not a music emu file, buf_ptr returns to prev position
	buf_ptr = bp;
	int res;
	if ( (res = RARReadHeader( rar, &head )) != ERAR_SUCCESS
	|| (res = RARProcessFile( rar, RAR_TEST, nullptr, nullptr )) != ERAR_SUCCESS )
		return erars[res - erar_offset];

	entry->name = head.FileName;
	entry->size = head.UnpSize;
	return nullptr;
}

Rar_Reader::~Rar_Reader() {
	RARCloseArchive( rar );
}

#endif // RARDLL
