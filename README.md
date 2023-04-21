OVERVIEW
--------
This directory contains pkgutils, a set of utilities like pkgadd(1), pkgrm(8),
and pkginfo(1) that are used for managing software packages: adding, removing
and basic information gathering.

This distribution is a fork of CRUX' pkgutils as of commit 9ca0da6 (Sat Nov 17
2018) with the following differences:
- added GNU-style long options
- refactored options parsing
- the man pages have been rewritten in POD format
- the man page pkgadd(8) have been split into two: pkgadd(8) and pkgadd.conf(5)
- added bash completion
- build files have been rewritten

See git log for complete/further differences.

The original sources can be downloaded from:
1. git://crux.nu/tools/pkgutils.git                        (git)
2. https://crux.nu/gitweb/?p=tools/pkgutils.git;a=summary  (web)


REQUIREMENTS
------------
**Build time**:
- c99 compiler
- POSIX sh(1p), make(1p) and "mandatory utilities"
- pod2man(1pm) to build man pages
- libarchive(3) to unpack an archive files

Also, see [rejmerge][1], an utility that merges files that were rejected by
pkgadd(8) during package upgrades.

[1]: https://github.com/zeppe-lin/rejmerge

INSTALL
-------
The shell commands `make && make install` should build and install this
package.  See `config.mk` file for configuration parameters.


LICENSE
-------
pkgutils is licensed through the GNU General Public License v2 or later
<https://gnu.org/licenses/gpl.html>.
Read the COPYING file for copying conditions.
Read the COPYRIGHT file for copyright notices.
