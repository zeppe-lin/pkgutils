pkgrm(8) "version #VERSION#" "Package Manager Manual"

# NAME

pkgrm - remove software package

# SYNOPSIS

pkgrm [options] _PKGNAME_

# DESCRIPTION

*pkgrm* is a _package management_ utility, which removes/uninstalls a
previously installed software packages.

# OPTIONS

*-r*, *--root* _PATH_
	Specify alternative installation root (default is "/").  This should be
	used if you want to remove a package from a temporary mounted partition,
	which is "owned" by another system.  By using this option you not only
	specify where the software is installed, but you also specify which
	package database to use.

*-v*, *--version*
	Print version and exit.

*-h*, *--help*
	Print help and exit.

# SEE ALSO

*pkgadd*(8), *pkginfo*(8), *pkgmk*(8), *rejmerge*(8)

# COPYRIGHT

pkgrm (pkgutils) is Copyright (c) 2000-2005 Per Liden++
pkgrm (pkgutils) is Copyright (c) 2006-2017 CRUX team (https://crux.nu)

pkgrm (pkgutils) is licensed through the GNU General Public License, read the
COPYING file for the complete license.

; vim:cc=80
; End of file.
