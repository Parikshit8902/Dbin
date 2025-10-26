# Makefile for Normal User

CC = gcc
CFLAGS = -Wall -Wextra -g -pthread
TARGET = nu
SRC = NU.c

# Default target
all: $(TARGET)

# Linking the object file
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

# Cleaning up build artifacts
clean:
	rm -f $(TARGET)

.PHONY: all clean
