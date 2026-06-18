# p-net Driver Test Project
# Makefile for building and testing the Profinet protocol implementation

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -I./include -g -O0
LDFLAGS = -lm

# Directories
SRC_DIR = src
TEST_DIR = tests
BUILD_DIR = build
INCLUDE_DIR = include

# Source files
SRCS = $(SRC_DIR)/pnet_device.c \
       $(SRC_DIR)/pnet_protocol.c \
       $(SRC_DIR)/pnet_network.c \
       $(SRC_DIR)/pnet_security.c \
       $(SRC_DIR)/pnet_driver.c

# Test files
TEST_SRCS = $(TEST_DIR)/test_main.c \
            $(TEST_DIR)/test_driver_arch.c \
            $(TEST_DIR)/test_install.c \
            $(TEST_DIR)/test_network.c \
            $(TEST_DIR)/test_security.c \
            $(TEST_DIR)/test_deployment.c

# Object files
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
TEST_OBJS = $(TEST_SRCS:$(TEST_DIR)/%.c=$(BUILD_DIR)/%.o)

# Targets
TEST_BIN = $(BUILD_DIR)/pnet_test

.PHONY: all test clean mkdir info

all: mkdir $(TEST_BIN)

mkdir:
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(TEST_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TEST_BIN): $(OBJS) $(TEST_OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(TEST_OBJS) -o $@ $(LDFLAGS)
	@echo "Build complete: $@"

test: $(TEST_BIN)
	@echo "\n--- Running p-net Test Suite ---"
	@./$(TEST_BIN)

clean:
	rm -rf $(BUILD_DIR)
	@echo "Clean complete"

info:
	@echo "p-net Driver Test Project"
	@echo "========================"
	@echo "Source files:  $(SRCS)"
	@echo "Test files:    $(TEST_SRCS)"
	@echo "Compiler:      $(CC)"
	@echo "Flags:         $(CFLAGS)"
