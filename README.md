OVERVIEW
========

This repository contains `pkgutils`, a set of utilities like
`pkgadd(1)`, `pkgrm(8)`, and `pkginfo(1)` used for managing software
packages: adding, removing, and gathering basic information.

This distribution is a fork of CRUX `pkgutils` as of commit 9ca0da6
(Sat Nov 17 2018) with the following differences:
  * Organized the code into a library (`libpkgutils`) and utilities
  * GNU-style options/help/usage
  * Better GNU Coding Standards support
  * Manual pages in `scdoc(5)` format
  * Split `pkgadd(8)` manual page into `pkgadd(8)` and
    `pkgadd.conf(5)`
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
  * C++11 compiler (GCC 4.8.1+, Clang 3.3+)
  * POSIX `sh(1p)`, `make(1p)` and "mandatory utilities"
  * `pkg-config(1)` is optional, for static linking
  * `libarchive(3)` to unpack archive files
  * `scdoc(1)` to build man pages

Also, see [rejmerge][1], a utility that merges files rejected by
`pkgadd(8)` during package upgrades.

[1]: https://github.com/zeppe-lin/rejmerge


INSTALL
=======

To build and install this package, run the following shell commands:

    make && make install

For static linking, you need `pkg-config(1)` and should run `make(1p)`
as follows:

    make LDFLAGS="-static $(pkg-config --static --libs libarchive)"

See the `config.mk` file for configuration parameters and the
`src/pathnames.h` file for absolute filenames and settings that
`pkgutils` uses for various defaults.


DOCUMENTATION
=============

See `/man` directory for manual pages.


LICENSE
=======

`pkgutils` is licensed through the
[GNU General Public License v2 or later](https://gnu.org/licenses/gpl.html).

See `COPYING` for license terms and `COPYRIGHT` for notices.
