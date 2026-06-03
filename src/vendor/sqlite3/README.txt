SQLite3 vendor directory for Windows builds
==========================================

For Windows builds, the CMake configuration expects the SQLite amalgamation to
be placed in this directory.

Required files:
- sqlite3.c
- sqlite3.h

You can fetch an amalgamation release directly from sqlite.org. Example:

  curl -LO https://www.sqlite.org/2024/sqlite-amalgamation-3460000.zip

Then extract sqlite3.c and sqlite3.h into this directory:

  src/vendor/sqlite3/

Notes:
- Do not commit downloaded SQLite release archives here unless your workflow
  explicitly vendors third-party sources.
- Non-Windows builds continue to use the system SQLite3 development package.
