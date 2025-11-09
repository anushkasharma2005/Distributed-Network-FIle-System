hmmCC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = -pthread

# Sources
CLIENT_SRCS := $(wildcard *.c)

# API objects from api_c_ns produced by its Makefile
API_OBJS := ../api_c_ns/networking.o ../api_c_ns/client_api.o ../api_c_ns/naming_server.o

TARGET := client_app

all: $(TARGET)

$(TARGET): $(CLIENT_SRCS) $(API_OBJS)
	@echo "Building client executable..."
	$(CC) $(CFLAGS) $(CLIENT_SRCS) $(API_OBJS) -o $(TARGET) -I.. -I../api_c_ns $(LDFLAGS)
	@echo "Built $(TARGET)"

test_ss: test_ss_connection.c
	gcc -Wall -Wextra -g -I.. -I../api_c_ss test_ss_connection.c -o test_ss_client

clean_test:
	rm -f test_ss_client

clean:
	@echo "Cleaning client..."
	-@rm -f $(TARGET)

.PHONY: all clean
