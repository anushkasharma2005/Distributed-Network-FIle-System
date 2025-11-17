# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -g -I../api_c_ns -I. -Iclient_specific -Iss_specific -Iregistry
LDFLAGS = -pthread

# API directory
API_DIR = ../api_c_ns

# Subdirectories
CLIENT_DIR = client_specific
SS_DIR = ss_specific
REGISTRY_DIR = registry

# Header dependencies
HEADERS = types.h conn.h \
          $(CLIENT_DIR)/handle_client_server.h $(CLIENT_DIR)/client_commands.h \
          $(SS_DIR)/handle_ss.h \
          $(REGISTRY_DIR)/ss_registry.h $(REGISTRY_DIR)/file_registry.h $(REGISTRY_DIR)/ss_selector.h

# Source files
NS_SRC = main.c conn.c
CLIENT_SRC = $(CLIENT_DIR)/handle_client_server.c $(CLIENT_DIR)/client_commands.c
SS_SRC = $(SS_DIR)/handle_ss.c
REGISTRY_SRC = $(REGISTRY_DIR)/ss_registry.c $(REGISTRY_DIR)/file_registry.c $(REGISTRY_DIR)/ss_selector.c $(REGISTRY_DIR)/user_registry.c
API_SRC = $(API_DIR)/networking.c $(API_DIR)/naming_server.c

# Object files
NS_OBJ = $(NS_SRC:.c=.o)
CLIENT_OBJ = $(CLIENT_SRC:.c=.o)
SS_OBJ = $(SS_SRC:.c=.o)
REGISTRY_OBJ = $(REGISTRY_SRC:.c=.o)
API_OBJ = $(API_DIR)/networking.o $(API_DIR)/naming_server.o

ALL_OBJ = $(NS_OBJ) $(CLIENT_OBJ) $(SS_OBJ) $(REGISTRY_OBJ)

# Executable
TARGET = ns_server

# Default target
all: $(TARGET)

# Build the NS server executable
$(TARGET): $(ALL_OBJ) $(API_OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)
	@echo "✓ NS server built successfully!"

# Compile object files with header dependencies
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Build API object files if needed
$(API_DIR)/%.o: $(API_DIR)/%.c
	$(MAKE) -C $(API_DIR) $(notdir $@)

# Clean build files
clean:
	rm -f $(NS_OBJ) $(CLIENT_OBJ) $(SS_OBJ) $(REGISTRY_OBJ) $(TARGET)
	@echo "✓ NS build files cleaned"

# Run the NS server
run: $(TARGET)
	./$(TARGET)

# Rebuild everything
rebuild: clean all

# Debug target to show variables
debug:
    @echo "NS_SRC: $(NS_SRC)"
    @echo "CLIENT_SRC: $(CLIENT_SRC)"
    @echo "SS_SRC: $(SS_SRC)"
    @echo "REGISTRY_SRC: $(REGISTRY_SRC)"
    @echo "ALL_OBJ: $(ALL_OBJ)"

.PHONY: all clean run rebuild debug