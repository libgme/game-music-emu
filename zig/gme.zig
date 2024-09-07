const ErrMsg = ?[*:0]const u8;
pub const Reader = ?*const fn (*anyopaque, *anyopaque, c_int) callconv(.C) ErrMsg;
pub const Cleanup = ?*const fn (*anyopaque) callconv(.C) void;
pub const info_only = -1;

extern fn gme_identify_header(header: *const anyopaque) [*:0]const u8;
/// Determine likely game music type based on first four bytes of file.
/// Returns string containing proper file suffix (i.e. "NSF", "SPC", etc.)
/// or "" if file header is not recognized.
pub const identifyHeader = gme_identify_header;

pub const Error = error {
	NewEmu,
	FromFile,
	Open,
	StartTrack,
	Play,
	Seek,
	LoadM3u,
	TrackInfo,
	LoadFile,
	LoadData,
	LoadTracks,
	LoadCustom,
	LoadM3uData,
};

pub const Equalizer = struct {
	treble: f64,
	bass: f64,
	d2: f64,
	d3: f64,
	d4: f64,
	d5: f64,
	d6: f64,
	d7: f64,
	d8: f64,
	d9: f64,
};

pub const Type = opaque {
	const Self = @This();

	extern fn gme_new_emu(*const Self, sample_rate: c_int) ?*Emu;
	/// Create new emulator and set sample rate.
	/// Returns an error if out of memory.
	/// If you only need track information, pass `info_only` for sample_rate.
	pub fn emu(self: *const Self, sample_rate: i32) Error!*Emu {
		return gme_new_emu(self, @intCast(sample_rate)) orelse Error.NewEmu;
	}

	extern fn gme_new_emu_multi_channel(*const Self, sample_rate: c_int) ?*Emu;
	/// Create new multichannel emulator and set sample rate.
	/// Returns an error if out of memory.
	/// If you only need track information, pass `info_only` for sample_rate.
	pub fn emuMultiChannel(self: *const Self, sample_rate: i32) Error!*Emu {
		return gme_new_emu_multi_channel(self, @intCast(sample_rate)) orelse Error.NewEmu;
	}

	/// Convenience function for creating an info-only emulator.
	pub fn emuInfo(self: *const Self) Error!*Emu {
		return gme_new_emu(self, info_only) orelse Error.NewEmu;
	}

	extern fn gme_identify_extension(path_or_extension: [*:0]const u8) ?*const Self;
	/// Get corresponding music type for file path or extension passed in.
	pub const fromExtension = gme_identify_extension;

	extern fn gme_identify_file(path: [*:0]const u8, type_out: *?*const Self) ErrMsg;
	/// Get corresponding music type from a file's extension or header
	/// (if extension isn't recognized).
	/// Returns type, or null if unrecognized or error.
	pub fn fromFile(path: [*:0]const u8) Error!?*const Self {
		var type_out: ?*const Self = undefined;
		if (gme_identify_file(path, &type_out) != null)
			return Error.FromFile;
		return type_out;
	}

	extern fn gme_type_system(*const Self) [*:0]const u8;
	/// Name of game system for this music file type.
	pub const system = gme_type_system;

	extern fn gme_type_multitrack(*const Self) c_int;
	/// True if this music file type supports multiple tracks.
	pub fn isMultiTrack(self: *const Self) bool {
		return (gme_type_multitrack(self) != 0);
	}

	extern fn gme_type_extension(music_type: *const Self) [*:0]const u8;
	/// Get typical file extension for a given music type.  This is not a replacement
	/// for a file content identification library (but see `identifyHeader()`).
	pub const extension = gme_type_extension;

	extern fn gme_fixed_track_count(*const Self) c_int;
	/// Return the fixed track count of an emu file type.
	pub fn trackCount(self: *const Self) u32 {
		return @intCast(gme_fixed_track_count(self));
	}
};

pub extern const gme_ay_type: *const Type;
pub extern const gme_gbs_type: *const Type;
pub extern const gme_gym_type: *const Type;
pub extern const gme_hes_type: *const Type;
pub extern const gme_kss_type: *const Type;
pub extern const gme_nsf_type: *const Type;
pub extern const gme_nsfe_type: *const Type;
pub extern const gme_sap_type: *const Type;
pub extern const gme_spc_type: *const Type;
pub extern const gme_vgm_type: *const Type;
pub extern const gme_vgz_type: *const Type;

pub const Info = extern struct {
	const Self = @This();

	length: c_int,
	intro_length: c_int,
	loop_length: c_int,
	play_length: c_int,
	fade_length: c_int,
	i5: c_int,
	i6: c_int,
	i7: c_int,
	i8: c_int,
	i9: c_int,
	i10: c_int,
	i11: c_int,
	i12: c_int,
	i13: c_int,
	i14: c_int,
	i15: c_int,
	system: [*:0]const u8,
	game: [*:0]const u8,
	song: [*:0]const u8,
	author: [*:0]const u8,
	copyright: [*:0]const u8,
	comment: [*:0]const u8,
	dumper: [*:0]const u8,
	s7: [*:0]const u8,
	s8: [*:0]const u8,
	s9: [*:0]const u8,
	s10: [*:0]const u8,
	s11: [*:0]const u8,
	s12: [*:0]const u8,
	s13: [*:0]const u8,
	s14: [*:0]const u8,
	s15: [*:0]const u8,

	extern fn gme_free_info(*Self) void;
	/// Frees track information.
	pub const free = gme_free_info;
};

pub const Emu = opaque {
	const Self = @This();

	extern fn gme_delete(*Self) void;
	/// Finish using emulator and free memory.
	pub const delete = gme_delete;

	extern fn gme_clear_playlist(*Self) void;
	/// Clear any loaded m3u playlist and any internal playlist
	/// that the music format supports (NSFE for example).
	pub const clearPlaylist = gme_clear_playlist;

	extern fn gme_set_stereo_depth(*Self, depth: f64) void;
	/// Adjust stereo echo depth, where 0.0 = off and 1.0 = maximum.
	/// Has no effect for GYM, SPC, and Sega Genesis VGM music.
	pub const setStereoDepth = gme_set_stereo_depth;

	extern fn gme_set_tempo(*Self, tempo: f64) void;
	/// Adjust song tempo, where 1.0 = normal, 0.5 = half speed, 2.0 = double speed.
	/// Track length as returned by `trackInfo()` assumes a tempo of 1.0.
	pub const setTempo = gme_set_tempo;

	extern fn gme_equalizer(*const Self, out: *Equalizer) void;
	/// Get current frequency equalizater parameters.
	pub const equalizer = gme_equalizer;

	extern fn gme_set_equalizer(*Self, eq: *const Equalizer) void;
	/// Change frequency equalizer parameters.
	pub const setEqualizer = gme_set_equalizer;

	extern fn gme_type(*const Self) *const Type;
	/// Type of this emulator.
	pub const toType = gme_type;

	extern fn gme_set_user_data(*Self, new_user_data: *anyopaque) void;
	/// Set pointer to data you want to associate with this emulator.
	/// You can use this for whatever you want.
	pub const setUserData = gme_set_user_data;

	extern fn gme_user_data(*const Self) ?*anyopaque;
	/// Get pointer to user data associated with this emulator.
	pub const userData = gme_user_data;

	extern fn gme_set_user_cleanup(*Self, func: Cleanup) void;
	/// Register cleanup function to be called when deleting emulator,
	/// or `null` to clear it. Passes user_data to cleanup function.
	pub const setUserCleanup = gme_set_user_cleanup;

	extern fn gme_open_file(
		path: [*:0]const u8, out: *?*Self, srate: c_int) ErrMsg;
	/// Returns an emulator with game music file/data loaded into it.
	pub fn fromFile(path: [*:0]const u8, srate: i32) Error!*Self {
		var self: ?*Self = null;
		if (gme_open_file(path, &self, @intCast(srate)) != null)
			return Error.Open;
		return self.?;
	}

	extern fn gme_open_data(
		*const anyopaque, size: c_long, out: *?*Self, srate: c_int) ErrMsg;
	/// Same as `fromFile()`, but uses file data already in memory. Makes copy of data.
	pub fn fromData(data: []const anyopaque, srate: i32) Error!*Self {
		var self: ?*Self = null;
		if (gme_open_data(data.ptr, data.len, &self, @intCast(srate)) != null)
			return Error.Open;
		return self.?;
	}

	extern fn gme_track_count(*const Self) c_int;
	/// Number of tracks available.
	pub fn trackCount(self: *const Self) u32 {
		return @intCast(gme_track_count(self));
	}

	extern fn gme_start_track(*Self, index: c_int) ErrMsg;
	/// Start a track, where 0 is the first track.
	pub fn startTrack(self: *Self, index: u32) Error!void {
		if (gme_start_track(self, @intCast(index)) != null)
			return Error.StartTrack;
	}

	extern fn gme_play(*Self, count: c_int, out: [*]c_short) ErrMsg;
	/// Generate 16-bit signed samples into `out`. Output is in stereo.
	pub fn play(self: *Self, out: []i16) Error!void {
		if (gme_play(self, @intCast(out.len), @ptrCast(out.ptr)) != null)
			return Error.Play;
	}

	extern fn gme_set_fade_msecs(*Self, start_msec: c_int, length_msecs: c_int) void;
	/// Set fade-out start time and duration. Once fade ends `trackEnded()` returns true.
	/// Fade time can be changed while track is playing.
	pub fn setFade(self: *Self, start: i32, length: u32) void {
		gme_set_fade_msecs(self, @intCast(start), @intCast(length));
	}

	extern fn gme_set_fade(*Self, start_msec: c_int) void;
	/// Set time to start fading track out. Once fade ends `trackEnded()` returns true.
	/// Fade time can be changed while track is playing.
	pub fn setFadeStart(self: *Self, start: u32) void {
		gme_set_fade(self, @intCast(start));
	}

	extern fn gme_set_autoload_playback_limit(*Self, do_autoload_limit: c_int) void;
	/// If true, then automatically load track length
	/// metadata (if present) and terminate playback once the track length has been
	/// reached. Otherwise playback will continue for an arbitrary period of time
	/// until a prolonged period of silence is detected.
	///
	/// Not all individual emulators support this setting.
	///
	/// By default, playback limits are loaded and applied.
	pub fn setAutoloadPlaybackLimit(self: *Self, state: bool) void {
		gme_set_autoload_playback_limit(self, @intFromBool(state));
	}

	extern fn gme_autoload_playback_limit(*const Self) c_int;
	/// Get the state of autoload playback limit. See `setAutoloadPlaybackLimit()`.
	pub fn autoloadPlaybackLimit(self: *const Self) bool {
		return (gme_autoload_playback_limit(self) != 0);
	}

	extern fn gme_track_ended(*const Self) c_int;
	/// True if a track has reached its end.
	pub fn trackEnded(self: *const Self) bool {
		return (gme_track_ended(self) != 0);
	}

	extern fn gme_tell(*const Self) c_int;
	/// Number of milliseconds (1000 = one second) played since beginning of track.
	pub fn tell(self: *const Self) u32 {
		return @intCast(gme_tell(self));
	}

	extern fn gme_tell_samples(*const Self) c_int;
	/// Number of samples generated since beginning of track.
	pub fn tellSamples(self: *const Self) u32 {
		return @intCast(gme_tell_samples(self));
	}

	extern fn gme_seek(*Self, msec: c_int) ErrMsg;
	/// Seek to new time in track. Seeking backwards or far forward can take a while.
	pub fn seek(self: *Self, msec: u32) Error!void {
		if (gme_seek(self, @intCast(msec)) != null)
			return Error.Seek;
	}

	extern fn gme_seek_samples(*Self, n: c_int) ErrMsg;
	/// Equivalent to restarting track then skipping n samples
	pub fn seekSamples(self: *Self, n: u32) Error!void {
		if (gme_seek_samples(self, @intCast(n)) != null)
			return Error.Seek;
	}

	extern fn gme_warning(*Self) ?[*:0]const u8;
	/// Most recent warning string, or null if none.
	/// Clears current warning after returning.
	/// Warning is also cleared when loading a file and starting a track.
	pub const warning = gme_warning;

	extern fn gme_load_m3u(*Self, path: [*:0]const u8) ErrMsg;
	/// Load m3u playlist file (must be done after loading music).
	pub fn loadM3u(self: *Self, path: [*:0]const u8) Error!void {
		if (gme_load_m3u(self, path) != null)
			return Error.LoadM3u;
	}

	extern fn gme_track_info(*const Self, out: *?*Info, track: c_int) ErrMsg;
	/// Gets information for a particular track (length, name, author, etc.).
	/// Must be freed after use.
	pub fn trackInfo(self: *const Self, track: u32) Error!*Info {
		var info: ?*Info = null;
		if (gme_track_info(self, &info, @intCast(track)) != null)
			return Error.TrackInfo;
		return info.?;
	}

	extern fn gme_ignore_silence(*Self, ignore: c_int) void;
	/// Disable automatic end-of-track detection and skipping of silence at beginning.
	pub fn ignoreSilence(self: *Self, ignore: bool) void {
		gme_ignore_silence(self, @intFromBool(ignore));
	}

	extern fn gme_voice_count(*const Self) c_int;
	/// Number of voices used by currently loaded file.
	pub fn voiceCount(self: *const Self) u32 {
		return @intCast(gme_voice_count(self));
	}

	extern fn gme_voice_name(*const Self, i: c_int) [*:0]const u8;
	/// Name of voice i, from 0 to `voiceCount()` - 1
	pub fn voiceName(self: *const Self, i: u32) [*:0]const u8 {
		return gme_voice_name(self, @intCast(i));
	}

	extern fn gme_mute_voice(*Self, index: c_int, mute: c_int) void;
	pub fn muteVoice(self: *Self, index: u32, mute: bool) void {
		gme_mute_voice(self, @intCast(index), @intFromBool(mute));
	}

	extern fn gme_mute_voices(*Self, muting_mask: c_uint) void;
	/// Mute/unmute voice i, where voice 0 is first voice.
	pub fn muteVoices(self: *Self, muting_mask: u32) void {
		gme_mute_voices(self, @intCast(muting_mask));
	}

	extern fn gme_disable_echo(*Self, disable: c_int) void;
	/// Disable/Enable echo effect for SPC files.
	pub fn disableEcho(self: *Self, disable: bool) void {
		gme_disable_echo(self, @intFromBool(disable));
	}

	extern fn gme_enable_accuracy(*Self, enabled: c_int) void;
	/// Enables/disables most accurate sound emulation options.
	pub fn enableAccuracy(self: *Self, enabled: bool) void {
		gme_enable_accuracy(self, @intFromBool(enabled));
	}

	extern fn gme_multi_channel(*const Self) c_int;
	/// whether the pcm output retrieved by gme_play() will have all 8 voices
	/// rendered to their individual stereo channel or (if false) these voices
	/// get mixed into one single stereo channel.
	pub fn isMultiChannel(self: *const Self) bool {
		return (gme_multi_channel(self) != 0);
	}

	extern fn gme_load_file(*Self, path: [*:0]const u8) ErrMsg;
	/// Load music file into emulator.
	pub fn loadFile(self: *Self, path: [*:0]const u8) Error!void {
		if (gme_load_file(self, path) != null)
			return Error.LoadFile;
	}

	extern fn gme_load_data(*Self, data: *const anyopaque, size: c_long) ErrMsg;
	/// Load music file from memory into emulator. Makes a copy of data passed.
	pub fn loadData(self: *Self, data: []const anyopaque) Error!void {
		if (gme_load_data(self, data.ptr, data.len) != null)
			return Error.LoadData;
	}

	extern fn gme_load_tracks(*Self,
		data: [*]const u8, sizes: [*]c_long, count: c_uint) ErrMsg;
	/// Load multiple single-track music files from memory into emulator.
	pub fn loadTracks(self: *Self, data: [*]const u8, sizes: []usize) Error!void {
		const result = gme_load_tracks(self,
			data, @ptrCast(sizes.ptr), @intCast(sizes.len));
		if (result != null)
			return Error.LoadTracks;
	}

	extern fn gme_load_custom(*Self,
		Reader, file_size: c_long, your_data: *anyopaque) ErrMsg;
	/// Load music file using custom data reader function that will be called to
	/// read file data. Most emulators load the entire file in one read call.
	pub fn loadCustom(self: *Self, func: Reader, data: []anyopaque) Error!void {
		if (gme_load_custom(self, func, data.len, data.ptr) != null)
			return Error.LoadCustom;
	}

	extern fn gme_load_m3u_data(*Self, data: *const anyopaque, size: c_long) ErrMsg;
	/// Load m3u playlist file from memory (must be done after loading music).
	pub fn loadM3uData(self: *Self, data: []const anyopaque) Error!void {
		if (gme_load_m3u_data(self, data.ptr, data.len) != null)
			return Error.LoadM3uData;
	}
};
