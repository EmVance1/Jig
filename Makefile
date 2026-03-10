CC      = gcc
CFLAGS  = -Wall -Wextra -Wpedantic -O2 -Iinclude -DVANGO_PKG_VERSION_MAJOR=0 -DVANGO_PKG_VERSION_MINOR=1 -DVANGO_PKG_VERSION_PATCH=0
AR      = ar
ARFLAGS = rcs

SRC_DIR = src
OBJ_DIR = build
LIB     = libjig.a

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))

all: $(LIB)

$(LIB): $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(OBJ_DIR) $(LIB)

.PHONY: all clean
