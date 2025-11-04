# Makefile for Storage Server

CC = gcc
CFLAGS = -Wall -Wextra -pthread -g -I.
LDFLAGS = -pthread

# Directories
API_NS_SS_DIR = ../api_ns_ss
API_C_SS_DIR = ../api_c_ss
BUILD_DIR = build
BIN_DIR = bin

# Source files
NS_SS_SRC = $(API_NS_SS_DIR)/ns_ss_connection.c
C_SS_SRC = $(API_C_SS_DIR)/client_ss_connection.c
MAIN_SRC = main.c

# Object files
NS_SS_OBJ = $(BUILD_DIR)/ns_ss_connection.o
C_SS_OBJ = $(BUILD_DIR)/client_ss_connection.o
MAIN_OBJ = $(BUILD_DIR)/storage_server_main.o

# Output binary
TARGET = $(BIN_DIR)/storage_server

# Default target
all: directories $(TARGET)

# Create necessary directories
directories:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(BIN_DIR)
	@mkdir -p storage

# Link object files to create executable
$(TARGET): $(NS_SS_OBJ) $(C_SS_OBJ) $(MAIN_OBJ)
	@echo "Linking $@..."
	# Link with networking implementation from api_c_ns
	$(CC) -o $@ $^ ../api_c_ns/networking.o $(LDFLAGS)
	@echo "Build complete: $@"

# Compile NS-SS API
$(NS_SS_OBJ): $(NS_SS_SRC) $(API_NS_SS_DIR)/ns_ss_connection.h
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -I.. -I$(API_NS_SS_DIR) -I$(API_C_SS_DIR) -c $< -o $@

# Compile Client-SS API
$(C_SS_OBJ): $(C_SS_SRC) $(API_C_SS_DIR)/client_ss_connection.h
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -I.. -I$(API_C_SS_DIR) -I$(API_NS_SS_DIR) -c $< -o $@

# Compile main
$(MAIN_OBJ): $(MAIN_SRC)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -I.. -I$(API_NS_SS_DIR) -I$(API_C_SS_DIR) -c $< -o $@

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(BUILD_DIR)
	rm -rf $(BIN_DIR)
	@echo "Clean complete"

# Clean everything including storage
cleanall: clean
	@echo "Cleaning storage directory..."
	rm -rf storage
	@echo "Clean all complete"

# Run the storage server (default configuration)
run: all
	@echo "Starting Storage Server..."
	./$(TARGET)

# Run with custom configuration
# Usage: make run-custom NS_IP=127.0.0.1 NS_PORT=8080 NM_PORT=9001 CLIENT_PORT=9002
run-custom: all
	@echo "Starting Storage Server with custom configuration..."
	./$(TARGET) $(NS_IP) $(NS_PORT) $(NM_PORT) $(CLIENT_PORT) ./storage

# Help target
help:
	@echo "Available targets:"
	@echo "  all         - Build the storage server (default)"
	@echo "  clean       - Remove build artifacts"
	@echo "  cleanall    - Remove build artifacts and storage"
	@echo "  run         - Build and run with default configuration"
	@echo "  run-custom  - Build and run with custom configuration"
	@echo "  help        - Show this help message"
	@echo ""
	@echo "Example custom run:"
	@echo "  make run-custom NS_IP=192.168.1.100 NS_PORT=8080 NM_PORT=9001 CLIENT_PORT=9002"

.PHONY: all directories clean cleanall run run-custom help