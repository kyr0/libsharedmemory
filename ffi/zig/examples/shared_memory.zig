/// Example: Zig ↔ C++ shared memory interop
///
/// Demonstrates creating a shared memory segment from Zig,
/// writing data into it, then reading it back via a second handle —
/// exactly the same pattern a C++ (or Rust, or C) process could use
/// on the other end.
const std = @import("std");
const lsm = @import("lsm");

pub fn main() !void {
    const message = "Hello from Zig!";

    // Writer: create the shared memory segment and copy a message in
    const writer = try lsm.SharedMemory.create("zigExample", 256, true);
    defer writer.deinit();

    const wbuf = writer.data();
    @memcpy(wbuf[0..message.len], message);
    std.debug.print("Wrote   : {s}\n", .{message});

    // Reader: open the same segment and read the bytes back
    const reader = try lsm.SharedMemory.open("zigExample", 256, true);
    defer reader.close();

    const received = reader.data()[0..message.len];
    std.debug.print("Received: {s}\n", .{received});

    if (!std.mem.eql(u8, received, message)) {
        return error.RoundTripMismatch;
    }
    std.debug.print("OK\n", .{});
}
