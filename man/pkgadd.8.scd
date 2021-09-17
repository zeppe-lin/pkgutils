pkgadd(8) "version #VERSION#" "Package Manager Manual"

# NAME

pkgadd - install software package

# SYNOPSIS

*pkgadd* [options] _FILE_

# DESCRIPTION

*pkgadd* is a _package management_ utility, which installs a software package.
A _package_ is an archive of files (.pkg.tar.gz).

# OPTIONS

*-u*, *--upgrade*
	Upgrade/replace package with the same name as _FILE_.

*-f*, *--force*
	Force installation, overwrite conflicting files.  If the package that
	is about to be installed contains files that are already installed this
	option will cause all those files to be overwritten.

	This option should be used with care, preferably not at all.

*-r*, *--root* _PATH_
	Specify alternative installation root (default is "/").  This should
	*not* be used as a way to install software into e.g. /usr/local instead
	of /usr.  Instead this should be used if you want to install a package
	on a temporary mounted partition, which is "owned" by another system.
	By using this option you not only specify where the software should be
	installed, but you also specify which package database to use.

*-v*, *--version*
	Print version and exit.

*-h*, *--help*
	Print help and exit.

# CONFIGURATION

*pkgadd* is configured by the file _/etc/pkgadd.conf_.  This file can contain
rules, that are built out of three fragments: _event_, _pattern_ and _action_.
The event describes in what kind of situation this rule applies.  Currently
there are two types of events: *UPGRADE* and *INSTALL*.  *UPGRADE* rules are
applied when a package is installed over an existing version, and *INSTALL*
rules are applied in any case. The pattern is a regular expression.  The action
applicable to both the *UPGRADE* and *INSTALL* event is *YES* and *NO*.  More
than one rule of the same event type is allowed, in which case the first rule
will have the lowest priority and the last rule will have the highest priority.

Example:

```
UPGRADE         ^etc/.*$                NO
UPGRADE         ^var/log/.*$            NO
UPGRADE         ^etc/X11/.*$            YES
UPGRADE         ^etc/X11/XF86Config$    NO
```

The above example will cause pkgadd to never upgrade anything in /etc/ or
/var/log/ (subdirectories included), except files in /etc/X11/
(subdirectories included), unless it is the file /etc/X11/XF86Config.  The
default rule is to upgrade/install everything, rules in this file are
exceptions to that rule.

(*NOTE!* A _pattern_ should never contain an initial "/" since you are referring
to the files in the package, not the files on the disk.)

If pkgadd finds that a specific file should not be upgraded it will install it
under _/var/lib/pkg/rejected/_.  The user is then free to examine/use/remove
that file manually.

# FILES

/etc/pkgadd.conf
	Configuration file.

# SEE ALSO

*pkgrm*(8), *pkginfo*(8), *pkgmk*(8), *rejmerge*(8)

# COPYRIGHT

pkgadd (pkgutils) is Copyright (c) 2000-2005 Per Liden++
pkgadd (pkgutils) is Copyright (c) 2006-2017 CRUX team (https://crux.nu)

pkgadd (pkgutils) is licensed through the GNU General Public License, read the
COPYING file for the complete license.

; End of file.
