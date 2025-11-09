# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -g -I../api_c_ns
LDFLAGS = -pthread

# API directory
API_DIR = ../api_c_ns

# Source files
NS_SRC = main.c conn.c handle_client.c handle_ss.c ss_registry.c
API_SRC = $(API_DIR)/networking.c $(API_DIR)/naming_server.c

# Object files
NS_OBJ = $(NS_SRC:.c=.o)
API_OBJ = $(API_DIR)/networking.o $(API_DIR)/naming_server.o

# Executable
TARGET = ns_server

# Default target
all: $(TARGET)

# Build the NS server executable
$(TARGET): $(NS_OBJ) $(API_OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)
	@echo "✓ NS server built successfully!"

# Compile object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Build API object files if needed
$(API_DIR)/%.o: $(API_DIR)/%.c
	$(MAKE) -C $(API_DIR) $(notdir $@)

# Clean build files
clean:
	rm -f $(NS_OBJ) $(TARGET)
	@echo "✓ NS build files cleaned"

# Run the NS server
run: $(TARGET)
	./$(TARGET)

# Rebuild everything
rebuild: clean all

.PHONY: all clean run rebuild