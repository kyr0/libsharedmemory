/// Safe Zig wrapper around the libsharedmemory C FFI (lsm_c.h).
///
/// Provides an idiomatic Zig interface to create, open, read, and write
/// shared memory segments backed by the C++20 library.
const std = @import("std");
const c = @cImport({
    @cInclude("lsm_c.h");
});

pub const SharedMemory = struct {
    handle: *c.lsm_memory,
    len: usize,
    is_creator: bool,

    pub const Error = error{
        CreateFailed,
        OpenFailed,
    };

    /// Create a new shared memory segment.
    pub fn create(name: [*:0]const u8, buf_size: usize, persistent: bool) Error!SharedMemory {
        const ptr = c.lsm_create(name, buf_size, if (persistent) @as(c_int, 1) else @as(c_int, 0));
        if (ptr) |p| {
            return SharedMemory{ .handle = p, .len = buf_size, .is_creator = true };
        }
        return Error.CreateFailed;
    }

    /// Open an existing shared memory segment.
    pub fn open(name: [*:0]const u8, buf_size: usize, persistent: bool) Error!SharedMemory {
        const ptr = c.lsm_open(name, buf_size, if (persistent) @as(c_int, 1) else @as(c_int, 0));
        if (ptr) |p| {
            return SharedMemory{ .handle = p, .len = buf_size, .is_creator = false };
        }
        return Error.OpenFailed;
    }

    /// Returns a slice over the mapped region.
    pub fn data(self: SharedMemory) []u8 {
        const ptr: [*]u8 = @ptrCast(c.lsm_data(self.handle));
        return ptr[0..self.len];
    }

    /// Returns the size of the mapped region in bytes.
    pub fn size(self: SharedMemory) usize {
        return self.len;
    }

    /// Unlink the segment from the OS.
    pub fn destroy(self: SharedMemory) void {
        c.lsm_destroy(self.handle);
    }

    /// Close the mapping and free the handle.
    pub fn close(self: SharedMemory) void {
        c.lsm_close(self.handle);
        if (self.is_creator) {
            c.lsm_destroy(self.handle);
        }
        c.lsm_free(self.handle);
    }

    /// Convenience: close + destroy (for writers).
    pub fn deinit(self: SharedMemory) void {
        self.close();
    }
};

// --------------- tests ---------------

const testing = std.testing;

test "roundtrip" {
    const msg = "Hello from Zig!";
    const writer = try SharedMemory.create("zig_test_rt", 256, true);
    defer writer.deinit();

    const buf = writer.data();
    @memcpy(buf[0..msg.len], msg);

    const reader = try SharedMemory.open("zig_test_rt", 256, true);
    defer reader.close();

    try testing.expectEqualStrings(msg, reader.data()[0..msg.len]);
}

test "size_matches" {
    const mem = try SharedMemory.create("zig_test_sz", 512, true);
    defer mem.deinit();
    try testing.expectEqual(@as(usize, 512), mem.size());
}
