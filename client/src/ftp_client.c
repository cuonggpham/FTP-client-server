#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../include/ftp_client.h"

/*
 * Log with format: hh:mm:ss <Command> <Response>
 */
void log_response(const char *cmd, const char *response) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    
    // Remove newline from response for cleaner printing
    char clean_resp[256];
    strncpy(clean_resp, response, sizeof(clean_resp) - 1);
    clean_resp[strcspn(clean_resp, "\r\n")] = 0;
    
    printf("%02d:%02d:%02d %s -> %s\n", 
           t->tm_hour, t->tm_min, t->tm_sec,
           cmd, clean_resp);
}

/*
 * Connect to FTP server
 */
int ftp_connect(FTPClient *client, const char *host, int port) {
    // Create socket
    client->ctrl_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client->ctrl_sock < 0) {
        perror("Cannot create socket");
        return -1;
    }
    
    // Setup server address
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, host, &server_addr.sin_addr) <= 0) {
        perror("Invalid IP address");
        close(client->ctrl_sock);
        return -1;
    }
    
    // Connect
    if (connect(client->ctrl_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Cannot connect to server");
        close(client->ctrl_sock);
        return -1;
    }
    
    strncpy(client->server_ip, host, sizeof(client->server_ip) - 1);
    client->server_port = port;
    client->logged_in = 0;
    
    // Read welcome message
    char buffer[BUFFER_SIZE];
    ftp_recv_response(client, buffer, sizeof(buffer));
    log_response("CONNECT", buffer);
    
    return 0;
}

/*
 * Disconnect
 */
void ftp_disconnect(FTPClient *client) {
    if (client->ctrl_sock >= 0) {
        close(client->ctrl_sock);
        client->ctrl_sock = -1;
    }
    client->logged_in = 0;
}

/*
 * Send command to server
 */
int ftp_send_cmd(FTPClient *client, const char *cmd) {
    char buffer[CMD_SIZE];
    snprintf(buffer, sizeof(buffer), "%s\r\n", cmd);
    return send(client->ctrl_sock, buffer, strlen(buffer), 0);
}

/*
 * Receive response from server
 */
int ftp_recv_response(FTPClient *client, char *buffer, int size) {
    memset(buffer, 0, size);
    int bytes = recv(client->ctrl_sock, buffer, size - 1, 0);
    if (bytes > 0) {
        buffer[bytes] = '\0';
    }
    return bytes;
}

/*
 * Login
 */
int ftp_login(FTPClient *client, const char *user, const char *pass) {
    char cmd[CMD_SIZE];
    char response[BUFFER_SIZE];
    
    // Send USER
    snprintf(cmd, sizeof(cmd), "USER %s", user);
    ftp_send_cmd(client, cmd);
    ftp_recv_response(client, response, sizeof(response));
    log_response(cmd, response);
    
    // Send PASS
    snprintf(cmd, sizeof(cmd), "PASS %s", pass);
    ftp_send_cmd(client, cmd);
    ftp_recv_response(client, response, sizeof(response));
    log_response("PASS ****", response);  // Hide password in log
    
    // Check result
    if (strncmp(response, "230", 3) == 0) {
        client->logged_in = 1;
        return 0;
    }
    return -1;
}

/*
 * Print current directory
 */
int ftp_pwd(FTPClient *client) {
    char response[BUFFER_SIZE];
    
    ftp_send_cmd(client, "PWD");
    ftp_recv_response(client, response, sizeof(response));
    log_response("PWD", response);
    
    return 0;
}

/*
 * Change directory
 */
int ftp_cwd(FTPClient *client, const char *dir) {
    char cmd[CMD_SIZE];
    char response[BUFFER_SIZE];
    
    snprintf(cmd, sizeof(cmd), "CWD %s", dir);
    ftp_send_cmd(client, cmd);
    ftp_recv_response(client, response, sizeof(response));
    log_response(cmd, response);
    
    return (strncmp(response, "250", 3) == 0) ? 0 : -1;
}

/*
 * Parse PASV response to get IP and port
 * Format: 227 Entering Passive Mode (h1,h2,h3,h4,p1,p2)
 */
int parse_pasv_response(const char *response, char *ip, int *port) {
    int h1, h2, h3, h4, p1, p2;
    
    // Find opening parenthesis
    const char *start = strchr(response, '(');
    if (start == NULL) return -1;
    
    if (sscanf(start, "(%d,%d,%d,%d,%d,%d)", &h1, &h2, &h3, &h4, &p1, &p2) != 6) {
        return -1;
    }
    
    sprintf(ip, "%d.%d.%d.%d", h1, h2, h3, h4);
    *port = p1 * 256 + p2;
    
    return 0;
}

/*
 * Create data connection in passive mode
 */
int open_data_connection(FTPClient *client) {
    char response[BUFFER_SIZE];
    
    // Send PASV
    ftp_send_cmd(client, "PASV");
    ftp_recv_response(client, response, sizeof(response));
    log_response("PASV", response);
    
    // Parse response
    char data_ip[50];
    int data_port;
    if (parse_pasv_response(response, data_ip, &data_port) < 0) {
        printf("Error parsing PASV response\n");
        return -1;
    }
    
    // Connect to data port
    int data_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (data_sock < 0) return -1;
    
    struct sockaddr_in data_addr;
    memset(&data_addr, 0, sizeof(data_addr));
    data_addr.sin_family = AF_INET;
    data_addr.sin_port = htons(data_port);
    inet_pton(AF_INET, data_ip, &data_addr.sin_addr);
    
    if (connect(data_sock, (struct sockaddr*)&data_addr, sizeof(data_addr)) < 0) {
        close(data_sock);
        return -1;
    }
    
    return data_sock;
}

/*
 * List files
 */
int ftp_list(FTPClient *client) {
    char response[BUFFER_SIZE];
    
    // Open data connection
    int data_sock = open_data_connection(client);
    if (data_sock < 0) {
        printf("Cannot open data connection\n");
        return -1;
    }
    
    // Send LIST
    ftp_send_cmd(client, "LIST");
    ftp_recv_response(client, response, sizeof(response));
    log_response("LIST", response);
    
    // Receive data from data socket
    printf("\n--- File listing ---\n");
    char buffer[BUFFER_SIZE];
    int bytes;
    while ((bytes = recv(data_sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes] = '\0';
        printf("%s", buffer);
    }
    printf("----------------------\n");
    
    close(data_sock);
    
    // Read final response
    ftp_recv_response(client, response, sizeof(response));
    log_response("LIST", response);
    
    return 0;
}

/*
 * Download file
 */
int ftp_retr(FTPClient *client, const char *filename, const char *local_path) {
    char cmd[CMD_SIZE];
    char response[BUFFER_SIZE];
    
    // Open data connection
    int data_sock = open_data_connection(client);
    if (data_sock < 0) {
        printf("Cannot open data connection\n");
        return -1;
    }
    
    // Set binary mode
    ftp_send_cmd(client, "TYPE I");
    ftp_recv_response(client, response, sizeof(response));
    
    // Send RETR
    snprintf(cmd, sizeof(cmd), "RETR %s", filename);
    ftp_send_cmd(client, cmd);
    ftp_recv_response(client, response, sizeof(response));
    log_response(cmd, response);
    
    if (strncmp(response, "150", 3) != 0 && strncmp(response, "125", 3) != 0) {
        close(data_sock);
        return -1;
    }
    
    // Open local file for writing
    const char *save_path = (local_path && strlen(local_path) > 0) ? local_path : filename;
    FILE *fp = fopen(save_path, "wb");
    if (fp == NULL) {
        perror("Cannot create local file");
        close(data_sock);
        return -1;
    }
    
    // Receive and write file
    char buffer[BUFFER_SIZE];
    int bytes;
    long total = 0;
    while ((bytes = recv(data_sock, buffer, sizeof(buffer), 0)) > 0) {
        fwrite(buffer, 1, bytes, fp);
        total += bytes;
    }
    
    fclose(fp);
    close(data_sock);
    
    printf("Downloaded %ld bytes -> %s\n", total, save_path);
    
    // Read final response
    ftp_recv_response(client, response, sizeof(response));
    log_response(cmd, response);
    
    return 0;
}

/*
 * Upload file
 */
int ftp_stor(FTPClient *client, const char *local_path, const char *remote_name) {
    char cmd[CMD_SIZE];
    char response[BUFFER_SIZE];
    
    // Open local file
    FILE *fp = fopen(local_path, "rb");
    if (fp == NULL) {
        perror("Cannot open local file");
        return -1;
    }
    
    // Open data connection
    int data_sock = open_data_connection(client);
    if (data_sock < 0) {
        fclose(fp);
        printf("Cannot open data connection\n");
        return -1;
    }
    
    // Set binary mode
    ftp_send_cmd(client, "TYPE I");
    ftp_recv_response(client, response, sizeof(response));
    
    // Send STOR
    const char *name = (remote_name && strlen(remote_name) > 0) ? remote_name : local_path;
    // Get filename (remove path)
    const char *base = strrchr(name, '/');
    if (base) name = base + 1;
    
    snprintf(cmd, sizeof(cmd), "STOR %s", name);
    ftp_send_cmd(client, cmd);
    ftp_recv_response(client, response, sizeof(response));
    log_response(cmd, response);
    
    if (strncmp(response, "150", 3) != 0 && strncmp(response, "125", 3) != 0) {
        fclose(fp);
        close(data_sock);
        return -1;
    }
    
    // Read and send file
    char buffer[BUFFER_SIZE];
    size_t bytes;
    long total = 0;
    while ((bytes = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        send(data_sock, buffer, bytes, 0);
        total += bytes;
    }
    
    fclose(fp);
    close(data_sock);
    
    printf("Uploaded %ld bytes\n", total);
    
    // Read final response
    ftp_recv_response(client, response, sizeof(response));
    log_response(cmd, response);
    
    return 0;
}

/*
 * Quit
 */
int ftp_quit(FTPClient *client) {
    char response[BUFFER_SIZE];
    
    ftp_send_cmd(client, "QUIT");
    ftp_recv_response(client, response, sizeof(response));
    log_response("QUIT", response);
    
    ftp_disconnect(client);
    return 0;
}