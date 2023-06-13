const std = @import("std");

export fn lib_entry(data: *anyopaque, new_state: c_int, old_state: c_int) ?*anyopaque {
    _ = data;
    _ = new_state;
    _ = old_state;

    std.debug.print("Hello world!\n", .{});
    return null;
}