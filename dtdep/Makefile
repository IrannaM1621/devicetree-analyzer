CC := gcc

CFLAGS := \
    -std=c11 \
    -Wall \
    -Wextra \
    -Werror \
    -Wshadow \
    -Wstrict-prototypes \
    -pedantic \
    -g

TARGET := dtdep

SRC_DIR := src
INC_DIR := include

CFLAGS += -I$(INC_DIR)

SRCS := $(shell find $(SRC_DIR) -name "*.c")
OBJS := $(SRCS:.c=.o)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	find $(SRC_DIR) -name "*.o" -delete
	rm -f $(TARGET)

.PHONY: clean
