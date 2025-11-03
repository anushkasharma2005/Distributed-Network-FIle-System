# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -g -I.
LDFLAGS = -pthread

# Source files
NETWORKING_SRC = networking.c
CLIENT_API_SRC = client_api.c
SERVER_API_SRC = naming_server.c

# Object files
NETWORKING_OBJ = $(NETWORKING_SRC:.c=.o)
CLIENT_API_OBJ = $(CLIENT_API_SRC:.c=.o)
SERVER_API_OBJ = $(SERVER_API_SRC:.c=.o)

# Default target: build object files for integration
all: $(NETWORKING_OBJ) $(CLIENT_API_OBJ) $(SERVER_API_OBJ)

# Compile object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# (Optional) If you have test_server.c/test_client.c, you can build them
# by invoking the specific targets. The default 'all' target only builds
# the reusable object files for use by other components.

# Clean build files
clean:
	rm -f *.o $(TEST_SERVER) $(TEST_CLIENT)

# Run server test
run-server: $(TEST_SERVER)
	./$(TEST_SERVER)

# Run client test (assumes server is running)
run-client: $(TEST_CLIENT)
	./$(TEST_CLIENT)

# Rebuild everything
rebuild: clean all

.PHONY: all clean run-server run-client rebuild
