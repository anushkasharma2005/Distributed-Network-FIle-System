# Naming Server (NS) Component

This directory contains the implementation of the Naming Server (NS) that receives messages from clients and prints them to the terminal.

## Features

- ✅ Accepts multiple client connections (one at a time)
- ✅ Receives and displays messages from clients
- ✅ Sends acknowledgment back to clients
- ✅ Graceful shutdown with Ctrl+C
- ✅ Uses modular networking API from `api_c_ns/`

## Building

### From project root:
```bash
make ns          # Build only NS component
make             # Build all components including NS
```

### From ns directory:
```bash
make             # Build NS server
make run         # Build and run NS server
make clean       # Clean build files
```

## Running

### Start the NS server:
```bash
# From ns directory
./ns_server

# Or using make
make run
```

The server will start listening on port **9090**.

### Test with a client:
You can test the NS server using the provided test script:

```bash
# From ns directory (in another terminal)
chmod +x test_ns_client.sh
./test_ns_client.sh "Your custom message here"
```

Or use the test client from `api_c_ns/`:
```bash
cd ../api_c_ns
./test_client
```

## Configuration

Edit `main.c` to change:
- **Port**: Change `NS_PORT` (default: 9090)
- **Backlog**: Change `BACKLOG` (default: 10)
- **Buffer Size**: Change `BUFFER_SIZE` (default: 4096)

## Usage Example

**Terminal 1 - Start NS Server:**
```bash
cd ns
make run
```

**Terminal 2 - Send a message:**
```bash
cd ns
./test_ns_client.sh "Hello NS Server!"
```

**Expected Output on NS Server:**
```
╔════════════════════════════════════════╗
║    NAMING SERVER (NS) STARTED         ║
╚════════════════════════════════════════╝

[NS] Initializing server on port 9090...
[SERVER] Listening on port 9090 (backlog: 10)
[NS] Server initialized successfully!
[NS] Waiting for client connections...
═══════════════════════════════════════════

[SERVER] Accepted connection from 127.0.0.1:52341 (fd: 4)

[NS] ✓ New client connected from 127.0.0.1:52341
═══════════════════════════════════════════

┌─ Message from 127.0.0.1:52341 ─────────────
│ Length: 17 bytes
│ Content: Hello NS Server!
└────────────────────────────────────────
[NS] ✓ Acknowledgment sent to client
[NS] Connection with 127.0.0.1:52341 closed.
═══════════════════════════════════════════
[NS] Waiting for next client...
```

## Stop the Server

Press `Ctrl+C` for graceful shutdown.
