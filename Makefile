include VERSION.mk

PREFIX ?= /usr/local

bin = spm

all: src $(bin)

.PHONY: src
src:
	@$(MAKE) -C src
	cp src/$(bin) $(CURDIR)

.PHONY: install
install: $(bin)
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(bin) $(DESTDIR)$(PREFIX)/bin

.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(bin)

.PHONY: setuid
setuid: $(bin)
	chmod u+s $(bin)

.PHONY: clean
clean:
	@$(MAKE) -C src clean
	rm -f $(bin)
