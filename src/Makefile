CC ?= gcc
override CFLAGS += -std=c99 -Wall -Wno-missing-braces -D_POSIX_C_SOURCE=199309L

bin = spm
objs = main.o list-command.o led-command.o event-command.o raw-command.o \
       options.o util.o
hdrs = commands.h options.h util.h

.PHONY: all
all: $(bin)

$(bin): $(objs) $(hdrs)
	$(CC) $(CFLAGS) $(filter-out %.h, $+) -o $@ $(LDFLAGS) -lspacemouse

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f $(objs) $(bin)
