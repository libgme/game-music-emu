// SPC2 (compressed SPC collection) music file emulator

// Game_Music_Emu https://github.com/libgme/game-music-emu
#ifndef SP2_EMU_H
#define SP2_EMU_H

#include "Fir_Resampler.h"
#include "Music_Emu.h"
#include "Snes_Spc.h"
#include "Spc_Filter.h"

class Sp2_Emu : public Music_Emu {
public:
	// The Super Nintendo hardware samples at 32kHz
	enum { native_sample_rate = 32000 };

	// SPC2 file header (16 bytes)
	enum { header_size = 16 };
	struct header_t
	{
		char tag [4];          // "KSPC"
		byte eof_char;         // 0x1A
		byte version_major;    // 0x01
		byte version_minor;    // 0x00
		byte spc_count [2];    // little-endian uint16
		byte reserved [7];     // must be zeros
	};

	// SPC data block within SP2 file (1024 bytes per track)
	enum { spc_block_size = 1024 };
	struct spc_block_t
	{
		byte block_map [512];      // 256 x uint16 LE offsets into RAM blocks
		byte dsp_regs [128];       // DSP register data
		byte ipl_rom [64];         // IPL ROM
		byte cpu_regs [7];         // PCL, PCH, A, X, Y, PSW, SP
		byte channel_disable;      // channel enable bits
		byte date [4];             // BCD format MM DD YY YY
		byte play_length [4];      // uint32 LE, 1/64000th seconds
		byte fade_length [4];      // uint32 LE, 1/64000th seconds
		byte amplification [4];    // uint32 LE, 0x10000 = 1.0
		byte emulator;             // emulator used for dump
		byte ost_disk;             // OST disk number
		byte ost_track [2];        // OST track number
		byte copyright_year [2];   // uint16 LE
		byte boot_code_ptr [2];    // uint16 LE (v1.3: for hardware upload)
		byte reserved [32];        // zeros
		char song_title [32];
		char game_title [32];
		char artist [32];
		char dumper [32];
		char comments [32];
		char ost_title [32];
		char publisher [32];
		char original_filename [28];
		byte extended_data_ptr [4]; // uint32 LE file offset
	};

	// Verify header is valid SPC2 format
	static blargg_err_t check_header( header_t const& );

	// Prevents channels and global volumes from being phase-negated
	void disable_surround( bool disable = true );

	static gme_type_t static_type() { return gme_sp2_type; }

public:
	Sp2_Emu();
	~Sp2_Emu();
protected:
	blargg_err_t load_mem_( byte const*, long );
	blargg_err_t track_info_( track_info_t*, int track ) const;
	blargg_err_t set_sample_rate_( long );
	blargg_err_t start_track_( int );
	blargg_err_t play_( long, sample_t* );
	blargg_err_t skip_( long );
	void mute_voices_( int );
	void disable_echo_( bool disable );
	void set_tempo_( double );
	void enable_accuracy_( bool );
private:
	byte const* file_data;
	long file_size;
	int spc_count_;
	Fir_Resampler<24> resampler;
	SPC_Filter filter;
	Snes_Spc apu;

	// Reconstructed 64KB RAM for current track
	byte ram_ [0x10000];

	// Helper methods
	spc_block_t const* get_spc_block( int track ) const;
	byte const* get_ram_blocks() const;
	long ram_blocks_offset() const;
	blargg_err_t reconstruct_ram( int track );

	blargg_err_t play_and_filter( long count, sample_t out [] );
};

inline void Sp2_Emu::disable_surround( bool b ) { apu.disable_surround( b ); }

#endif
