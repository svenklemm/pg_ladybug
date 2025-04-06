## pg_ladybug
<p align="center">
  <a href="https://github.com/timescale/pgspot/blob/main/LICENSE"><img alt="License: PostgreSQL" src="https://img.shields.io/github/license/timescale/pgspot"></a>
</p>

Static C code checker for postgres API

pg_ladybug checks C code using the postgres API for problematic patterns. It is implemented as a clang-tidy plugin.

pg_ladybug checks for the following patterns:

- passing large int into Bitmapset


