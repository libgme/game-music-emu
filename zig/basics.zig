//! opens a game music file and records 10 seconds to "out.wav"
const std = @import("std");
const gme = @import("gme");

const chan_count = 2;
const header_size = 0x2C;

pub fn main() !void {
	var args = std.process.args();
	_ = args.skip();
	const filename = args.next() orelse "test.nsf";
	const track = if (args.next()) |arg| try std.fmt.parseInt(u32, arg, 10) else 0;

	// Open music file in new emulator
	const sample_rate = 48000;
	const emu = try gme.Emu.fromFile(filename, sample_rate);
	defer emu.delete();

	// Start track
	try emu.startTrack(track);

	// Create a wave file
	const file = try std.fs.cwd().createFile("out.wav", .{});
	defer file.close();
	const writer = file.writer();

	// Create buffer
	const buf_size = 4000;
	var buf: [buf_size]i16 = undefined;
	const bytes = @as([*]u8, @ptrCast(&buf))[0..buf_size * @sizeOf(i16)];

	// Reserve space for header
	try writer.writeAll(bytes[0..header_size]);

	// Record 10 seconds of track
	const duration_secs = 10;
	const total_samples = duration_secs * sample_rate * chan_count;
	while (emu.tellSamples() < total_samples) {
		try emu.play(&buf);
		try writer.writeAll(bytes);
	}

	// Write the header
	try file.seekTo(0);
	const ds = total_samples * @sizeOf(i16);
	const rs = header_size - 8 + ds;
	const frame_size = chan_count * @sizeOf(i16);
	const bytes_per_second = sample_rate * frame_size;
	try writer.writeAll("RIFF");
	try writer.writeInt(u32, rs, .little);
	try writer.writeAll("WAVE");
	try writer.writeAll("fmt ");
	try writer.writeAll(&.{
		0x10,0,0,0, // size of fmt chunk
		1,0,        // uncompressed format
		chan_count,0,
	});
	try writer.writeInt(u32, sample_rate, .little);
	try writer.writeInt(u32, bytes_per_second, .little);
	try writer.writeAll(&.{
		frame_size,0,
		@bitSizeOf(i16),0,
	});
	try writer.writeAll("data");
	try writer.writeInt(u32, ds, .little);
}
