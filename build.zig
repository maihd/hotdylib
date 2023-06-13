const std = @import("std");

// Although this function looks imperative, note that its job is to
// declaratively construct a build graph that will be executed by an external
// runner.
pub fn build(b: *std.Build) void {
    // Standard target options allows the person running `zig build` to choose
    // what target to build for. Here we do not override the defaults, which
    // means any target is allowed, and the default is native. Other options
    // for restricting supported target set are available.
    const target = b.standardTargetOptions(.{});

    // Standard optimization options allow the person running `zig build` to select
    // between Debug, ReleaseSafe, ReleaseFast, and ReleaseSmall. Here we do not
    // set a preferred release mode, allowing the user to decide how to optimize.
    const optimize = b.standardOptimizeOption(.{});

    const lib = b.addStaticLibrary(.{
        .name = "hotdylib",
        // In this case the main source file is merely a path, however, in more
        // complicated build scripts, this could be a generated file.
        .root_source_file = .{ .path = "src/main.zig" },
        .target = target,
        .optimize = optimize,
    });
    try link(b, lib);

    // This declares intent for the library to be installed into the standard
    // location when the user invokes the "install" step (the default step when
    // running `zig build`).
    b.installArtifact(lib);

    const guest_lib = b.addSharedLibrary(.{
        .name = "hotdylib_guest",
        .root_source_file = .{ .path = "src/guest.zig" },
        .target = target,
        .optimize = optimize,
    });
    b.installArtifact(guest_lib);

    // Creates a step for unit testing. This only builds the test executable
    // but does not run it.
    const main_tests = b.addTest(.{
        .root_source_file = .{ .path = "src/test.zig" },
        .target = target,
        .optimize = optimize,
    });
    try link(b, main_tests);

    const test_options = b.addOptions();
    test_options.addOption([]const u8, "lib_path", thisDir() ++ "/zig-out/lib/hotdylib_guest.dll");
    main_tests.addOptions("test_options", test_options);

    const run_main_tests = b.addRunArtifact(main_tests);

    // This creates a build step. It will be visible in the `zig build --help` menu,
    // and can be selected like this: `zig build test`
    // This will evaluate the `test` step rather than the default, which is "install".
    const test_step = b.step("test", "Run library tests");
    test_step.dependOn(&run_main_tests.step);
}

inline fn thisDir() []const u8 {
    return comptime std.fs.path.dirname(@src().file) orelse ".";
}

pub fn link(b: *std.Build, step: *std.Build.CompileStep) !void {
    step.linkLibC();
    step.addIncludePath(thisDir());
    step.addCSourceFile(thisDir() ++ "/HotDylib.c", &.{
        // "-std=gnu99",
        // "-DPLATFORM_DESKTOP",
        // "-DGL_SILENCE_DEPRECATION=199309L",
        "-fno-sanitize=undefined", // https://github.com/raysan5/raylib/issues/1891
    });

    const module = b.createModule(.{
        .source_file = .{ .path = "src/main.zig" }
    });
    step.addModule("hotdylib", module);
}