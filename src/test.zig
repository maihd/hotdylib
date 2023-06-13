const std = @import("std");
const HotDylib = @import("hotdylib");

test "basic" {
    const test_options = @import("test_options");
    const hot_dylib = try HotDylib.open(test_options.lib_path, "lib_entry");
    defer hot_dylib.close();

    while (try hot_dylib.update() != .init)
    {
    }

    try std.testing.expect(true);
}
