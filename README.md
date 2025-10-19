dwarfstack
==========

[![build](https://github.com/ssbssa/dwarfstack/actions/workflows/build.yml/badge.svg?branch=master)](https://github.com/ssbssa/dwarfstack/actions/workflows/build.yml?query=branch%3Amaster)

dwarfstack aims to provide a simple way to get meaningful stacktraces.

Only windows executables (mingw-gcc) are supported.


It uses the debug information (if available) of the executables and converts
the addresses into source file/line information.

To get a better picture, check out the examples.


compilation:
------------
    make

installation:
-------------
    make PREFIX=/your/install/path


notes:
------
If you need a full stacktrace for release builds, you should consider
using -fno-omit-frame-pointer (and maybe even
-fno-optimize-sibling-calls / -fno-schedule-insns2).
Otherwise, depending on the situation (like gcc version), some frames
might be missing.


code signing:
-------------
Free code signing provided by [SignPath.io](https://about.signpath.io), certificate by [SignPath Foundation](https://signpath.org)
