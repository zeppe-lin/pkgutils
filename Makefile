#
#  pkgutils
#
#  Copyright (c) 2000-2005 by Per Liden <per@fukt.bth.se>
#  Copyright (c) 2006-2017 by CRUX team (http://crux.nu)
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, 
#  USA.
#

.SUFFIXES: .cc .o
include config.mk

SRC = $(wildcard *.cc)
OBJ = $(SRC:.cc=.o)
BIN = pkgadd
MAN = pkgadd.8 pkginfo.8 pkgrm.8

all: $(BIN) $(MAN)

%: %.in
	sed -e "s/#VERSION#/$(VERSION)/" $< > $@

.cc.o:
	$(CXX) -c -o $@ $< $(CXXFLAGS) $(CPPFLAGS)

$(BIN): $(OBJ)
	$(LD) -o $@ $^ $(LDFLAGS)

install: all
	install -D -m0755 pkgadd      $(DESTDIR)$(BINDIR)/pkgadd
	install -D -m0644 pkgadd.conf $(DESTDIR)$(ETCDIR)/pkgadd.conf
	install -D -m0644 pkgadd.8    $(DESTDIR)$(MANDIR)/man8/pkgadd.8
	install -D -m0644 pkgrm.8     $(DESTDIR)$(MANDIR)/man8/pkgrm.8
	install -D -m0644 pkginfo.8   $(DESTDIR)$(MANDIR)/man8/pkginfo.8
	ln -sf pkgadd $(DESTDIR)$(BINDIR)/pkgrm
	ln -sf pkgadd $(DESTDIR)$(BINDIR)/pkginfo

uninstall:
	rm -f  $(DESTDIR)$(BINDIR)/pkgadd
	rm -f  $(DESTDIR)$(ETCDIR)/pkgadd.conf
	rm -f  $(DESTDIR)$(MANDIR)/pkgadd.8
	rm -f  $(DESTDIR)$(MANDIR)/pkgrm.8
	rm -f  $(DESTDIR)$(MANDIR)/pkginfo.8
	unlink $(DESTDIR)$(BINDIR)/pkgrm
	unlink $(DESTDIR)$(BINDIR)/pkginfo

clean:
	rm -f $(OBJ) $(MAN) $(BIN)

# End of file
