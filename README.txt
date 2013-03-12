dwarfstack aims to provide a simple way to get meaningful stacktraces.

Only windows executables (mingw-gcc) are supported.


It uses the debug information (if available) in the executables and converts
the addresses into source file/line information.

To get a better picture, check out the examples.


compilation:
$ make

installation:
$ make PREFIX=/your/install/path


notes:
If you need a full stacktrace for release builds, you should consider
using -fno-omit-frame-pointer (and maybe even -fno-optimize-sibling-calls).
Otherwise, depending on the situation (like gcc version), some frames
might be missing.
