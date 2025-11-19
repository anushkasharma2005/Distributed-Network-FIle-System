# CC = gcc
# CFLAGS = -Wall -Wextra -pthread -g -I. -I.. -I../api_ns_ss -I../api_c_ss
# LDFLAGS = -pthread

# # Directories
# BUILD_DIR = build
# BIN_DIR = bin
# API_NS_SS_DIR = ../api_ns_ss
# API_C_SS_DIR = ../api_c_ss
# API_C_NS_DIR = ../api_c_ns

# # Source files from current directory (ss folder)
# SS_LOCAL_SRCS = file_structure_1.c file_structure_2.c file_structure_3.c main.c

# # Source files from API directories
# NS_SS_SRC = $(API_NS_SS_DIR)/ns_ss_connection.c
# C_SS_SRC = $(API_C_SS_DIR)/client_ss_connection.c
# C_SS_SRC_2 = $(API_C_SS_DIR)/client_ss_connection_2.c
# C_SS_SRC_3 = $(API_C_NS_DIR)/client_operations_3.c

# # Object files
# SS_LOCAL_OBJS = $(BUILD_DIR)/file_structure_1.o $(BUILD_DIR)/file_structure_2.o $(BUILD_DIR)/file_structure_3.o $(BUILD_DIR)/storage_server_main.o
# NS_SS_OBJ = $(BUILD_DIR)/ns_ss_connection.o
# C_SS_OBJ = $(BUILD_DIR)/client_ss_connection.o
# C_SS_OBJ_2 = $(BUILD_DIR)/client_ss_connection_2.o
# C_SS_OBJ_3 = $(BUILD_DIR)/client_operations_3.o

# # All object files
# ALL_OBJS = $(SS_LOCAL_OBJS) $(NS_SS_OBJ) $(C_SS_OBJ) $(C_SS_OBJ_2) $(C_SS_OBJ_3)

# ALL_OBJ = $(SS_OBJ) $(API_NS_SS_OBJ) $(API_C_SS_OBJ) $(API_C_NS_OBJ)

# # Executable
# TARGET = $(BIN_DIR)/ss_server


# directories:
# 	@mkdir -p $(BUILD_DIR)
# 	@mkdir -p $(BIN_DIR)
# 	@mkdir -p storage


# # Default target
# all: $(TARGET)

# # Create directories
# $(BUILD_DIR):
# 	mkdir -p $(BUILD_DIR)


# $(BIN_DIR):
# 	mkdir -p $(BIN_DIR)

# # Link object files to create executable
# $(TARGET): $(ALL_OBJS)
# 	@echo "Linking $@..."
# 	$(CC) -o $@ $^ ../api_c_ns/networking.o $(LDFLAGS)
# 	@echo "✓ Build complete: $@"

# # Compile local SS files
# $(BUILD_DIR)/file_structure_1.o: file_structure_1.c file_structure.h | $(BUILD_DIR)
# 	@echo "Compiling file_structure_1.c..."
# 	$(CC) $(CFLAGS) -c $< -o $@

# $(BUILD_DIR)/file_structure_2.o: file_structure_2.c file_structure.h | $(BUILD_DIR)
# 	@echo "Compiling file_structure_2.c..."
# 	$(CC) $(CFLAGS) -c $< -o $@

# $(BUILD_DIR)/file_structure_3.o: file_structure_3.c file_structure.h | $(BUILD_DIR)
# 	@echo "Compiling file_structure_3.c..."
# 	$(CC) $(CFLAGS) -c $< -o $@

# $(BUILD_DIR)/storage_server_main.o: main.c file_structure.h | $(BUILD_DIR)
# 	@echo "Compiling main.c..."
# 	$(CC) $(CFLAGS) -c $< -o $@

# # Compile NS-SS API
# $(NS_SS_OBJ): $(NS_SS_SRC) $(API_NS_SS_DIR)/ns_ss_connection.h | $(BUILD_DIR)
# 	@echo "Compiling ns_ss_connection.c..."
# 	$(CC) $(CFLAGS) -c $< -o $@

# # Compile Client-SS API (main file)
# $(C_SS_OBJ): $(C_SS_SRC) $(API_C_SS_DIR)/client_ss_connection.h | $(BUILD_DIR)
# 	@echo "Compiling client_ss_connection.c..."
# 	$(CC) $(CFLAGS) -c $< -o $@

# # Compile Client-SS API (secondary file)
# $(C_SS_OBJ_2): $(C_SS_SRC_2) $(API_C_SS_DIR)/client_ss_connection.h | $(BUILD_DIR)
# 	@echo "Compiling client_ss_connection_2.c..."
# 	$(CC) $(CFLAGS) -c $< -o $@

# $(C_SS_OBJ_3): $(C_SS_SRC_3) $(API_C_SS_DIR)/client_ss_connection.h | $(BUILD_DIR)
# 	@echo "Compiling client_ss_connection_3.c..."
# 	$(CC) $(CFLAGS) -c $< -o $@

# # Compile API C-NS networking source files
# $(BUILD_DIR)/%.o: $(API_C_NS_DIR)/%.c | $(BUILD_DIR)
# 	@echo "Compiling $<..."
# 	$(CC) $(CFLAGS) -c $< -o $@

# # Clean build files
# clean:
# 	@echo "Cleaning build artifacts..."
# 	rm -rf $(BUILD_DIR)
# 	rm -rf $(BIN_DIR)
# 	@echo "✓ Clean complete"

# # Clean everything including storage
# cleanall: clean
# 	@echo "Cleaning storage directory..."
# 	rm -rf storage
# 	@echo "✓ Clean all complete"

# # Run the storage server (default configuration)
# run: $(TARGET)
# 	@echo "Starting Storage Server..."
# 	./$(TARGET)

# # Rebuild everything
# rebuild: clean all

# # Debug build (with debug symbols and verbose output)
# debug: CFLAGS += -DDEBUG -g3 -O0
# debug: clean all
# 	@echo "✓ Debug build complete"

# # Test target - compiles everything and runs basic checks
# test: all
# 	@echo "Running basic tests..."
# 	@if [ -f $(TARGET) ]; then \
#         echo "✓ Binary exists"; \
#         ls -lh $(TARGET); \
#     else \
#         echo "✗ Binary not found"; \
#         exit 1; \
#     fi
# 	@echo "✓ All tests passed"

# # Show build information
# info:
# 	@echo "=== Build Information ==="
# 	@echo "Compiler: $(CC)"
# 	@echo "Flags: $(CFLAGS)"
# 	@echo "Linker Flags: $(LDFLAGS)"
# 	@echo "Target: $(TARGET)"
# 	@echo "Object Files:"
# 	@echo "  $(ALL_OBJS)" | tr ' ' '\n' | sed 's/^/  /'
# 	@echo "========================="

# # Help target
# help:
# 	@echo "Available targets:"
# 	@echo "  all         - Build the storage server (default)"
# 	@echo "  clean       - Remove build artifacts"
# 	@echo "  cleanall    - Remove build artifacts and storage"
# 	@echo "  run         - Build and run with default configuration"
# 	@echo "  run-custom  - Build and run with custom configuration"
# 	@echo "  debug       - Build with debug symbols and no optimization"
# 	@echo "  test        - Build and run basic tests"
# 	@echo "  info        - Show build configuration information"
# 	@echo "  help        - Show this help message"
# 	@echo ""
# 	@echo "Source files included:"
# 	@echo "  - file_structure_1.c"
# 	@echo "  - file_structure_2.c"
# 	@echo "  - main.c"
# 	@echo "  - $(C_SS_SRC)"
# 	@echo "  - $(C_SS_SRC_2)"
# 	@echo "  - $(NS_SS_SRC)"
# 	@echo ""
# 	@echo "Example custom run:"
# 	@echo "  make run-custom NS_IP=192.168.1.100 NS_PORT=8080 NM_PORT=9001 CLIENT_PORT=9002"

# .PHONY: all directories clean cleanall run run-custom debug test info help

CC = gcc
CFLAGS = -Wall -Wextra -pthread -g -I. -I.. -I../api_ns_ss -I../api_c_ss
LDFLAGS = -pthread

# Directories
API_NS_SS_DIR = ../api_ns_ss
API_C_SS_DIR = ../api_c_ss
API_C_NS_DIR = ../api_c_ns
BUILD_DIR = build
BIN_DIR = bin

# Source files from current directory (ss folder)
SS_LOCAL_SRCS = file_structure_1.c file_structure_2.c file_structure_3.c main.c

# Source files from API directories
NS_SS_SRC = $(API_NS_SS_DIR)/ns_ss_connection.c
C_SS_SRC = $(API_C_SS_DIR)/client_ss_connection.c
C_SS_SRC_2 = $(API_C_SS_DIR)/client_ss_connection_2.c
C_SS_SRC_3 = $(API_C_SS_DIR)/client_ss_connection_3.c

# Object files
SS_LOCAL_OBJS = $(BUILD_DIR)/file_structure_1.o $(BUILD_DIR)/file_structure_2.o $(BUILD_DIR)/file_structure_3.o $(BUILD_DIR)/storage_server_main.o
NS_SS_OBJ = $(BUILD_DIR)/ns_ss_connection.o
C_SS_OBJ = $(BUILD_DIR)/client_ss_connection.o
C_SS_OBJ_2 = $(BUILD_DIR)/client_ss_connection_2.o
C_SS_OBJ_3 = $(BUILD_DIR)/client_ss_connection_3.o

# All object files
ALL_OBJS = $(SS_LOCAL_OBJS) $(NS_SS_OBJ) $(C_SS_OBJ) $(C_SS_OBJ_2) $(C_SS_OBJ_3)

# Output binary
TARGET = $(BIN_DIR)/ss_server

# Default target
all: directories $(TARGET)

# Create necessary directories
directories:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(BIN_DIR)
	@mkdir -p storage
	@mkdir -p storage/.checkpoints

# Link object files to create executable
$(TARGET): $(ALL_OBJS)
	@echo "Linking $@..."
	$(CC) -o $@ $^ $(API_C_NS_DIR)/networking.o $(LDFLAGS)
	@echo "Build complete: $@"

# Compile local SS files
$(BUILD_DIR)/file_structure_1.o: file_structure_1.c file_structure.h | $(BUILD_DIR)
	@echo "Compiling file_structure_1.c..."
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/file_structure_2.o: file_structure_2.c file_structure.h | $(BUILD_DIR)
	@echo "Compiling file_structure_2.c..."
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/file_structure_3.o: file_structure_3.c file_structure.h | $(BUILD_DIR)
	@echo "Compiling file_structure_3.c..."
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/storage_server_main.o: main.c file_structure.h | $(BUILD_DIR)
	@echo "Compiling main.c..."
	$(CC) $(CFLAGS) -c $< -o $@

# Compile NS-SS API
$(NS_SS_OBJ): $(NS_SS_SRC) $(API_NS_SS_DIR)/ns_ss_connection.h | $(BUILD_DIR)
	@echo "Compiling ns_ss_connection.c..."
	$(CC) $(CFLAGS) -c $< -o $@

# Compile Client-SS API
$(C_SS_OBJ): $(C_SS_SRC) $(API_C_SS_DIR)/client_ss_connection.h | $(BUILD_DIR)
	@echo "Compiling client_ss_connection.c..."
	$(CC) $(CFLAGS) -c $< -o $@

$(C_SS_OBJ_2): $(C_SS_SRC_2) $(API_C_SS_DIR)/client_ss_connection.h | $(BUILD_DIR)
	@echo "Compiling client_ss_connection_2.c..."
	$(CC) $(CFLAGS) -c $< -o $@

$(C_SS_OBJ_3): $(C_SS_SRC_3) $(API_C_SS_DIR)/client_ss_connection.h file_structure.h | $(BUILD_DIR)
	@echo "Compiling client_ss_connection_3.c..."
	$(CC) $(CFLAGS) -c $< -o $@

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
	./$(TARGET) 127.0.0.1 9091 9001 9002 ./storage

# Help target
help:
	@echo "Available targets:"
	@echo "  all         - Build the storage server (default)"
	@echo "  clean       - Remove build artifacts"
	@echo "  cleanall    - Remove build artifacts and storage"
	@echo "  run         - Build and run with default configuration"
	@echo "  help        - Show this help message"

.PHONY: all clean cleanall run help directories