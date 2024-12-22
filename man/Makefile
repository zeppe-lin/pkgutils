include $(CURDIR)/../config.mk

MAN1 = pkginfo.1
MAN5 = pkgadd.conf.5
MAN8 = pkgadd.8 pkgrm.8

all:

install:
	mkdir -p $(DESTDIR)$(MANPREFIX)/man1
	mkdir -p $(DESTDIR)$(MANPREFIX)/man5
	mkdir -p $(DESTDIR)$(MANPREFIX)/man8
	cp -f $(MAN1) $(DESTDIR)$(MANPREFIX)/man1
	cp -f $(MAN5) $(DESTDIR)$(MANPREFIX)/man5
	cp -f $(MAN8) $(DESTDIR)$(MANPREFIX)/man8
	cd $(DESTDIR)$(MANPREFIX)/man1 && chmod 0644 $(MAN1)
	cd $(DESTDIR)$(MANPREFIX)/man5 && chmod 0644 $(MAN5)
	cd $(DESTDIR)$(MANPREFIX)/man8 && chmod 0644 $(MAN8)

uninstall:
	cd $(DESTDIR)$(MANPREFIX)/man1 && rm -f $(MAN1)
	cd $(DESTDIR)$(MANPREFIX)/man5 && rm -f $(MAN5)
	cd $(DESTDIR)$(MANPREFIX)/man8 && rm -f $(MAN8)

clean:

.PHONY: all install uninstall clean
