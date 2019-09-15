// Simple audio waveform scope in a window, using SDL multimedia library

#ifndef AUDIO_SCOPE_H
#define AUDIO_SCOPE_H

#include "SDL.h"

#include <string>

class Audio_Scope {
public:
	typedef const char* error_t;
	
	// Initialize scope window of specified size. Height must be 256 or less.
	// If result is not an empty string, it is an error message
	std::string init( int width, int height );
	
	// Draw at most 'count' samples from 'in', skipping 'step' samples after
	// each sample drawn. Step can be less than 1.0.
	error_t draw( const short* in, long count, double step = 1.0 );
	
	Audio_Scope();
	~Audio_Scope();

	void set_caption( const char* caption );
	
private:
	typedef unsigned char byte;
	SDL_Window* window;
	SDL_Renderer* window_renderer;
	SDL_Surface* draw_buffer;
	SDL_Texture* surface_tex;
	byte* buf;
	int buf_size;
	int sample_shift;
	int v_offset;
	
	void render( short const* in, long count, long step );
};

#endif
