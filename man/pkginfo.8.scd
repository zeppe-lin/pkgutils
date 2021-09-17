pkginfo(8) "version #VERSION" "Package Manager Manual"

# NAME

pkginfo - display software package information

# SYNOPSIS

*pkginfo* [options]

# DESCRIPTION

*pkginfo* is a _package management_ utility, which displays information about
software packages that are installed on the system or that reside in a
particular directory.

# OPTIONS

*-i*, *--installed*
	List installed packages and their version.

*-l*, *--list* <_PKGNAME_|_FILE_>
	List files owned by the specified _PKGNAME_ or contained in _FILE_.

*-o*, *--owner* _PATTERN_
	List owner(s) of file(s) matching _PATTERN_.

*-f*, *--footprint* _FILE_
	Print footprint for _FILE_.  This feature is mainly used by *pkgmk*(8)
	for creating and comparing footprints.

*-r*, *--root* _PATH_
	Specify alternative installation root (default is "/").  This should be
	used if you want to display information about a package that is
	installed on a temporary mounted partition, which is "owned" by another
	system.  By using this option you specify which package database to use.

*-v*, *--version*
	Print version and exit.

*-h*, *--help*
	Print help and exit.

# SEE ALSO

*pkgadd*(8), *pkgrm*(8), *pkgmk*(8), *rejmerge*(8)

# COPYRIGHT

pkginfo (pkgutils) is Copyright (c) 2000-2005 Per Liden++
pkginfo (pkgutils) is Copyright (c) 2006-2017 CRUX Team (https://crux.nu)

pkginfo (pkgutils) is licensed through the GNU General Public License, read the
COPYING file for the complete license.

; End of file.
