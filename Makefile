# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = -pthread

# Directories
API_C_NS_DIR = api_c_ns
API_C_SS_DIR = api_c_ss
API_NS_SS_DIR = api_ns_ss
CLIENT_DIR = client
NS_DIR = ns
SS_DIR = ss

# Subdirectories to build (add more as needed)
SUBDIRS = $(API_C_NS_DIR) $(NS_DIR)
# Future additions:
# SUBDIRS = $(API_C_NS_DIR) $(API_C_SS_DIR) $(API_NS_SS_DIR) $(CLIENT_DIR) $(NS_DIR) $(SS_DIR)

# Client build (client/ has no Makefile) - build at root level
CLIENT_SRCS := $(wildcard $(CLIENT_DIR)/*.c)
# Only include the API files needed (exclude test files)
API_C_NS_SRCS := $(API_C_NS_DIR)/networking.c $(API_C_NS_DIR)/client_api.c $(API_C_NS_DIR)/naming_server.c
CLIENT_BIN := $(CLIENT_DIR)/client_app

# Default target - build all subdirectories
all:
	@echo "=== Building all components ==="
	@for dir in $(SUBDIRS); do \
		if [ -f $$dir/Makefile ]; then \
			echo "Building $$dir..."; \
			$(MAKE) -C $$dir || exit 1; \
		fi; \
	done
	@echo "Building client..."
	$(MAKE) client || exit 1
	@echo "=== Build complete ==="

# Clean all subdirectories
clean:
	@echo "=== Cleaning all components ==="
	@for dir in $(SUBDIRS); do \
		if [ -f $$dir/Makefile ]; then \
			echo "Cleaning $$dir..."; \
			$(MAKE) -C $$dir clean; \
		fi; \
	done
	@$(MAKE) clean-client >/dev/null 2>&1 || true
	@echo "=== Clean complete ==="

# Rebuild everything
rebuild: clean all

# Build specific component
api_c_ns:
	@echo "=== Building api_c_ns ==="
	@$(MAKE) -C $(API_C_NS_DIR)

ns:
	@echo "=== Building ns ==="
	@$(MAKE) -C $(NS_DIR)

# Future targets (uncomment when ready)
# api_c_ss:
# 	@$(MAKE) -C $(API_C_SS_DIR)
#
# api_ns_ss:
# 	@$(MAKE) -C $(API_NS_SS_DIR)
#
# client:
# 	@$(MAKE) -C $(CLIENT_DIR)
#
# ns:
# 	@$(MAKE) -C $(NS_DIR)
#
# ss:
# 	@$(MAKE) -C $(SS_DIR)

# Build client executable (root-level)
client: $(CLIENT_BIN)

$(CLIENT_BIN): $(CLIENT_SRCS) $(API_C_NS_SRCS)
	@echo "Compiling client and api_c_ns sources..."
	$(CC) $(CFLAGS) $(CLIENT_SRCS) $(API_C_NS_SRCS) -o $(CLIENT_BIN) -I. $(LDFLAGS)
	@echo "Client built: $(CLIENT_BIN)"

clean-client:
	@echo "Cleaning client build..."
	-@rm -f $(CLIENT_BIN)

# Help target
help:
	@echo "Available targets:"
	@echo "  all          - Build all components"
	@echo "  clean        - Clean all build artifacts"
	@echo "  rebuild      - Clean and rebuild everything"
	@echo "  api_c_ns     - Build only api_c_ns"
	@echo "  ns           - Build only ns (naming server)"
	@echo ""
	@echo "Current subdirectories: $(SUBDIRS)"

.PHONY: all clean rebuild api_c_ns ns help
