# Makefile for Central Repository

CC = gcc
CFLAGS = -Wall -Wextra -g -pthread
LDLIBS = -lsqlite3  # Linker libraries
TARGET = cr
SRC = CR.c

# Default target
all: $(TARGET)

# Linking the object file, including necessary libraries
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDLIBS)

# Cleaning up build artifacts
clean:
	rm -f $(TARGET) *.db # Also removes the database file if desired

.PHONY: all clean
