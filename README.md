OVERVIEW
========

This repository contains `pkgutils`, a set of utilities like
`pkgadd(1)`, `pkgrm(8)`, and `pkginfo(1)` used for managing software
packages: adding, removing, and gathering basic information.

This distribution is a fork of CRUX's `pkgutils` as of commit
`9ca0da6` (Sat Nov 17 2018) with the following differences:
  * Organized the code into a library (`libpkgutils`) and utilities
  * GNU-style options/help/usage
  * Better GNU Coding Standards support
  * Manual pages in `mdoc(7)` format
  * Split `pkgadd(8)` manual page into `pkgadd(8)` and `pkgadd.conf(5)`
  * `zstd` packages support
  * Vim syntax highlighting for `pkgadd.conf` file
  * Optional support for preserving ACLs & xattrs by `pkgadd(8)`

See git log for complete/further differences.

The original sources can be downloaded from:
  * https://git.crux.nu/tools/pkgutils.git


REQUIREMENTS
============

Build time
----------
  * C++11 compiler (GCC 4.8.1 and later, Clang 3.3 and later)
  * POSIX `sh(1p)`, `make(1p)` and "mandatory utilities"
  * `pkg-config(1)` is optional, for static linking
  * `libarchive(3)` to unpack archive files

Also, see [rejmerge][1], a utility that merges files rejected by
`pkgadd(8)` during package upgrades.

[1]: https://github.com/zeppe-lin/rejmerge


INSTALL
=======

To build and install this package, run the following shell commands:

```sh
make && make install
```

For static linking, you need `pkg-config(1)` and should run `make` as
follows:

```sh
make LDFLAGS="-static `pkg-config --static --libs libarchive`"
```

See the `config.mk` file for configuration parameters and the
`src/pathnames.h` file for absolute filenames and settings that pkgutils
uses for various defaults.


DOCUMENTATION
=============

Online documentation
--------------------

Refer to the human-readable man pages located in the `/man` directory.


LICENSE
=======

pkgutils is licensed through the GNU General Public License v2 or
later <https://gnu.org/licenses/gpl.html>.
Read the COPYING file for copying conditions.
Read the COPYRIGHT file for copyright notices.
