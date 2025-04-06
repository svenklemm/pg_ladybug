Static C code checker for postgres API

pg_ladybug checks C code using the postgres API for problematic patterns. It is implemented as a clang-tidy plugin.

pg_ladybug checks for the following patterns:

- passing large int into Bitmapset
