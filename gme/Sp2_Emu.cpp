// Game_Music_Emu https://github.com/libgme/game-music-emu

#include "Sp2_Emu.h"

#include "blargg_endian.h"
#include <string.h>
#include <stdio.h>

/* Copyright (C) 2004-2006 Shay Green. This module is free software; you
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

Sp2_Emu::Sp2_Emu()
{
	set_type( gme_sp2_type );

	static const char* const names [Snes_Spc::voice_count] = {
		"DSP 1", "DSP 2", "DSP 3", "DSP 4", "DSP 5", "DSP 6", "DSP 7", "DSP 8"
	};
	set_voice_names( names );

	set_gain( 1.4 );

	file_data = 0;
	file_size = 0;
	spc_count_ = 0;
	ram_blocks_count_ = 0;
	track_channel_disable_ = 0;
}

Sp2_Emu::~Sp2_Emu() { }

// Header verification

blargg_err_t Sp2_Emu::check_header( header_t const& h )
{
	if ( memcmp( h.tag, "KSPC", 4 ) != 0 || h.eof_char != 0x1A )
		return gme_wrong_file_type;
	if ( h.version_major != 1 )
		return "Unsupported SPC2 version";
	return 0;
}

// Helper methods

Sp2_Emu::spc_block_t const* Sp2_Emu::get_spc_block( int track ) const
{
	return (spc_block_t const*) (file_data + header_size + (long) track * spc_block_size);
}

long Sp2_Emu::ram_blocks_offset() const
{
	return header_size + (long) spc_block_size * spc_count_;
}

byte const* Sp2_Emu::get_ram_blocks() const
{
	return file_data + ram_blocks_offset();
}

blargg_err_t Sp2_Emu::reconstruct_ram_into( spc_block_t const* sb, byte* ram_out ) const
{
	byte const* ram_blocks = get_ram_blocks();

	for ( int i = 0; i < 256; i++ )
	{
		unsigned block_idx = get_le16( sb->block_map + i * 2 );
		if ( (long) block_idx >= ram_blocks_count_ )
			return "Corrupt SPC2 file: RAM block index out of range";
		memcpy( ram_out + i * 256, ram_blocks + block_idx * 256, 256 );
	}

	return 0;
}

// Track info

static int copy_sp2_field( char* out, char const* in, int len )
{
	// SP2 fields are not null-terminated, copy and trim trailing spaces/nulls
	int end = len;
	while ( end > 0 && (in[end-1] == 0 || in[end-1] == ' ') )
		end--;

	if ( end > Gme_File::max_field_ )
		end = Gme_File::max_field_;

	memcpy( out, in, end );
	out[end] = 0;
	return end;
}

// Extended metadata chunk types
enum {
	chunk_eod = 0,
	chunk_song = 1,
	chunk_game = 2,
	chunk_artist = 3,
	chunk_dumper = 4,
	chunk_comments = 5,
	chunk_ost_title = 6,
	chunk_publisher = 7,
	chunk_filename = 8,
	chunk_upload_mask = 9  // v1.3: SPC page upload mask (32 bytes, hardware only)
};

// Read extended metadata and append to fields
static void read_extended_metadata( byte const* file_data, long file_size,
	uint32_t ext_ptr, track_info_t* out, int* field_lens )
{
	if ( ext_ptr == 0 || ext_ptr >= (uint32_t) file_size )
		return;

	byte const* ptr = file_data + ext_ptr;
	byte const* end = file_data + file_size;

	// Use (end - ptr) comparisons rather than (ptr + N <= end) to avoid forming
	// pointers past one-past-the-end (undefined behavior).
	while ( end - ptr >= 2 )
	{
		int chunk_type = ptr[0];
		int chunk_len = ptr[1];
		ptr += 2;

		if ( chunk_type == chunk_eod )
			break;

		if ( chunk_len > end - ptr )
			break;  // corrupt data

		char* field = NULL;
		int* cur_len = NULL;
		int max_len = Gme_File::max_field_;

		switch ( chunk_type )
		{
			case chunk_song:      field = out->song;    cur_len = &field_lens[0]; break;
			case chunk_game:      field = out->game;    cur_len = &field_lens[1]; break;
			case chunk_artist:    field = out->author;  cur_len = &field_lens[2]; break;
			case chunk_dumper:    field = out->dumper;  cur_len = &field_lens[3]; break;
			case chunk_comments:  field = out->comment; cur_len = &field_lens[4]; break;
			case chunk_ost_title: field = out->ost;     cur_len = &field_lens[5]; break;
			case chunk_publisher: /* no track_info_t slot; folded into copyright by caller */ break;
			case chunk_filename:  /* no track_info_t slot */ break;
		}

		if ( field && cur_len )
		{
			int space = max_len - *cur_len;
			int copy_len = (chunk_len < space) ? chunk_len : space;
			if ( copy_len > 0 )
			{
				memcpy( field + *cur_len, ptr, copy_len );
				*cur_len += copy_len;
				field[*cur_len] = 0;
			}
		}

		ptr += chunk_len;
	}
}

// Decode a packed BCD byte (each nibble 0-9). Returns -1 if any nibble is non-BCD.
static int bcd_byte( byte b )
{
	int hi = (b >> 4) & 0xF;
	int lo = b & 0xF;
	if ( hi > 9 || lo > 9 )
		return -1;
	return hi * 10 + lo;
}

// Convert 4-byte BCD date (MM DD YYhi YYlo) to "YYYY/MM/DD" string
static void format_bcd_date( char* out, size_t out_size, byte const* date )
{
	int mm    = bcd_byte( date[0] );
	int dd    = bcd_byte( date[1] );
	int yy_hi = bcd_byte( date[2] );
	int yy_lo = bcd_byte( date[3] );

	if ( mm < 0 || dd < 0 || yy_hi < 0 || yy_lo < 0 )
	{
		out[0] = 0;  // invalid BCD nibble
		return;
	}

	int yyyy = yy_hi * 100 + yy_lo;

	if ( mm == 0 && dd == 0 && yyyy == 0 )
		out[0] = 0;
	else
		snprintf( out, out_size, "%04d/%02d/%02d", yyyy, mm, dd );
}

// Shared track-info reader used by both Sp2_Emu and Sp2_File.
// Validates that the requested track's SPC block is fully inside the file.
static blargg_err_t fill_sp2_track_info( byte const* file_data, long file_size,
	int spc_count, int track, track_info_t* out )
{
	if ( track < 0 || track >= spc_count )
		return "Invalid SPC2 track index";

	long block_offset = Sp2_Emu::header_size + (long) track * Sp2_Emu::spc_block_size;
	if ( block_offset + Sp2_Emu::spc_block_size > file_size )
		return "Corrupt SPC2 file: SPC block past end of file";

	Sp2_Emu::spc_block_t const* sb =
		(Sp2_Emu::spc_block_t const*) (file_data + block_offset);

	int field_lens[6];
	field_lens[0] = copy_sp2_field( out->song,    sb->song_title, 32 );
	field_lens[1] = copy_sp2_field( out->game,    sb->game_title, 32 );
	field_lens[2] = copy_sp2_field( out->author,  sb->artist,     32 );
	field_lens[3] = copy_sp2_field( out->dumper,  sb->dumper,     32 );
	field_lens[4] = copy_sp2_field( out->comment, sb->comments,   32 );
	field_lens[5] = copy_sp2_field( out->ost,     sb->ost_title,  32 );

	uint32_t ext_ptr = get_le32( sb->extended_data_ptr );
	read_extended_metadata( file_data, file_size, ext_ptr, out, field_lens );

	strncpy( out->system, "Super Nintendo", sizeof(out->system) - 1 );
	out->system[sizeof(out->system) - 1] = 0;

	// Copyright: combine year and publisher when present (e.g. "1995 Squaresoft")
	unsigned year = get_le16( sb->copyright_year );
	char pub[33];
	int pub_len = copy_sp2_field( pub, sb->publisher, 32 );

	out->copyright[0] = 0;
	if ( year > 0 && pub_len > 0 )
		snprintf( out->copyright, sizeof(out->copyright), "%u %s", year, pub );
	else if ( year > 0 )
		snprintf( out->copyright, sizeof(out->copyright), "%u", year );
	else if ( pub_len > 0 )
		snprintf( out->copyright, sizeof(out->copyright), "%s", pub );

	format_bcd_date( out->date, sizeof(out->date), sb->date );

	if ( sb->ost_disk > 0 )
		snprintf( out->disc, sizeof(out->disc), "%d", sb->ost_disk );
	else
		out->disc[0] = 0;

	unsigned ost_track = get_le16( sb->ost_track );
	if ( ost_track > 0 )
		snprintf( out->track, sizeof(out->track), "%u", ost_track );
	else
		out->track[0] = 0;

	// Durations in 1/64000th seconds -> milliseconds
	unsigned play_len = get_le32( sb->play_length );
	unsigned fade_len = get_le32( sb->fade_length );

	out->length      = (play_len > 0) ? (long) (play_len * 1000LL / 64000) : -1;
	out->fade_length = (fade_len > 0) ? (long) (fade_len * 1000LL / 64000) : -1;
	out->intro_length = -1;
	out->loop_length  = -1;

	return 0;
}

blargg_err_t Sp2_Emu::track_info_( track_info_t* out, int track ) const
{
	return fill_sp2_track_info( file_data, file_size, spc_count_, track, out );
}

// Info-only reader

struct Sp2_File : Gme_Info_
{
	Sp2_Emu::header_t header;
	byte const* file_data;
	long file_size;
	int spc_count;

	Sp2_File() { set_type( gme_sp2_type ); }

	blargg_err_t load_mem_( byte const* data, long size )
	{
		if ( size < Sp2_Emu::header_size )
			return gme_wrong_file_type;

		file_data = data;
		file_size = size;
		memcpy( &header, data, sizeof(header) );

		RETURN_ERR( Sp2_Emu::check_header( header ) );

		spc_count = get_le16( header.spc_count );
		if ( spc_count <= 0 )
			return "Corrupt SPC2 file: zero tracks";

		long spc_area_end = Sp2_Emu::header_size + (long) Sp2_Emu::spc_block_size * spc_count;
		if ( size < spc_area_end )
			return "Corrupt SPC2 file: truncated SPC block area";

		set_track_count( spc_count );
		return 0;
	}

	blargg_err_t track_info_( track_info_t* out, int track ) const
	{
		return fill_sp2_track_info( file_data, file_size, spc_count, track, out );
	}
};

static Music_Emu* new_sp2_emu () { return BLARGG_NEW Sp2_Emu ; }
static Music_Emu* new_sp2_file() { return BLARGG_NEW Sp2_File; }

static gme_type_t_ const gme_sp2_type_ = {
	"Super Nintendo (SPC2)",
	0,  // track_count determined dynamically
	&new_sp2_emu,
	&new_sp2_file,
	"SP2",
	0   // flags
};
gme_type_t const gme_sp2_type = &gme_sp2_type_;

// Setup

blargg_err_t Sp2_Emu::set_sample_rate_( long sample_rate )
{
	RETURN_ERR( apu.init() );
	enable_accuracy( false );
	if ( sample_rate != native_sample_rate )
	{
		RETURN_ERR( resampler.buffer_size( native_sample_rate / 20 * 2 ) );
		resampler.time_ratio( (double) native_sample_rate / sample_rate, 0.9965 );
	}
	return 0;
}

void Sp2_Emu::enable_accuracy_( bool b )
{
	Music_Emu::enable_accuracy_( b );
	filter.enable( b );
}

void Sp2_Emu::mute_voices_( int m )
{
	Music_Emu::mute_voices_( m );
	apu.mute_voices( m | track_channel_disable_ );
}

void Sp2_Emu::disable_echo_( bool disable )
{
	apu.disable_echo( disable );
}

blargg_err_t Sp2_Emu::load_mem_( byte const* in, long size )
{
	blaarg_static_assert( sizeof (header_t)    == header_size,    "SP2 header layout incorrect!" );
	blaarg_static_assert( sizeof (spc_block_t) == spc_block_size, "SP2 SPC block layout incorrect!" );

	if ( size < header_size )
		return gme_wrong_file_type;

	header_t const& h = *(header_t const*) in;
	RETURN_ERR( check_header( h ) );

	file_data = in;
	file_size = size;
	spc_count_ = get_le16( h.spc_count );

	if ( spc_count_ <= 0 )
		return "Corrupt SPC2 file: zero tracks";

	long spc_area_end = header_size + (long) spc_block_size * spc_count_;
	if ( size < spc_area_end )
		return "Corrupt SPC2 file: truncated SPC block area";

	// Per spec, layout is: SPC blocks | RAM blocks | extended metadata. The
	// smallest non-zero extended_data_ptr across all tracks (if any) marks the
	// boundary between RAM blocks and metadata; otherwise RAM blocks run to EOF.
	long ram_area_end = size;
	for ( int i = 0; i < spc_count_; i++ )
	{
		spc_block_t const* sb =
			(spc_block_t const*) (in + header_size + (long) i * spc_block_size);
		uint32_t ext_ptr = get_le32( sb->extended_data_ptr );
		if ( ext_ptr == 0 )
			continue;
		// ext_ptr must land within the file and leave room for at least one RAM block.
		if ( ext_ptr > (uint32_t) size || ext_ptr < (uint32_t) (spc_area_end + 256) )
			return "Corrupt SPC2 file: invalid extended_data_ptr";
		long ep = (long) ext_ptr;
		if ( ep < ram_area_end )
			ram_area_end = ep;
	}

	if ( ram_area_end - spc_area_end < 256 )
		return "Corrupt SPC2 file: truncated RAM blocks";

	ram_blocks_count_ = (ram_area_end - spc_area_end) / 256;

	set_track_count( spc_count_ );
	set_voice_count( Snes_Spc::voice_count );

	return 0;
}

// Emulation

void Sp2_Emu::set_tempo_( double t )
{
	apu.set_tempo( (int) (t * apu.tempo_unit) );
}

blargg_err_t Sp2_Emu::start_track_( int track )
{
	RETURN_ERR( Music_Emu::start_track_( track ) );

	resampler.clear();
	filter.clear();

	spc_block_t const* sb = get_spc_block( track );

	// Build an in-memory SPC file image to feed apu.load_spc().
	// Layout: 256-byte header + 64KB RAM + 128 DSP + 64 unused + 64 IPL
	memset( spc_buffer_, 0, sizeof(spc_buffer_) );

	memcpy( spc_buffer_, "SNES-SPC700 Sound File Data v0.30", 33 );
	spc_buffer_[0x21] = 0x1A;
	spc_buffer_[0x22] = 0x1A;
	spc_buffer_[0x23] = 0x1A;  // has ID666
	spc_buffer_[0x24] = 30;    // version minor

	// CPU registers at offset 0x25
	spc_buffer_[0x25] = sb->cpu_regs[0];  // PCL
	spc_buffer_[0x26] = sb->cpu_regs[1];  // PCH
	spc_buffer_[0x27] = sb->cpu_regs[2];  // A
	spc_buffer_[0x28] = sb->cpu_regs[3];  // X
	spc_buffer_[0x29] = sb->cpu_regs[4];  // Y
	spc_buffer_[0x2A] = sb->cpu_regs[5];  // PSW
	spc_buffer_[0x2B] = sb->cpu_regs[6];  // SP

	// Reconstruct 64KB RAM directly into the SPC buffer at offset 0x100
	RETURN_ERR( reconstruct_ram_into( sb, spc_buffer_ + 0x100 ) );

	// DSP registers at offset 0x10100, IPL ROM at offset 0x101C0
	memcpy( spc_buffer_ + 0x10100, sb->dsp_regs, 128 );
	memcpy( spc_buffer_ + 0x101C0, sb->ipl_rom,  64 );

	RETURN_ERR( apu.load_spc( spc_buffer_, sizeof(spc_buffer_) ) );

	apu.clear_echo();

	// Apply amplification from SP2 file (0x10000 = 1.0)
	uint32_t amplification = get_le32( sb->amplification );
	double amp_factor = (amplification > 0) ? (amplification / 65536.0) : 1.0;
	filter.set_gain( (int) (gain() * amp_factor * SPC_Filter::gain_unit) );

	// Re-apply user mute mask combined with this track's channel_disable.
	// remute_voices() routes through mute_voices_() which ORs in track_channel_disable_.
	track_channel_disable_ = sb->channel_disable;
	remute_voices();

	// Set fade based on track info; honor the SP2 fade_length when present
	track_info_t info;
	RETURN_ERR( track_info_( &info, track ) );

	if ( autoload_playback_limit() && info.length > 0 )
		set_fade( info.length, info.fade_length > 0 ? info.fade_length : 50 );

	return 0;
}

blargg_err_t Sp2_Emu::play_and_filter( long count, sample_t out [] )
{
	RETURN_ERR( apu.play( count, out ) );
	filter.run( out, count );
	return 0;
}

blargg_err_t Sp2_Emu::skip_( long count )
{
	if ( sample_rate() != native_sample_rate )
	{
		count = long (count * resampler.ratio()) & ~1;
		count -= resampler.skip_input( count );
	}

	if ( count > 0 )
	{
		RETURN_ERR( apu.skip( count ) );
		filter.clear();
	}

	// eliminate pop due to resampler
	const int resampler_latency = 64;
	sample_t buf [resampler_latency];
	return play_( resampler_latency, buf );
}

blargg_err_t Sp2_Emu::play_( long count, sample_t* out )
{
	if ( sample_rate() == native_sample_rate )
		return play_and_filter( count, out );

	long remain = count;
	while ( remain > 0 )
	{
		remain -= resampler.read( &out [count - remain], remain );
		if ( remain > 0 )
		{
			long n = resampler.max_write();
			RETURN_ERR( play_and_filter( n, resampler.buffer() ) );
			resampler.write( n );
		}
	}
	check( remain == 0 );
	return 0;
}
