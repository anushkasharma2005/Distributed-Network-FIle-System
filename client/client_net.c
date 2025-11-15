#include "client.h"
#include <stdio.h>
#include <string.h>

// Use api_c_ns client API and networking utilities
#include "../api_c_ns/client_api.h"
#include "../api_c_ns/networking.h"
# include "../api_c_ss/client_ss_connection.h"
// Initialize client structure
int client_init_struct(Client *client, const char *nm_ip, int nm_port, const char *username)
{
    if (!client || !nm_ip || !username)
        return ERR_INVALID_COMMAND;

    memset(client, 0, sizeof(Client));
    client->nm_socket = -1;
    client->ss_socket = -1;
    client->nm_port = nm_port;
    client->client_port = -1;
    strncpy(client->nm_ip, nm_ip, sizeof(client->nm_ip) - 1);
    strncpy(client->username, username, sizeof(client->username) - 1);
    client->connected = false;

    return SUCCESS;
}

void client_cleanup(Client *client)
{
    if (!client)
        return;
    if (client->ss_socket >= 0)
    {
        close_client(client->ss_socket);
        client->ss_socket = -1;
    }
    if (client->nm_socket >= 0)
    {
        close_client(client->nm_socket);
        client->nm_socket = -1;
    }
    client->connected = false;
}

int client_connect_to_nm(Client *client)
{
    if (!client)
        return ERR_INVALID_COMMAND;
    if (client->nm_socket >= 0)
        return SUCCESS; // already connected
    printf("p1\n");
    int fd = init_client(client->nm_ip, client->nm_port);
    if (fd < 0)
    {
        return ERR_CONNECTION;
    }
    client->nm_socket = fd;
    client->connected = true;
    return SUCCESS;
}

int client_connect_to_ss(Client *client, const char *ss_ip, int ss_port)
{
    if (!client || !ss_ip)
        return ERR_INVALID_COMMAND;

    // If already connected to the requested SS, reuse it
    if (client->ss_socket >= 0)
    {
        // Optionally: check peer info to validate it's the same SS
        return SUCCESS;
    }

    int fd = init_client(ss_ip, ss_port);
    if (fd < 0)
    {
        return ERR_CONNECTION;
    }
    client->ss_socket = fd;
    return SUCCESS;
}

void client_disconnect_from_ss(Client *client)
{
    if (!client)
        return;
    if (client->ss_socket >= 0)
    {
        close_client(client->ss_socket);
        client->ss_socket = -1;
    }
}

// Sending/receiving wrappers using networking API
int send_to_nm(Client *client, const void *data, size_t len)
{
    if (!client || client->nm_socket < 0 || !data)
        return ERR_CONNECTION;

    // networking's send_message expects a null-terminated string
    // Ensure temporary buffer is null-terminated
    const char *msg = (const char *)data;
    int sent = send_message(client->nm_socket, msg);
    if (sent <= 0)
        return ERR_CONNECTION;
    return SUCCESS;
}

int recv_from_nm(Client *client, void *buffer, size_t len)
{
    if (!client || client->nm_socket < 0 || !buffer)
        return ERR_CONNECTION;
    int r = recv_message(client->nm_socket, (char *)buffer, len);
    return r; // caller expects <=0 on error/closed, >0 bytes
}

int send_to_ss(Client *client, const void *data, size_t len)
{
    if (!client || client->ss_socket < 0 || !data)
        return ERR_CONNECTION;
    const char *msg = (const char *)data;
    int sent = send_message(client->ss_socket, msg);
    if (sent <= 0)
        return ERR_CONNECTION;
    return SUCCESS;
}

int recv_from_ss(Client *client, void *buffer, size_t len)
{
    if (!client || client->ss_socket < 0 || !buffer)
        return ERR_CONNECTION;
    int r = recv_message(client->ss_socket, (char *)buffer, len);
    return r;
}

// Helper function to send ClientRequest to SS
int send_request_to_ss(Client *client, ClientRequest *req) {
    if (send(client->ss_socket, req, sizeof(ClientRequest), 0) < 0) {
        perror("Failed to send request to SS");
        return ERR_CONNECTION;
    }
    return SUCCESS;
}

// Helper function to receive ClientRequest from SS
int recv_response_from_ss(Client *client, ClientRequest *resp) {
    int bytes = recv(client->ss_socket, resp, sizeof(ClientRequest), 0);
    if (bytes <= 0) {
        perror("Failed to receive response from SS");
        return ERR_CONNECTION;
    }
    return SUCCESS;
}