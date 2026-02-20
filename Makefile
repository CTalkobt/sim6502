CC = gcc
CFLAGS = -Wall -Wextra -O2 -I src
SRCS = src/sim6502.c src/opcodes_6502.c src/opcodes_6502_undoc.c src/opcodes_65c02.c src/opcodes_65ce02.c src/opcodes_45gs02.c
OBJS = $(SRCS:.c=.o)
TARGET = sim6502

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

test: $(TARGET)
	./tools/run_tests.py

.PHONY: all clean test
