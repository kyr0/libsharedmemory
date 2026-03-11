const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    // Root of the libsharedmemory repo (two levels up from ffi/zig/)
    const root = b.path("../..");

    // --- Zig module that wraps the C API ---
    const lsm_mod = b.addModule("lsm", .{
        .root_source_file = b.path("src/lsm.zig"),
        .target = target,
        .optimize = optimize,
    });
    lsm_mod.addIncludePath(root.path(b, "example"));
    lsm_mod.addIncludePath(root.path(b, "include"));
    lsm_mod.addCSourceFile(.{
        .file = root.path(b, "example/lsm_c.cpp"),
        .flags = &.{"-std=c++20"},
    });
    lsm_mod.link_libc = true;
    lsm_mod.link_libcpp = true;

    // --- runnable example ---
    const exe = b.addExecutable(.{
        .name = "shared_memory",
        .root_module = b.createModule(.{
            .root_source_file = b.path("examples/shared_memory.zig"),
            .target = target,
            .optimize = optimize,
            .imports = &.{
                .{ .name = "lsm", .module = lsm_mod },
            },
        }),
    });

    b.installArtifact(exe);

    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());

    const run_step = b.step("run", "Run the shared_memory example");
    run_step.dependOn(&run_cmd.step);

    // --- tests ---
    const tests = b.addTest(.{
        .root_module = lsm_mod,
    });

    const run_tests = b.addRunArtifact(tests);
    const test_step = b.step("test", "Run unit tests");
    test_step.dependOn(&run_tests.step);
}
