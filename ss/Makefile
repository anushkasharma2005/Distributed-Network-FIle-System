# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -pthread -g -I. -I.. -I../api_ns_ss -I../api_c_ss -I../api_c_ns
LDFLAGS = -pthread

# Directories
BUILD_DIR = build
BIN_DIR = bin
API_NS_SS_DIR = ../api_ns_ss
API_C_SS_DIR = ../api_c_ss
API_C_NS_DIR = ../api_c_ns

# Source files
SS_SRC = main.c
API_NS_SS_SRC = $(API_NS_SS_DIR)/ns_ss_connection.c $(API_NS_SS_DIR)/ns_api.c $(API_NS_SS_DIR)/ss_api.c
API_C_SS_SRC = $(API_C_SS_DIR)/client_ss_connection.c
API_C_NS_SRC = $(API_C_NS_DIR)/networking.c

# Object files
SS_OBJ = $(patsubst %.c,$(BUILD_DIR)/%.o,$(notdir $(SS_SRC)))
API_NS_SS_OBJ = $(patsubst %.c,$(BUILD_DIR)/%.o,$(notdir $(API_NS_SS_SRC)))
API_C_SS_OBJ = $(patsubst %.c,$(BUILD_DIR)/%.o,$(notdir $(API_C_SS_SRC)))
API_C_NS_OBJ = $(patsubst %.c,$(BUILD_DIR)/%.o,$(notdir $(API_C_NS_SRC)))

ALL_OBJ = $(SS_OBJ) $(API_NS_SS_OBJ) $(API_C_SS_OBJ) $(API_C_NS_OBJ)

# Executable
TARGET = $(BIN_DIR)/ss_server

# Default target
all: $(TARGET)

# Create directories
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Build the SS server executable
$(TARGET): $(BUILD_DIR) $(BIN_DIR) $(ALL_OBJ)
	$(CC) $(CFLAGS) $(ALL_OBJ) -o $@ $(LDFLAGS)
	@echo "✓ SS server built successfully!"

# Compile SS source files
$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Compile API NS-SS source files
$(BUILD_DIR)/%.o: $(API_NS_SS_DIR)/%.c | $(BUILD_DIR)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Compile API C-SS source files
$(BUILD_DIR)/%.o: $(API_C_SS_DIR)/%.c | $(BUILD_DIR)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Compile API C-NS networking source files
$(BUILD_DIR)/%.o: $(API_C_NS_DIR)/%.c | $(BUILD_DIR)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build files
clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(BUILD_DIR)
	rm -rf $(BIN_DIR)
	@echo "Clean complete"

# Run the SS server
run: $(TARGET)
	./$(TARGET)

# Rebuild everything
rebuild: clean all

.PHONY: all clean run rebuild