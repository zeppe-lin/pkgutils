OVERVIEW
========

`pkgutils` is a collection of utilities such as `pkgadd(1)`,
`pkgrm(8)`, and `pkginfo(1)` for managing software packages:
installing, removing, and retrieving basic information.

This distribution is a fork of CRUX `pkgutils` at commit 9ca0da6
(Sat Nov 17 2018) with the following differences:
  * Code organized into a library (`libpkgutils`) and utilities
  * GNU-style options, help, and usage output
  * Improved adherence to GNU Coding Standards
  * Manual pages in `scdoc(5)` format
  * Split `pkgadd(8)` manual into `pkgadd(8)` and `pkgadd.conf(5)`
  * Support for `zstd` packages
  * Vim syntax highlighting for `pkgadd.conf`
  * Optional support for preserving ACLs and xattrs in `pkgadd(8)`

See the git log for full history.

Original sources:
  * https://git.crux.nu/tools/pkgutils.git

---

REQUIREMENTS
============

Build-time
----------
  * C++11 compiler (GCC 4.8.1+, Clang 3.3+)
  * POSIX `sh(1p)`, `make(1p)`, and "mandatory utilities"
  * `libarchive(3)` to unpack archive files
  * `scdoc(1)` to generate manual pages
  * `pkg-config(1)` (optional, for static linking)

Also see [rejmerge][https://github.com/zeppe-lin/rejmerge], a utility
for merging files rejected by `pkgadd(8)` during package upgrades.

---

INSTALLATION
============

To build and install:

```sh
make
make install   # as root
```

For static linking, ensure `pkg-config(1)` is available and run:

```sh
make LDFLAGS="-static $(pkg-config --static --libs libarchive)"
```

Configuration parameters are defined in `config.mk`.  
Default file paths and settings are specified in `src/pathnames.h`.

---

DOCUMENTATION
=============

Manual pages are provided in `/man` and installed under the system
manual hierarchy.

---

LICENSE
=======

`pkgutils` is licensed under the
[GNU General Public License v2 or later](https://gnu.org/licenses/gpl.html).

See `COPYING` for license terms and `COPYRIGHT` for notices.
