.POSIX:

include ../config.mk

MAN1 = pkgchk.1 pkginfo.1
MAN5 = pkgadd.conf.5
MAN7 = packages.7
MAN8 = pkgadd.8 pkgrm.8

all: $(MAN1) $(MAN5) $(MAN7) $(MAN8)

$(MAN1) $(MAN5) $(MAN7) $(MAN8):
	scdoc < $@.scdoc > $@

install: all
	mkdir -p $(DESTDIR)$(MANPREFIX)/man1
	mkdir -p $(DESTDIR)$(MANPREFIX)/man5
	mkdir -p $(DESTDIR)$(MANPREFIX)/man7
	mkdir -p $(DESTDIR)$(MANPREFIX)/man8
	cp -f $(MAN1) $(DESTDIR)$(MANPREFIX)/man1
	cp -f $(MAN5) $(DESTDIR)$(MANPREFIX)/man5
	cp -f $(MAN7) $(DESTDIR)$(MANPREFIX)/man7
	cp -f $(MAN8) $(DESTDIR)$(MANPREFIX)/man8
	cd $(DESTDIR)$(MANPREFIX)/man1 && chmod 0644 $(MAN1)
	cd $(DESTDIR)$(MANPREFIX)/man5 && chmod 0644 $(MAN5)
	cd $(DESTDIR)$(MANPREFIX)/man7 && chmod 0644 $(MAN7)
	cd $(DESTDIR)$(MANPREFIX)/man8 && chmod 0644 $(MAN8)

uninstall:
	cd $(DESTDIR)$(MANPREFIX)/man1 && rm -f $(MAN1)
	cd $(DESTDIR)$(MANPREFIX)/man5 && rm -f $(MAN5)
	cd $(DESTDIR)$(MANPREFIX)/man7 && rm -f $(MAN7)
	cd $(DESTDIR)$(MANPREFIX)/man8 && rm -f $(MAN8)

clean:
	rm -f $(MAN1) $(MAN5) $(MAN7) $(MAN8)

.PHONY: all install uninstall clean
