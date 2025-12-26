/*
 * Run: ./server [port]
 * Default port 2121
 * Supports multithread - multiple clients connected simultaneously
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "../include/account.h"
#include "../include/ftp_server.h"
#include "../helpers/logger.h"

#define DEFAULT_PORT 2121
#define ACCOUNT_FILE "./server/data/accounts.txt"

// Global session counter (thread-safe)
static int next_session_id = 0;
static pthread_mutex_t session_id_mutex = PTHREAD_MUTEX_INITIALIZER;

// Generate unique session ID
static int get_next_session_id(void) {
    pthread_mutex_lock(&session_id_mutex);
    int id = ++next_session_id;
    pthread_mutex_unlock(&session_id_mutex);
    return id;
}

// Struct to pass data to thread
typedef struct {
    int client_sock;
    struct sockaddr_in client_addr;
    int session_id;
} ClientInfo;

/*
 * Handler function for each client thread
 */
void *client_thread(void *arg) {
    ClientInfo *info = (ClientInfo *)arg;
    
    // Handle client
    handle_client(info->client_sock, info->client_addr, info->session_id);
    
    log_info("[S:%d] Client disconnected: %s:%d", 
           info->session_id,
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
    
    // Initialize logger
    if (init_logger() < 0) {
        fprintf(stderr, "Failed to initialize logger\n");
        return 1;
    }
    
    // Read account list
    log_info("=== FTP SERVER ===");
    log_info("Loading account file...");
    
    if (load_accounts(ACCOUNT_FILE) < 0) {
        log_info("Cannot load account file, creating new file...");
        // Add default account
        add_account("user1", "123456", "./data/user1");
        save_accounts(ACCOUNT_FILE);
    }
    
    // Create socket
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        log_error("Cannot create socket: %s", strerror(errno));
        close_logger();
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
        log_error("Bind failed: %s", strerror(errno));
        close(server_sock);
        close_logger();
        return 1;
    }
    
    // Listen
    if (listen(server_sock, 10) < 0) {
        log_error("Listen failed: %s", strerror(errno));
        close(server_sock);
        close_logger();
        return 1;
    }
    
    log_info("FTP Server running on port %d", port);
    log_info("Waiting for client connections...");
    printf("FTP Server running on port %d - Logs: %s\n", port, "./server/logs/");
    
    // Main loop - create new thread for each client
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        // Accept new connection
        int client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock < 0) {
            log_error("Accept failed: %s", strerror(errno));
            continue;
        }
        
        // Generate unique session ID
        int session_id = get_next_session_id();
        
        log_info("[S:%d] Client connected: %s:%d", 
               session_id,
               inet_ntoa(client_addr.sin_addr), 
               ntohs(client_addr.sin_port));
        
        // Create struct to hold client info
        ClientInfo *info = (ClientInfo *)malloc(sizeof(ClientInfo));
        if (info == NULL) {
            log_error("Cannot allocate memory: %s", strerror(errno));
            close(client_sock);
            continue;
        }
        info->client_sock = client_sock;
        info->client_addr = client_addr;
        info->session_id = session_id;
        
        // Create new thread to handle client
        pthread_t tid;
        if (pthread_create(&tid, NULL, client_thread, (void *)info) != 0) {
            log_error("Cannot create thread: %s", strerror(errno));
            close(client_sock);
            free(info);
            continue;
        }
        
        // Detach thread - automatically free when finished
        pthread_detach(tid);
    }
    
    close(server_sock);
    close_logger();
    return 0;
}