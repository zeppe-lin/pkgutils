.POSIX:

include config.mk

all lint install uninstall clean:
	cd src && $(MAKE) $@
	cd man && $(MAKE) $@
	cd vim && $(MAKE) $@
	cd completion && $(MAKE) $@

.PHONY: all lint install uninstall clean
