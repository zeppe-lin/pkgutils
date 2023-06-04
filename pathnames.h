//! \file  pathnames.h
//! \brief Absolute filenames that pkgutils wants for various defaults.
//!        See COPYING and COPYRIGHT files for corresponding information.

#pragma once

//!< Default location for pkgadd configuration file.
#define PKGADD_CONF             "/etc/pkgadd.conf"

//!< Default max length for configuration statement.
#define PKGADD_CONF_MAXLINE     1024

//!< Default package extension.
#define PKG_EXT         ".pkg.tar."

//!< Default path for package database.
#define PKG_DIR         "var/lib/pkg"

//!< Default package database location.
#define PKG_DB          "var/lib/pkg/db"

//!< Default path for rejected files.
#define PKG_REJECTED    "var/lib/pkg/rejected"

//!< Default package's name#version delimiter.
#define VERSION_DELIM   '#'

//!< Default location for ldconfig(8) binary.
#define LDCONFIG        "/sbin/ldconfig"

//!< Default location for ldconfig(8) configuration file.
#define LDCONFIG_CONF   "/etc/ld.so.conf"

// vim:sw=2:ts=2:sts=2:et:cc=72:tw=70
// End of file.
