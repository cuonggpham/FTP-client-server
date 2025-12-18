#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "ftp_client.h"

/*
 * In log theo format: hh:mm:ss <Lenh> <Response>
 */
void log_response(const char *cmd, const char *response) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    
    // Bo ky tu xuong dong trong response de in dep hon
    char clean_resp[256];
    strncpy(clean_resp, response, sizeof(clean_resp) - 1);
    clean_resp[strcspn(clean_resp, "\r\n")] = 0;
    
    printf("%02d:%02d:%02d %s -> %s\n", 
           t->tm_hour, t->tm_min, t->tm_sec,
           cmd, clean_resp);
}

/*
 * Ket noi den FTP server
 */
int ftp_connect(FTPClient *client, const char *host, int port) {
    // Tao socket
    client->ctrl_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client->ctrl_sock < 0) {
        perror("Khong the tao socket");
        return -1;
    }
    
    // Thiet lap dia chi server
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, host, &server_addr.sin_addr) <= 0) {
        perror("Dia chi IP khong hop le");
        close(client->ctrl_sock);
        return -1;
    }
    
    // Ket noi
    if (connect(client->ctrl_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Khong the ket noi den server");
        close(client->ctrl_sock);
        return -1;
    }
    
    strncpy(client->server_ip, host, sizeof(client->server_ip) - 1);
    client->server_port = port;
    client->logged_in = 0;
    
    // Doc welcome message
    char buffer[BUFFER_SIZE];
    ftp_recv_response(client, buffer, sizeof(buffer));
    log_response("CONNECT", buffer);
    
    return 0;
}

/*
 * Ngat ket noi
 */
void ftp_disconnect(FTPClient *client) {
    if (client->ctrl_sock >= 0) {
        close(client->ctrl_sock);
        client->ctrl_sock = -1;
    }
    client->logged_in = 0;
}

/*
 * Gui lenh den server
 */
int ftp_send_cmd(FTPClient *client, const char *cmd) {
    char buffer[CMD_SIZE];
    snprintf(buffer, sizeof(buffer), "%s\r\n", cmd);
    return send(client->ctrl_sock, buffer, strlen(buffer), 0);
}

/*
 * Nhan phan hoi tu server
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
 * Dang nhap
 */
int ftp_login(FTPClient *client, const char *user, const char *pass) {
    char cmd[CMD_SIZE];
    char response[BUFFER_SIZE];
    
    // Gui USER
    snprintf(cmd, sizeof(cmd), "USER %s", user);
    ftp_send_cmd(client, cmd);
    ftp_recv_response(client, response, sizeof(response));
    log_response(cmd, response);
    
    // Gui PASS
    snprintf(cmd, sizeof(cmd), "PASS %s", pass);
    ftp_send_cmd(client, cmd);
    ftp_recv_response(client, response, sizeof(response));
    log_response("PASS ****", response);  // An password trong log
    
    // Kiem tra ket qua
    if (strncmp(response, "230", 3) == 0) {
        client->logged_in = 1;
        return 0;
    }
    return -1;
}

/*
 * Xem thu muc hien tai
 */
int ftp_pwd(FTPClient *client) {
    char response[BUFFER_SIZE];
    
    ftp_send_cmd(client, "PWD");
    ftp_recv_response(client, response, sizeof(response));
    log_response("PWD", response);
    
    return 0;
}

/*
 * Doi thu muc
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
 * Parse response cua PASV de lay IP va port
 * Format: 227 Entering Passive Mode (h1,h2,h3,h4,p1,p2)
 */
int parse_pasv_response(const char *response, char *ip, int *port) {
    int h1, h2, h3, h4, p1, p2;
    
    // Tim dau (
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
 * Tao data connection theo passive mode
 */
int open_data_connection(FTPClient *client) {
    char response[BUFFER_SIZE];
    
    // Gui PASV
    ftp_send_cmd(client, "PASV");
    ftp_recv_response(client, response, sizeof(response));
    log_response("PASV", response);
    
    // Parse response
    char data_ip[50];
    int data_port;
    if (parse_pasv_response(response, data_ip, &data_port) < 0) {
        printf("Loi parse PASV response\n");
        return -1;
    }
    
    // Ket noi den data port
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
 * Liet ke file
 */
int ftp_list(FTPClient *client) {
    char response[BUFFER_SIZE];
    
    // Mo data connection
    int data_sock = open_data_connection(client);
    if (data_sock < 0) {
        printf("Khong the mo data connection\n");
        return -1;
    }
    
    // Gui LIST
    ftp_send_cmd(client, "LIST");
    ftp_recv_response(client, response, sizeof(response));
    log_response("LIST", response);
    
    // Nhan du lieu tu data socket
    printf("\n--- Danh sach file ---\n");
    char buffer[BUFFER_SIZE];
    int bytes;
    while ((bytes = recv(data_sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes] = '\0';
        printf("%s", buffer);
    }
    printf("----------------------\n");
    
    close(data_sock);
    
    // Doc response cuoi
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
    
    // Mo data connection
    int data_sock = open_data_connection(client);
    if (data_sock < 0) {
        printf("Khong the mo data connection\n");
        return -1;
    }
    
    // Set binary mode
    ftp_send_cmd(client, "TYPE I");
    ftp_recv_response(client, response, sizeof(response));
    
    // Gui RETR
    snprintf(cmd, sizeof(cmd), "RETR %s", filename);
    ftp_send_cmd(client, cmd);
    ftp_recv_response(client, response, sizeof(response));
    log_response(cmd, response);
    
    if (strncmp(response, "150", 3) != 0 && strncmp(response, "125", 3) != 0) {
        close(data_sock);
        return -1;
    }
    
    // Mo file local de ghi
    const char *save_path = (local_path && strlen(local_path) > 0) ? local_path : filename;
    FILE *fp = fopen(save_path, "wb");
    if (fp == NULL) {
        perror("Khong the tao file local");
        close(data_sock);
        return -1;
    }
    
    // Nhan va ghi file
    char buffer[BUFFER_SIZE];
    int bytes;
    long total = 0;
    while ((bytes = recv(data_sock, buffer, sizeof(buffer), 0)) > 0) {
        fwrite(buffer, 1, bytes, fp);
        total += bytes;
    }
    
    fclose(fp);
    close(data_sock);
    
    printf("Da download %ld bytes -> %s\n", total, save_path);
    
    // Doc response cuoi
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
    
    // Mo file local
    FILE *fp = fopen(local_path, "rb");
    if (fp == NULL) {
        perror("Khong the mo file local");
        return -1;
    }
    
    // Mo data connection
    int data_sock = open_data_connection(client);
    if (data_sock < 0) {
        fclose(fp);
        printf("Khong the mo data connection\n");
        return -1;
    }
    
    // Set binary mode
    ftp_send_cmd(client, "TYPE I");
    ftp_recv_response(client, response, sizeof(response));
    
    // Gui STOR
    const char *name = (remote_name && strlen(remote_name) > 0) ? remote_name : local_path;
    // Lay ten file (bo duong dan)
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
    
    // Doc va gui file
    char buffer[BUFFER_SIZE];
    size_t bytes;
    long total = 0;
    while ((bytes = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        send(data_sock, buffer, bytes, 0);
        total += bytes;
    }
    
    fclose(fp);
    close(data_sock);
    
    printf("Da upload %ld bytes\n", total);
    
    // Doc response cuoi
    ftp_recv_response(client, response, sizeof(response));
    log_response(cmd, response);
    
    return 0;
}

/*
 * Thoat
 */
int ftp_quit(FTPClient *client) {
    char response[BUFFER_SIZE];
    
    ftp_send_cmd(client, "QUIT");
    ftp_recv_response(client, response, sizeof(response));
    log_response("QUIT", response);
    
    ftp_disconnect(client);
    return 0;
}