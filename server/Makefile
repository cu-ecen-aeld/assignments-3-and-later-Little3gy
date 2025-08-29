# Makefile for aesdsocket app

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -g -std=c99 
TARGET = aesdsocket
SOURCES = aesdsocket.c


# Cross compilation support
ifdef CROSS_COMPILE
	CC= $(CROSS_COMPILE)gcc
endif


# Default target
default: $(TARGET)


# All target 
all: $(TARGET)

# Build the target
$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES)

# Clean build artifacts 
clean: 
	rm -f $(TARGET) *.o *~

# Install target (if needed later)
#install -m 755 $(TARGET) $(DESTDIR)/user/bin

# .PHONY is used to prevent issue with filenames matching makefile commands
.PHONY: all default clean 
#install
