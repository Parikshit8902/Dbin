# Makefile for Super User

CC = gcc
CFLAGS = -Wall -Wextra -g -pthread
TARGET = su
SRC = SU.c

# Default target
all: $(TARGET)

# Linking the object file
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

# Cleaning up build artifacts
clean:
	rm -f $(TARGET)

.PHONY: all clean
