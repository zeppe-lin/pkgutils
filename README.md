OVERVIEW
--------
This directory contains pkgutils, a set of utilities like
pkgadd(1), pkgrm(8), and pkginfo(1) that are used for managing
software packages: adding, removing and basic information
gathering.

This distribution is a fork of CRUX' pkgutils as of commit
9ca0da6 (Sat Nov 17 2018) with the following differences:
- GNU-style options/help/usage
- better GNU Coding Standards support
- manual pages in mdoc(7) format
- split pkgadd(8) manual page into pkgadd(8) and pkgadd.conf(5)
- zstd packages support
- vim syntax highlight for `pkgadd.conf` file

See git log for complete/further differences.

The original sources can be downloaded from:
1. git://crux.nu/tools/pkgutils.git              (git)
2. https://crux.nu/gitweb/?p=tools/pkgutils.git  (web)


REQUIREMENTS
------------
**Build time**:
- C++11 compiler (GCC 4.8.1 and later, Clang 3.3 and later)
- POSIX sh(1p) and "mandatory utilities"
- GNU make(1)
- pkg-config(1)
- libarchive(3) to unpack an archive files

Also, see [rejmerge]( https://github.com/zeppe-lin/rejmerge ),
an utility that merges files that were rejected by pkgadd(8)
during package upgrades.


INSTALL
-------
The shell commands `make && make install` should build and
install this package.
The shell command `make install_bashcomp` should install bash
completion script.

See `config.mk` file for configuration parameters, and
`src/pathnames.h` for absolute filenames and settings that
pkgutils wants for various defaults.


LICENSE
-------
pkgutils is licensed through the GNU General Public License v2
or later <https://gnu.org/licenses/gpl.html>.
Read the COPYING file for copying conditions.
Read the COPYRIGHT file for copyright notices.
