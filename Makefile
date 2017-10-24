CFLAGS = -O2 -g -std=c99 -Wall

.PHONY: all

all: pa_volume
	echo "All done"

%: %.c
	$(CC) $(CFLAGS) -o $@ $< $(shell pkg-config  pkg-config --libs libpulse)

clean:
	rm -f pa_volume
