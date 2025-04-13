.POSIX:

include ../config.mk

all:

install:
	mkdir -p $(DESTDIR)$(VIMFILESDIR)/ftdetect
	mkdir -p $(DESTDIR)$(VIMFILESDIR)/syntax
	cp -f ftdetect/pkgaddconf.vim $(DESTDIR)$(VIMFILESDIR)/ftdetect
	cp -f syntax/pkgaddconf.vim $(DESTDIR)$(VIMFILESDIR)/syntax

uninstall:
	rm -f $(DESTDIR)$(VIMFILESDIR)/ftdetect/pkgaddconf.vim
	rm -f $(DESTDIR)$(VIMFILESDIR)/syntax/pkgaddconf.vim

clean:

.PHONY: all install uninstall clean
