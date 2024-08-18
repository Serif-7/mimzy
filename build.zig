const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const exe = b.addExecutable(.{
        .name = "mimzy",
        .root_source_file = .{ .path = "zathura/main.c" },
        .target = target,
        .optimize = optimize,
    });

    // Add C source files
    const sources = [_][]const u8{
        "zathura/adjustment.c",
        "zathura/bookmarks.c",
        // ... add all other .c files here
    };
    exe.addCSourceFiles(&sources, &[_][]const u8{"-std=c17"});

    // Add include directories
    exe.addIncludePath(.{ .path = "." });

    // Add dependencies
    // Note: You'll need to handle external dependencies differently in Zig
    // This is just a placeholder to show where you'd add them
    exe.linkLibC();
    // exe.linkSystemLibrary("girara-gtk3");
    // exe.linkSystemLibrary("glib-2.0");
    // ... add other dependencies

    // Add defines
    exe.defineCMacro("GETTEXT_PACKAGE", "\"zathura\"");
    exe.defineCMacro("LOCALEDIR", "\"" ++ b.install_prefix ++ "/share/locale\"");
    exe.defineCMacro("ZATHURA_PLUGINDIR", "\"" ++ b.install_prefix ++ "/lib/zathura\"");

    // Install
    b.installArtifact(exe);

    // Add test step
    const run_cmd = exe.run();
    run_cmd.step.dependOn(b.getInstallStep());
    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    const run_step = b.step("run", "Run the app");
    run_step.dependOn(&run_cmd.step);
}
