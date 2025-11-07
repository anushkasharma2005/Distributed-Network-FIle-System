# # Makefile for Storage Server

# CC = gcc
# CFLAGS = -Wall -Wextra -pthread -g -I.
# LDFLAGS = -pthread

# # Directories
# API_NS_SS_DIR = ../api_ns_ss
# API_C_SS_DIR = ../api_c_ss
# BUILD_DIR = build
# BIN_DIR = bin

# # Source files
# NS_SS_SRC = $(API_NS_SS_DIR)/ns_ss_connection.c
# C_SS_SRC = $(API_C_SS_DIR)/client_ss_connection.c
# MAIN_SRC = main.c

# # Object files
# NS_SS_OBJ = $(BUILD_DIR)/ns_ss_connection.o
# C_SS_OBJ = $(BUILD_DIR)/client_ss_connection.o
# MAIN_OBJ = $(BUILD_DIR)/storage_server_main.o

# # Output binary
# TARGET = $(BIN_DIR)/storage_server

# # Default target
# all: directories $(TARGET)

# # Create necessary directories
# directories:
# 	@mkdir -p $(BUILD_DIR)
# 	@mkdir -p $(BIN_DIR)
# 	@mkdir -p storage

# # Link object files to create executable
# $(TARGET): $(NS_SS_OBJ) $(C_SS_OBJ) $(MAIN_OBJ)
# 	@echo "Linking $@..."
# 	# Link with networking implementation from api_c_ns
# 	$(CC) -o $@ $^ ../api_c_ns/networking.o $(LDFLAGS)
# 	@echo "Build complete: $@"

# # Compile NS-SS API
# $(NS_SS_OBJ): $(NS_SS_SRC) $(API_NS_SS_DIR)/ns_ss_connection.h
# 	@echo "Compiling $<..."
# 	$(CC) $(CFLAGS) -I.. -I$(API_NS_SS_DIR) -I$(API_C_SS_DIR) -c $< -o $@

# # Compile Client-SS API
# $(C_SS_OBJ): $(C_SS_SRC) $(API_C_SS_DIR)/client_ss_connection.h
# 	@echo "Compiling $<..."
# 	$(CC) $(CFLAGS) -I.. -I$(API_C_SS_DIR) -I$(API_NS_SS_DIR) -c $< -o $@

# # Compile main
# $(MAIN_OBJ): $(MAIN_SRC)
# 	@echo "Compiling $<..."
# 	$(CC) $(CFLAGS) -I.. -I$(API_NS_SS_DIR) -I$(API_C_SS_DIR) -c $< -o $@

# # Clean build artifacts
# clean:
# 	@echo "Cleaning build artifacts..."
# 	rm -rf $(BUILD_DIR)
# 	rm -rf $(BIN_DIR)
# 	@echo "Clean complete"

# # Clean everything including storage
# cleanall: clean
# 	@echo "Cleaning storage directory..."
# 	rm -rf storage
# 	@echo "Clean all complete"

# # Run the storage server (default configuration)
# run: all
# 	@echo "Starting Storage Server..."
# 	./$(TARGET)

# # Run with custom configuration
# # Usage: make run-custom NS_IP=127.0.0.1 NS_PORT=8080 NM_PORT=9001 CLIENT_PORT=9002
# run-custom: all
# 	@echo "Starting Storage Server with custom configuration..."
# 	./$(TARGET) $(NS_IP) $(NS_PORT) $(NM_PORT) $(CLIENT_PORT) ./storage

# # Help target
# help:
# 	@echo "Available targets:"
# 	@echo "  all         - Build the storage server (default)"
# 	@echo "  clean       - Remove build artifacts"
# 	@echo "  cleanall    - Remove build artifacts and storage"
# 	@echo "  run         - Build and run with default configuration"
# 	@echo "  run-custom  - Build and run with custom configuration"
# 	@echo "  help        - Show this help message"
# 	@echo ""
# 	@echo "Example custom run:"
# 	@echo "  make run-custom NS_IP=192.168.1.100 NS_PORT=8080 NM_PORT=9001 CLIENT_PORT=9002"

# .PHONY: all directories clean cleanall run run-custom help

# Makefile for Storage Server

CC = gcc
CFLAGS = -Wall -Wextra -pthread -g -I. -I.. -I../api_ns_ss -I../api_c_ss
LDFLAGS = -pthread

# Directories
API_NS_SS_DIR = ../api_ns_ss
API_C_SS_DIR = ../api_c_ss
BUILD_DIR = build
BIN_DIR = bin

# Source files from current directory (ss folder)
SS_LOCAL_SRCS = file_structure_1.c file_structure_2.c main.c

# Source files from API directories
NS_SS_SRC = $(API_NS_SS_DIR)/ns_ss_connection.c
C_SS_SRC = $(API_C_SS_DIR)/client_ss_connection.c
C_SS_SRC_2 = $(API_C_SS_DIR)/client_ss_connection_2.c

# Object files
SS_LOCAL_OBJS = $(BUILD_DIR)/file_structure_1.o $(BUILD_DIR)/file_structure_2.o $(BUILD_DIR)/storage_server_main.o
NS_SS_OBJ = $(BUILD_DIR)/ns_ss_connection.o
C_SS_OBJ = $(BUILD_DIR)/client_ss_connection.o
C_SS_OBJ_2 = $(BUILD_DIR)/client_ss_connection_2.o

# All object files
ALL_OBJS = $(SS_LOCAL_OBJS) $(NS_SS_OBJ) $(C_SS_OBJ) $(C_SS_OBJ_2)

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
$(TARGET): $(ALL_OBJS)
	@echo "Linking $@..."
	$(CC) -o $@ $^ ../api_c_ns/networking.o $(LDFLAGS)
	@echo "✓ Build complete: $@"

# Compile local SS files
$(BUILD_DIR)/file_structure_1.o: file_structure_1.c file_structure.h
	@echo "Compiling file_structure_1.c..."
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/file_structure_2.o: file_structure_2.c file_structure.h
	@echo "Compiling file_structure_2.c..."
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/storage_server_main.o: main.c file_structure.h
	@echo "Compiling main.c..."
	$(CC) $(CFLAGS) -c $< -o $@

# Compile NS-SS API
$(NS_SS_OBJ): $(NS_SS_SRC) $(API_NS_SS_DIR)/ns_ss_connection.h
	@echo "Compiling ns_ss_connection.c..."
	$(CC) $(CFLAGS) -c $< -o $@

# Compile Client-SS API (main file)
$(C_SS_OBJ): $(C_SS_SRC) $(API_C_SS_DIR)/client_ss_connection.h
	@echo "Compiling client_ss_connection.c..."
	$(CC) $(CFLAGS) -c $< -o $@

# Compile Client-SS API (secondary file)
$(C_SS_OBJ_2): $(C_SS_SRC_2) $(API_C_SS_DIR)/client_ss_connection.h
	@echo "Compiling client_ss_connection_2.c..."
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(BUILD_DIR)
	rm -rf $(BIN_DIR)
	@echo "✓ Clean complete"

# Clean everything including storage
cleanall: clean
	@echo "Cleaning storage directory..."
	rm -rf storage
	@echo "✓ Clean all complete"

# Run the storage server (default configuration)
run: all
	@echo "Starting Storage Server..."
	./$(TARGET)

# Run with custom configuration
# Usage: make run-custom NS_IP=127.0.0.1 NS_PORT=8080 NM_PORT=9001 CLIENT_PORT=9002
run-custom: all
	@echo "Starting Storage Server with custom configuration..."
	./$(TARGET) $(NS_IP) $(NS_PORT) $(NM_PORT) $(CLIENT_PORT) ./storage

# Debug build (with debug symbols and verbose output)
debug: CFLAGS += -DDEBUG -g3 -O0
debug: clean all
	@echo "✓ Debug build complete"

# Test target - compiles everything and runs basic checks
test: all
	@echo "Running basic tests..."
	@if [ -f $(TARGET) ]; then \
        echo "✓ Binary exists"; \
        ls -lh $(TARGET); \
    else \
        echo "✗ Binary not found"; \
        exit 1; \
    fi
	@echo "✓ All tests passed"

# Show build information
info:
	@echo "=== Build Information ==="
	@echo "Compiler: $(CC)"
	@echo "Flags: $(CFLAGS)"
	@echo "Linker Flags: $(LDFLAGS)"
	@echo "Target: $(TARGET)"
	@echo "Object Files:"
	@echo "  $(ALL_OBJS)" | tr ' ' '\n' | sed 's/^/  /'
	@echo "========================="

# Help target
help:
	@echo "Available targets:"
	@echo "  all         - Build the storage server (default)"
	@echo "  clean       - Remove build artifacts"
	@echo "  cleanall    - Remove build artifacts and storage"
	@echo "  run         - Build and run with default configuration"
	@echo "  run-custom  - Build and run with custom configuration"
	@echo "  debug       - Build with debug symbols and no optimization"
	@echo "  test        - Build and run basic tests"
	@echo "  info        - Show build configuration information"
	@echo "  help        - Show this help message"
	@echo ""
	@echo "Source files included:"
	@echo "  - file_structure_1.c"
	@echo "  - file_structure_2.c"
	@echo "  - main.c"
	@echo "  - $(C_SS_SRC)"
	@echo "  - $(C_SS_SRC_2)"
	@echo "  - $(NS_SS_SRC)"
	@echo ""
	@echo "Example custom run:"
	@echo "  make run-custom NS_IP=192.168.1.100 NS_PORT=8080 NM_PORT=9001 CLIENT_PORT=9002"

.PHONY: all directories clean cleanall run run-custom debug test info help