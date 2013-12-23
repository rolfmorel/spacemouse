VER_MAJOR = 0
VER_MINOR = 1
VER_PATCH =

ifneq ($(VER_PATCH),)
VERSION = $(VER_MAJOR).$(VER_MINOR).$(VER_PATCH)
else
VERSION = $(VER_MAJOR).$(VER_MINOR)
endif

all:

version:
	$(info $(VERSION))
