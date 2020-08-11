CFLAGS = -O2 -std=c99 -Wall

HAVE_PANDOC := $(shell pandoc --version 2>/dev/null)

.PHONY: all clean

all: pa_volume $(if $(HAVE_PANDOC),pa_volume.1)
	echo "All done"

%: %.c
	$(CC) $(CFLAGS) -o $@ $< $(shell pkg-config  pkg-config --libs libpulse)

%.1: %.1.md
	pandoc --standalone --to man $< -o $@

clean:
	rm -f pa_volume pa_volume.1
