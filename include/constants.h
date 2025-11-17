#ifndef PROJECT_CONSTANTS_H
#define PROJECT_CONSTANTS_H

// ============================================
// BUFFER SIZES
// ============================================
// Global buffer size used across components
#define MAX_BUFFER_SIZE 8192
#define MAX_PATH_LEN 256
#define MAX_FILES 1000






// ============================================
// NAMING SERVER - CLIENT CONFIGURATION
// ============================================
// Port number for client connections to Naming Server
#define NS_CLIENT_PORT 9090

// Maximum number of pending client connections
#define NS_CLIENT_BACKLOG 10

// Size of buffer for client data transfer
#define NS_CLIENT_BUFFER_SIZE 4096



// ============================================
// NAMING SERVER - STORAGE SERVER CONFIGURATION
// ============================================
// Port number for Storage Server connections to Naming Server
#define NS_SS_PORT 9091

// Maximum number of pending Storage Server connections
#define NS_SS_BACKLOG 20

// Size of buffer for Storage Server data transfer
#define NS_SS_BUFFER_SIZE 8192




// ============================================
// STORAGE SERVER CONFIGURATION
// ============================================
// Default Storage Server IP
#define DEFAULT_SS_IP "127.0.0.1"

// Default NM Port for Storage Server
#define DEFAULT_SS_NM_PORT 9002

// Default Client Port for Storage Server
#define DEFAULT_SS_CLIENT_PORT 9003

// Default base path for file storage
#define DEFAULT_SS_BASE_PATH "./storage"




// ============================================
// CLIENT CONFIGURATION
// ============================================
// Maximum command length for client input
#define MAX_COMMAND_LENGTH 1024

// Default Naming Server IP for client
#define DEFAULT_NS_IP "127.0.0.1"

// Default Naming Server port for client connections
#define DEFAULT_NS_PORT NS_CLIENT_PORT





// Maximum number of storage servers and hash table size
#define MAX_STORAGE_SERVERS 100
#define HASH_TABLE_SIZE 128  // Should be prime or power of 2


#define FILE_HASH_TABLE_SIZE 1024  // Prime number for better distribution

#define NS_CMD_CREATE  1
#define NS_CMD_DELETE  2
#define NS_CMD_COPY    3



// Heartbeat monitoring configuration
#define SS_HEARTBEAT_INTERVAL_SEC 5      // Check SS connection every 5 seconds
#define SS_HEARTBEAT_TIMEOUT_SEC 15      // Mark SS inactive after 15 seconds of no response

// TCP Keepalive configuration (if using OS-level keepalive)
#define TCP_KEEPALIVE_TIME 10            // Start probes after 10s idle
#define TCP_KEEPALIVE_INTERVAL 5         // Probe every 5s
#define TCP_KEEPALIVE_PROBES 3           // 3 failed probes = dead

#endif // PROJECT_CONSTANTS_H