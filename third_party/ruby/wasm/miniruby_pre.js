// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
//
// Build-time shim for miniruby, which is the only part of the interpreter that
// runs on the build machine rather than in a browser.
//
// miniruby is linked with `-sNODERAWFS=1` so that it reads the source tree and
// writes `builtin_binary.rbbin` directly, without either side being copied
// through the module's in-memory filesystem.  Two things in that mode do not
// survive Windows:
//
//   1. `fstat` assumes every stream has a backing node, but the three stdio
//      streams are ttys and have none.  `ruby_sysinit` fstats all three to
//      decide whether it is attached to a terminal, so the run dies before the
//      first line of Ruby is parsed:
//
//          TypeError: Cannot read properties of undefined (reading 'node_ops')
//
//      A character device is reported for those three instead.
//
//   2. `getcwd` fails for any path with a drive letter in it -- `C:\...` is not
//      a path the C library can walk, so `Dir.pwd` raises
//      `Errno::ENOENT: No such file or directory - getcwd`.  Node resolves a
//      path without a drive against the current drive, so dropping the `C:` is
//      both what the C side can digest and the same location.
Module["preRun"] = Module["preRun"] || [];
Module["preRun"].push(function () {
  var fstat = FS.fstat;
  FS.fstat = function (fd) {
    var stream = FS.getStream(fd);
    if (stream && !stream.node) {
      return {
        dev: 0,
        ino: 0,
        mode: 0o20666,
        nlink: 1,
        uid: 0,
        gid: 0,
        rdev: 0,
        size: 0,
        blksize: 4096,
        blocks: 0,
        atime: new Date(0),
        mtime: new Date(0),
        ctime: new Date(0),
      };
    }
    return fstat.call(FS, fd);
  };

  FS.cwd = function () {
    return process.cwd().replace(/^[A-Za-z]:/, "").replace(/\\/g, "/");
  };
});
