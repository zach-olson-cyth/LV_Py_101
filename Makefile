CC      ?= x86_64-linux-gnu-gcc
CFLAGS  := -O2 -fPIC -Wall -Wextra -std=c99 -D_GNU_SOURCE
TARGET  := waveform_gen.so
SRC     := waveform_gen.c

.PHONY: all test clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -shared -o $@ $< -lm
	@echo "Built $(TARGET) for NI Linux RT (x86-64)"

test: $(SRC)
	$(CC) $(CFLAGS) -DWAVEFORM_SMOKE_TEST -o waveform_gen_test $< -lm
	./waveform_gen_test

clean:
	rm -f $(TARGET) waveform_gen_test
