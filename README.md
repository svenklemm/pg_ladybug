## pg_ladybug
<p align="center">
  <a href="https://github.com/timescale/pgspot/blob/main/LICENSE"><img alt="License: PostgreSQL" src="https://img.shields.io/github/license/timescale/pgspot"></a>
</p>

Static C code checker for postgres API

pg_ladybug checks C code using the postgres API for problematic patterns. It is implemented as a clang-tidy plugin.

pg_ladybug checks for the following patterns:

- passing large int into Bitmapset

## Installation

```
apt-get install llvm-19 llvm-19-dev clang-19 libclang-19-dev clang-tidy-19
git clone https://github.com/svenklemm/pg_ladybug
cd pg_ladybug
cmake -S . -B build
make -C build
```

## Requirements

- clang
- clang-tidy
- llvm

### Usage

Verify checks are present:
```
clang-tidy --load build/lib/libPostgresCheck.so --checks='-*,postgres-*' --list-checks
```

```
% clang-tidy --load ~/projects/pg_ladybug/build/lib/libPostgresCheck.so --checks='postgres-*' file.c
2 warnings and 2 errors generated.
Error while processing file.c.
file.c:224:30: error: potential wrong function argument. bms_add_member called with datatype Oid [postgres-bitmapset-member]
  224 |                 uncompressed_attrs_found = bms_add_member(uncompressed_attrs_found, ladybug_test);
      |                                            ^
file.c:227:30: error: potential wrong function argument. bms_add_member called with datatype Oid [postgres-bitmapset-member]
  227 |                 uncompressed_attrs_found = bms_add_member(uncompressed_attrs_found, ladybug_test);
      |                                            ^
Suppressed 2 warnings (2 in non-user code).
Use -header-filter=.* to display errors from all non-system headers. Use -system-headers to display errors from system headers as well.
```

