ABOUT
-----
This directory contains _pkgutils_, a set of utilities like _pkgadd_,
_pkgrm_, and _pkginfo_ that are used for managing software packages:
adding, removing and basic information gathering.

This _pkgutils_ distribution is a fork of CRUX' _pkgutils_ as of
commit 9ca0da6 (Sat Nov 17 2018) with the following differences:
  * added GNU-style long options
  * refactored options parsing
  * the man pages have been rewritten in POD format
  * the man page pkgadd(8) have been split into two:
    pkgadd(8) and pkgadd.conf(5)
  * added bash completion
  * build files have been rewritten

See git log for complete/further differences.

The original sources can be downloaded from:
  1. git://crux.nu/tools/pkgutils.git                        (git)
  2. https://crux.nu/gitweb/?p=tools/pkgutils.git;a=summary  (web)

REQUIREMENTS
------------
Build time:
  * c99 compiler
  * POSIX sh(1p), make(1p) and "mandatory utilities"
  * pod2man(1pm) to build man pages
  * libarchive(3) to unpack an archive files

Tests:
  * podchecker(1pm) to check PODs for errors
  * curl(1) to check URLs for response code

INSTALL
-------
The shell commands `make && make install` should build and install
this package.  See _config.mk_ file for configuration parameters.

The shell command `make check` should start some tests.

LICENSE
-------
_pkgutils_ is licensed through the GNU General Public License v2 or
later <https://gnu.org/licenses/gpl.html>.
Read the _COPYING_ file for copying conditions.
Read the _COPYRIGHT_ file for copyright notices.

<!-- vim:sw=2:ts=2:sts=2:et:cc=72:tw=70
End of file. -->
