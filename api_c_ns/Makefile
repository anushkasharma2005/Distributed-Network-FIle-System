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

# Test executables
TEST_SERVER = test_server
TEST_CLIENT = test_client

# Default target
all: $(TEST_SERVER) $(TEST_CLIENT)

# Compile object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Build test server
$(TEST_SERVER): test_server.c $(NETWORKING_OBJ) $(SERVER_API_OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Build test client
$(TEST_CLIENT): test_client.c $(NETWORKING_OBJ) $(CLIENT_API_OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

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
