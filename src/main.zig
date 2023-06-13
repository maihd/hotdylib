const std = @import("std");
const c = @cImport({
    @cInclude("HotDylib.h");
});

const HotDylib = @This();

pub const State = enum {
    none,
    init,
    quit,
    unload,
    reload,
    locked,
    failed,
};

pub const Error = error {
    None,
    Abort,
    Float,
    IllCode,
    SysCall,
    MisAlign,
    NotFound,
    SegFault,
    OutOfBounds,
    OutOfMemory,
    StackOverflow,
};

inline fn stateFromC(state: c.HotDylibState) State {
    return switch (state) {
        c.HOTDYLIB_NONE => .none,
        c.HOTDYLIB_INIT => .init,
        c.HOTDYLIB_QUIT => .quit,
        c.HOTDYLIB_UNLOAD => .unload,
        c.HOTDYLIB_RELOAD => .reload,
        c.HOTDYLIB_LOCKED => .locked,
        c.HOTDYLIB_FAILED => .failed,
        else => .none,
    };
}

inline fn errorFromC(@"error": c.HotDylibError) Error {
    return switch (@"error") {
        c.HOTDYLIB_ERROR_NONE => Error.None,
        c.HOTDYLIB_ERROR_ABORT => Error.Abort,
        c.HOTDYLIB_ERROR_FLOAT => Error.Float,
        c.HOTDYLIB_ERROR_ILLCODE => Error.IllCode,
        c.HOTDYLIB_ERROR_SYSCALL => Error.SysCall,
        c.HOTDYLIB_ERROR_MISALIGN => Error.MisAlign,
        c.HOTDYLIB_ERROR_OUTBOUNDS => Error.SegFault,
        c.HOTDYLIB_ERROR_STACKOVERFLOW => Error.StackOverflow,
        else => Error.None,
    };
}

pub fn open(path: []const u8, entry_name: []const u8) Error!*HotDylib {
    const path_c = @ptrCast([*]const u8, path);
    const entry_name_c = @ptrCast([*]const u8, entry_name);
    const result_c = c.HotDylibOpen(path_c, entry_name_c);
    if (result_c == null) {
        return error.OutOfMemory;
    }

    if (result_c.*.@"error" != c.HOTDYLIB_ERROR_NONE) {
        return errorFromC(result_c.*.@"error");
    } 

    return fromCPtr(result_c);
}

pub fn close(self: *HotDylib) void {
    c.HotDylibFree(self.toCPtr());
}

pub fn update(self: *HotDylib) Error!State {
    const hotdylib_c = self.toCPtr();
    const state = stateFromC(c.HotDylibUpdate(hotdylib_c));
    if (state == .failed) {
        return errorFromC(hotdylib_c.@"error");
    }
    return state;
}

pub fn getSymbol(self: *HotDylib, symbol_name: []const u8) !*anyopaque {
    const hotdylib_c = self.toCPtr();
    const symbol_name_c = @ptrCast([*]const u8, symbol_name);
    const symbol = stateFromC(c.HotDylibGetSymbol(hotdylib_c, symbol_name_c));

    if (symbol == null) {
        return error.NotFound;
    }

    return symbol;
}

pub fn getErrorMessage(self: *HotDylib) []const u8 {
    const hotdylib_c = self.toCPtr();
    const error_message_c = c.HotDylibGetError(hotdylib_c);
    const error_message = std.mem.span(@ptrCast([*:0]const u8, error_message_c));
    return error_message;
}

fn toCPtr(self: *HotDylib) *c.HotDylib {
    return @ptrCast(*c.HotDylib, @alignCast(@alignOf(*c.HotDylib), self));
}

fn fromCPtr(ptr: *c.HotDylib) *HotDylib {
    return @ptrCast(*HotDylib, @alignCast(@alignOf(*HotDylib), ptr));
}
