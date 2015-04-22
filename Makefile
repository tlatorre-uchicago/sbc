default: all

.DEFAULT:
	cd src && $(MAKE) $@

test:
	cd tests && $(MAKE) $@
