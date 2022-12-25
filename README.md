ABOUT
-----
This directory contains *pkgutils*, a set of utilities like *pkgadd*,
*pkgrm*, and *pkginfo* that are used for managing software packages:
adding, removing and basic information gathering.

This *pkgutils* distribution is a fork of CRUX' *pkgutils* as of
commit 9ca0da6 (Sat Nov 17 2018) with the following differences:
- parsing arguments with getopt
- the man pages have been rewritten in POD
- the man page pkgadd(8) have been split into 2: pkgadd(8) and
  pkgadd.conf(5)
- added bash completion
- build files have been rewritten
See git log for further differences.

The original sources can be downloaded from:
1. git://crux.nu/tools/pkgutils.git                         (git)
2. https://crux.nu/gitweb/?p=tools/pkgutils.git;a=summary   (web)

REQUIREMENTS
------------
**Build time:**
- c99 compiler
- POSIX sh(1p), make(1p) and "mandatory utilities"
- pod2man(1pm) from perl distribution
- libarchive(3)

**Tests:**
- podchecker(1pm) to check PODs for errors
- httpx(1) to check URLs for non-200 response code

INSTALL
-------
The shell command `make install` should build and install
this package.  See `config.mk` file for configuration parameters.

The shell command `make check` should start some tests.

LICENSE
-------
*pkgutils* is licensed through the GNU General Public License v2 or
later <https://gnu.org/licenses/gpl.html>.
Read the COPYING file for copying conditions.
Read the COPYRIGHT file for copyright notices.

<!-- vim:sw=2:ts=2:sts=2:et:cc=72:tw=70
End of file. -->
