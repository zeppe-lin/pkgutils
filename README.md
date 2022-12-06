pkgutils - set of utilities to manage software packages
=======================================================
pkgutils is a set of utilities like pkgadd, pkgrm and pkginfo that are
used for managing software packages: adding, removing and basic
information gathering.

This pkgutils distribution is a fork of CRUX' pkgutils as of commit
9ca0da6 (Sat Nov 17 2018).  The man pages have been rewritten in POD
(Plain Old Documentation).  Added bash completion.

The original sources can be downloaded from:

  1.
    git clone git://crux.nu/tools/pkgutils.git
    git checkout 9ca0da6

  2. https://crux.nu/gitweb/?p=tools/pkgutils.git;a=summary


Dependencies
------------
Build time:
- c++ compiler
- POSIX make(1p) and utilities like mkdir(1p), cp(1p), ln(1p), rm(1p)
- podchecker(1pm) and pod2man(1pm) from perl distribution
- libarchive(3)


Install
-------
The shell commands `make; make install` should build and install this
package.  See `config.mk` file for configuration parameters.


License and Copyright
---------------------
pkgutils is licensed through the GNU General Public License v2 or
later <https://gnu.org/licenses/gpl.html>.
Read the COPYING file for copying conditions.
Read the COPYRIGHT file for copyright notices.


<!-- vim:ft=markdown:sw=2:ts=2:sts=2:et:cc=72:tw=70
End of file. -->
