
.SUFFIXES: .cc .o
include config.mk

SRC = $(wildcard src/*.cc)
OBJ = $(SRC:.cc=.o)
SCD = $(wildcard man/*.scd)
MAN = $(SCD:.scd=)
BIN = pkgadd

all: $(BIN) $(MAN)

%: %.scd
	sed -e "s/#VERSION#/$(VERSION)/" $< | scdoc > $@

.cc.o:
	$(CXX) -c -o $@ $< $(CXXFLAGS) $(CPPFLAGS)

$(BIN): $(OBJ)
	$(LD) -o $@ $^ $(LDFLAGS)

install: all
	install -m 755 -D pkgadd              $(DESTDIR)$(BINDIR)/pkgadd
	install -m 644 -D pkgadd.conf.example $(DESTDIR)$(ETCDIR)/pkgadd.conf
	install -m 644 -D -t $(DESTDIR)$(MANDIR)/man8/ $(MAN)
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

# End of file.
