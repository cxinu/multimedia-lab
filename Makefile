CC      := gcc
CFLAGS  := -std=c11 -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion -O2
TARGET  := media_analyzer
SRCS    := main.c

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET)

run: $(TARGET)
	@if [ -z "$(FILE)" ]; then \
		echo "Usage: make run FILE=<path_to_media_file>"; \
		exit 1; \
	fi
	./$(TARGET) $(FILE)

clean:
	rm -f $(TARGET)
