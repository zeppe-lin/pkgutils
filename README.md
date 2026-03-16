OVERVIEW
========

`pkgutils` is a collection of utilities for working with software
packages:

- `pkgadd(8)` - install packages
- `pkgrm(8)` - remove packages
- `pkginfo(1)` - show package information
- `pkgchk(1)` - check package integrity

This distribution is a fork of CRUX `pkgutils` at commit 9ca0da6
(Sat Nov 17 2018) with the following differences:
  * Code organized into a library [libpkgcore][1] and utilities
  * GNU-style options, help, and usage output
  * Improved adherence to GNU Coding Standards
  * Manual pages in `scdoc(5)` format
  * Split `pkgadd(8)` manual into `pkgadd(8)` and `pkgadd.conf(5)`
  * Support for `zstd` packages
  * Vim syntax highlighting for `pkgadd.conf`
  * Optional support for preserving ACLs and xattrs in `pkgadd(8)`
  * New utility `pkgchk(1)` for integrity checks

See the git log for full history.

Original sources:
  * https://git.crux.nu/tools/pkgutils.git

---

REQUIREMENTS
============

Build-time
----------
  * C++11 compiler (GCC 4.8.1+, Clang 3.3+)
  * Meson
  * Ninja
  * `pkg-config(1)`
  * [libpkgcore][1]
  * [libpkgaudit][2]
  * `scdoc(1)` to generate manual pages

Also see [rejmerge][3], a utility for merging files rejected by
`pkgadd(8)` during package upgrades.

---

INSTALLATION
============

General
-------

```sh
# Configure
meson setup build

# Compile
meson compile -C build

# Install
meson install -C build
```

Link Mode
---------

`pkgutils` can be built against shared or static [libpkgcore][1], and
[libpkgaudit][2].

Shared:

```sh
meson setup build -Dlink_mode=shared
meson compile -C build
```

Static:

```sh
meson setup build -Dlink_mode=static
meson compile -C build
```

For generic static packaging, keep LTO disabled unless the whole
toolchain is prepared for static LTO archives.

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

[1]: https://github.com/zeppe-lin/libpkgcore
[2]: https://github.com/zeppe-lin/libpkgaudit
[3]: https://github.com/zeppe-lin/rejmerge
