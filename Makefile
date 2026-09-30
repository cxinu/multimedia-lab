CC      := gcc
CFLAGS  := -std=c11 -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -O2
TARGET  := image_analyzer
SRCS    := main.c

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
