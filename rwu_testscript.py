#!/usr/bin/env python3
"""
Comprehensive test script for Storage Server operations
Tests: READ, WRITE (BEGIN/UPDATE/END), STREAM, UNDO
"""

import socket
import struct
import time
import sys

# Configuration
SS_IP = "127.0.0.1"
SS_PORT = 9002
TEST_FILE = "test.txt"
USERNAME = "testuser"

# Operation types
OP_READ = 1
OP_WRITE_BEGIN = 3
OP_WRITE_UPDATE = 4
OP_WRITE_END = 5
OP_STREAM = 6
OP_UNDO = 7
OP_ACK = 100
OP_ERROR = 101
OP_STOP = 102

# Color codes
RED = '\033[0;31m'
GREEN = '\033[0;32m'
YELLOW = '\033[1;33m'
BLUE = '\033[0;34m'
NC = '\033[0m'

CLIENT_REQUEST_SIZE = 1848  # Total size

def pack_request(op_type, filename, username=USERNAME, sentence_num=0, word_index=0, content=""):
    """Pack a ClientRequest structure matching the C struct exactly"""
    request = bytearray(CLIENT_REQUEST_SIZE)
    
    offset = 0
    
    # op_type (int, 4 bytes)
    struct.pack_into('i', request, offset, op_type)
    offset += 4
    
    # filename (char[256])
    filename_bytes = filename.encode('utf-8')[:255]
    request[offset:offset+len(filename_bytes)] = filename_bytes
    offset += 256
    
    # username (char[64])
    username_bytes = username.encode('utf-8')[:63]
    request[offset:offset+len(username_bytes)] = username_bytes
    offset += 64
    
    # sentence_num (int, 4 bytes)
    struct.pack_into('i', request, offset, sentence_num)
    offset += 4
    
    # word_index (int, 4 bytes)
    struct.pack_into('i', request, offset, word_index)
    offset += 4
    
    # content (char[1000])
    content_bytes = content.encode('utf-8')[:999]
    request[offset:offset+len(content_bytes)] = content_bytes
    offset += 1000
    
    # status (int, 4 bytes)
    struct.pack_into('i', request, offset, 0)
    offset += 4
    
    # error_msg (char[512])
    offset += 512
    
    return bytes(request)

def recv_full(sock, size):
    """Receive exactly 'size' bytes from socket"""
    data = bytearray()
    while len(data) < size:
        packet = sock.recv(size - len(data))
        if not packet:
            return None
        data.extend(packet)
    return bytes(data)

def unpack_response(data):
    """Unpack a ClientRequest structure"""
    if len(data) < CLIENT_REQUEST_SIZE:
        # Pad with zeros if needed
        data = data + b'\x00' * (CLIENT_REQUEST_SIZE - len(data))
    
    offset = 0
    
    # op_type (int, 4 bytes)
    op_type = struct.unpack_from('i', data, offset)[0]
    offset += 4
    
    # filename (char[256])
    filename = data[offset:offset+256].decode('utf-8', errors='ignore').rstrip('\x00')
    offset += 256
    
    # username (char[64])
    username = data[offset:offset+64].decode('utf-8', errors='ignore').rstrip('\x00')
    offset += 64
    
    # sentence_num (int, 4 bytes)
    sentence_num = struct.unpack_from('i', data, offset)[0]
    offset += 4
    
    # word_index (int, 4 bytes)
    word_index = struct.unpack_from('i', data, offset)[0]
    offset += 4
    
    # content (char[1000])
    content = data[offset:offset+1000].decode('utf-8', errors='ignore').rstrip('\x00')
    offset += 1000
    
    # status (int, 4 bytes)
    status = struct.unpack_from('i', data, offset)[0]
    offset += 4
    
    # error_msg (char[512])
    error_msg = data[offset:offset+512].decode('utf-8', errors='ignore').rstrip('\x00')
    offset += 512
    
    return {
        'op_type': op_type,
        'filename': filename,
        'username': username,
        'sentence_num': sentence_num,
        'word_index': word_index,
        'content': content,
        'status': status,
        'error_msg': error_msg
    }

def connect_to_ss():
    """Connect to Storage Server"""
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(10)  # 10 second timeout
    sock.connect((SS_IP, SS_PORT))
    return sock

def test_read():
    """Test READ operation"""
    print(f"\n{YELLOW}>>> Test 1: READ Operation{NC}")
    try:
        sock = connect_to_ss()
        print(f"{BLUE}Connected to SS at {SS_IP}:{SS_PORT}{NC}")
        
        # Send READ request
        request = pack_request(OP_READ, TEST_FILE)
        sock.sendall(request)
        print(f"Sent READ request for {TEST_FILE}")
        
        # Receive response - ensure we get full response
        data = recv_full(sock, CLIENT_REQUEST_SIZE)
        if not data:
            print(f"{RED}✗ Failed to receive complete response{NC}")
            sock.close()
            return False
            
        resp = unpack_response(data)
        
        if resp['status'] == 0:
            print(f"{GREEN}✓ READ successful{NC}")
            print(f"Content:\n{'-'*40}\n{resp['content']}\n{'-'*40}")
        else:
            print(f"{RED}✗ READ failed: {resp['error_msg']}{NC}")
        
        sock.close()
        return resp['status'] == 0
    except Exception as e:
        print(f"{RED}✗ Error: {e}{NC}")
        return False

def test_write():
    """Test WRITE operation (BEGIN -> UPDATE -> END)"""
    print(f"\n{YELLOW}>>> Test 2: WRITE Operation{NC}")
    try:
        sock = connect_to_ss()
        print(f"{BLUE}Connected to SS{NC}")
        
        # WRITE_BEGIN
        print("Sending WRITE_BEGIN...")
        request = pack_request(OP_WRITE_BEGIN, TEST_FILE, sentence_num=0)
        sock.sendall(request)
        
        data = recv_full(sock, CLIENT_REQUEST_SIZE)
        if not data:
            print(f"{RED}✗ Failed to receive response{NC}")
            sock.close()
            return False
            
        resp = unpack_response(data)
        if resp['status'] != 0:
            print(f"{RED}✗ WRITE_BEGIN failed: {resp['error_msg']}{NC}")
            sock.close()
            return False
        print(f"{GREEN}✓ WRITE_BEGIN successful: {resp['error_msg']}{NC}")
        
        # WRITE_UPDATE (update word 0)
        print("Sending WRITE_UPDATE (word 0 -> 'Modified')...")
        request = pack_request(OP_WRITE_UPDATE, TEST_FILE, sentence_num=0, 
                             word_index=0, content="Modified")
        sock.sendall(request)
        
        data = recv_full(sock, CLIENT_REQUEST_SIZE)
        if not data:
            print(f"{RED}✗ Failed to receive response{NC}")
            sock.close()
            return False
            
        resp = unpack_response(data)
        if resp['status'] != 0:
            print(f"{RED}✗ WRITE_UPDATE failed: {resp['error_msg']}{NC}")
        else:
            print(f"{GREEN}✓ WRITE_UPDATE successful: {resp['error_msg']}{NC}")
        
        # WRITE_UPDATE (update word 1)
        print("Sending WRITE_UPDATE (word 1 -> 'Text')...")
        request = pack_request(OP_WRITE_UPDATE, TEST_FILE, sentence_num=0, 
                             word_index=1, content="Text")
        sock.sendall(request)
        
        data = recv_full(sock, CLIENT_REQUEST_SIZE)
        if not data:
            print(f"{RED}✗ Failed to receive response{NC}")
            sock.close()
            return False
            
        resp = unpack_response(data)
        if resp['status'] != 0:
            print(f"{RED}✗ WRITE_UPDATE failed: {resp['error_msg']}{NC}")
        else:
            print(f"{GREEN}✓ WRITE_UPDATE successful: {resp['error_msg']}{NC}")
        
        # WRITE_END
        print("Sending WRITE_END...")
        request = pack_request(OP_WRITE_END, TEST_FILE, sentence_num=0)
        sock.sendall(request)
        
        data = recv_full(sock, CLIENT_REQUEST_SIZE)
        if not data:
            print(f"{RED}✗ Failed to receive response{NC}")
            sock.close()
            return False
            
        resp = unpack_response(data)
        if resp['status'] != 0:
            print(f"{RED}✗ WRITE_END failed: {resp['error_msg']}{NC}")
            sock.close()
            return False
        print(f"{GREEN}✓ WRITE_END successful: {resp['error_msg']}{NC}")
        
        sock.close()
        return True
    except Exception as e:
        print(f"{RED}✗ Error: {e}{NC}")
        return False

def test_stream():
    """Test STREAM operation"""
    print(f"\n{YELLOW}>>> Test 3: STREAM Operation{NC}")
    try:
        sock = connect_to_ss()
        sock.settimeout(15)  # Longer timeout for streaming
        print(f"{BLUE}Connected to SS{NC}")
        
        # Send STREAM request
        request = pack_request(OP_STREAM, TEST_FILE)
        sock.sendall(request)
        print(f"Sent STREAM request for {TEST_FILE}")
        print(f"\nStreaming content:\n{'-'*40}")
        
        word_count = 0
        while True:
            try:
                # Receive full response for each word
                data = recv_full(sock, CLIENT_REQUEST_SIZE)
                if not data:
                    print(f"\n{RED}✗ Connection closed by server{NC}")
                    break
                
                resp = unpack_response(data)
                
                if resp['op_type'] == OP_STOP:
                    print(f"\n{'-'*40}")
                    print(f"{GREEN}✓ Stream completed. Total words: {word_count}{NC}")
                    break
                elif resp['op_type'] == OP_ERROR:
                    print(f"\n{RED}✗ Stream failed: {resp['error_msg']}{NC}")
                    return False
                elif resp['op_type'] == OP_ACK and resp['status'] == 0:
                    if resp['content'].strip():  # Only print non-empty content
                        print(resp['content'], end=' ', flush=True)
                        word_count += 1
                    time.sleep(0.02)  # Small delay for display
                else:
                    # Silently ignore unexpected responses during stream
                    pass
                    
            except socket.timeout:
                print(f"\n{RED}✗ Timeout while streaming{NC}")
                break
            except Exception as e:
                print(f"\n{RED}✗ Error during streaming: {e}{NC}")
                break
        
        sock.close()
        return True
    except Exception as e:
        print(f"\n{RED}✗ Error: {e}{NC}")
        return False

def test_undo():
    """Test UNDO operation"""
    print(f"\n{YELLOW}>>> Test 4: UNDO Operation{NC}")
    try:
        sock = connect_to_ss()
        print(f"{BLUE}Connected to SS{NC}")
        
        # Send UNDO request
        request = pack_request(OP_UNDO, TEST_FILE)
        sock.sendall(request)
        print(f"Sent UNDO request for {TEST_FILE}")
        
        # Receive response
        data = recv_full(sock, CLIENT_REQUEST_SIZE)
        if not data:
            print(f"{RED}✗ Failed to receive response{NC}")
            sock.close()
            return False
            
        resp = unpack_response(data)
        
        if resp['status'] == 0:
            msg = resp['error_msg'] if resp['error_msg'] else "Undo successful"
            print(f"{GREEN}✓ UNDO successful: {msg}{NC}")
        else:
            print(f"{RED}✗ UNDO failed: {resp['error_msg']}{NC}")
        
        sock.close()
        return resp['status'] == 0
    except Exception as e:
        print(f"{RED}✗ Error: {e}{NC}")
        return False

def main():
    print(f"{BLUE}{'='*50}{NC}")
    print(f"{BLUE}  Storage Server Comprehensive Test Suite{NC}")
    print(f"{BLUE}{'='*50}{NC}")
    print(f"SS Address: {SS_IP}:{SS_PORT}")
    print(f"Test File: {TEST_FILE}")
    print(f"Username: {USERNAME}")
    
    results = {
        'READ (Initial)': test_read(),
        'WRITE': test_write(),
        'READ (After Write)': test_read(),
        'STREAM': test_stream(),
        'UNDO': test_undo(),
        'READ (After Undo)': test_read()
    }
    
    print(f"\n{BLUE}{'='*50}{NC}")
    print(f"{BLUE}  Test Summary{NC}")
    print(f"{BLUE}{'='*50}{NC}")
    
    for test_name, result in results.items():
        status = f"{GREEN}PASS{NC}" if result else f"{RED}FAIL{NC}"
        print(f"{test_name}: {status}")
    
    passed = sum(results.values())
    total = len(results)
    print(f"\n{BLUE}Total: {passed}/{total} tests passed{NC}")
    
    return 0 if passed == total else 1

if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print(f"\n{YELLOW}Test interrupted by user{NC}")
        sys.exit(1)