/*
 * Run: ./server [port]
 * Default port 2121
 * Supports multithread - multiple clients connected simultaneously
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "../include/account.h"
#include "../include/ftp_server.h"

#define DEFAULT_PORT 2121
#define ACCOUNT_FILE "./server/data/accounts.txt"

// Struct to pass data to thread
typedef struct {
    int client_sock;
    struct sockaddr_in client_addr;
} ClientInfo;

/*
 * Handler function for each client thread
 */
void *client_thread(void *arg) {
    ClientInfo *info = (ClientInfo *)arg;
    
    // Handle client
    handle_client(info->client_sock, info->client_addr);
    
    printf("Client disconnected: %s:%d\n\n", 
           inet_ntoa(info->client_addr.sin_addr), 
           ntohs(info->client_addr.sin_port));
    
    // Free memory
    free(info);
    
    return NULL;
}

int main(int argc, char *argv[]) {
    // Get port from argument or use default
    int port = DEFAULT_PORT;
    if (argc > 1) {
        port = atoi(argv[1]);
    }
    
    // Read account list
    printf("=== FTP SERVER ===\n");
    printf("Loading account file...\n");
    
    if (load_accounts(ACCOUNT_FILE) < 0) {
        printf("Cannot load account file, creating new file...\n");
        // Add default account
        add_account("user1", "123456", "./data/user1");
        save_accounts(ACCOUNT_FILE);
    }
    
    // Create socket
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("Cannot create socket");
        return 1;
    }
    
    // Allow address reuse
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Setup server address
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    // Bind socket
    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_sock);
        return 1;
    }
    
    // Listen
    if (listen(server_sock, 10) < 0) {
        perror("Listen failed");
        close(server_sock);
        return 1;
    }
    
    printf("FTP Server running on port %d\n", port);
    printf("Waiting for client connections...\n\n");
    
    // Main loop - create new thread for each client
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        // Accept new connection
        int client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock < 0) {
            perror("Accept failed");
            continue;
        }
        
        printf("Client connected: %s:%d\n", 
               inet_ntoa(client_addr.sin_addr), 
               ntohs(client_addr.sin_port));
        
        // Create struct to hold client info
        ClientInfo *info = (ClientInfo *)malloc(sizeof(ClientInfo));
        if (info == NULL) {
            perror("Cannot allocate memory");
            close(client_sock);
            continue;
        }
        info->client_sock = client_sock;
        info->client_addr = client_addr;
        
        // Create new thread to handle client
        pthread_t tid;
        if (pthread_create(&tid, NULL, client_thread, (void *)info) != 0) {
            perror("Cannot create thread");
            close(client_sock);
            free(info);
            continue;
        }
        
        // Detach thread - automatically free when finished
        pthread_detach(tid);
    }
    
    close(server_sock);
    return 0;
}