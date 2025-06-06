#compiler and flags
CC := $(CROSS_COMPILE)gcc
CFLAGS := -Wall -Werror

# Source and target
TARGET := writer
SRCS := writer.c
OBJS := $(SRCS:.c=.o)

# Default target
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean target
clean: 
	rm -f $(TARGET) *.o

