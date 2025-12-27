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
 * ghi log voi dinh dang: hh:mm:ss <Lenh> <Phan hoi>
 */
void log_response(const char *cmd, const char *response) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    
    // xoa ky tu xuong dong de in sach hon
    char clean_resp[256];
    strncpy(clean_resp, response, sizeof(clean_resp) - 1);
    clean_resp[strcspn(clean_resp, "\r\n")] = 0;
    
    printf("%02d:%02d:%02d %s -> %s\n", 
           t->tm_hour, t->tm_min, t->tm_sec,
           cmd, clean_resp);
}

/*
 * ket noi den may chu FTP
 */
int ftp_connect(FTPClient *client, const char *host, int port) {
    // tao socket
    client->ctrl_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client->ctrl_sock < 0) {
        perror("Cannot create socket");
        return -1;
    }
    
    // thiet lap dia chi may chu
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, host, &server_addr.sin_addr) <= 0) {
        perror("Invalid IP address");
        close(client->ctrl_sock);
        return -1;
    }
    
    // ket noi
    if (connect(client->ctrl_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Cannot connect to server");
        close(client->ctrl_sock);
        return -1;
    }
    
    strncpy(client->server_ip, host, sizeof(client->server_ip) - 1);
    client->server_port = port;
    client->logged_in = 0;
    
    // doc thong diep chao mung
    char buffer[BUFFER_SIZE];
    ftp_recv_response(client, buffer, sizeof(buffer));
    log_response("CONNECT", buffer);
    
    return 0;
}

/*
 * ngat ket noi
 */
void ftp_disconnect(FTPClient *client) {
    if (client->ctrl_sock >= 0) {
        close(client->ctrl_sock);
        client->ctrl_sock = -1;
    }
    client->logged_in = 0;
}

/*
 * gui lenh den may chu
 */
int ftp_send_cmd(FTPClient *client, const char *cmd) {
    char buffer[CMD_SIZE];
    snprintf(buffer, sizeof(buffer), "%s\r\n", cmd);
    return send(client->ctrl_sock, buffer, strlen(buffer), 0);
}

/*
 * nhan phan hoi tu may chu
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
 * dang nhap
 */
int ftp_login(FTPClient *client, const char *user, const char *pass) {
    char cmd[CMD_SIZE];
    char response[BUFFER_SIZE];
    
    // gui lenh USER
    snprintf(cmd, sizeof(cmd), "USER %s", user);
    ftp_send_cmd(client, cmd);
    ftp_recv_response(client, response, sizeof(response));
    log_response(cmd, response);
    
    // gui lenh PASS
    snprintf(cmd, sizeof(cmd), "PASS %s", pass);
    ftp_send_cmd(client, cmd);
    ftp_recv_response(client, response, sizeof(response));
    log_response("PASS ****", response);  // an mat khau trong log
    
    // kiem tra ket qua
    if (strncmp(response, "230", 3) == 0) {
        client->logged_in = 1;
        return 0;
    }
    return -1;
}

/*
 * in thu muc hien tai
 */
int ftp_pwd(FTPClient *client) {
    char response[BUFFER_SIZE];
    
    ftp_send_cmd(client, "PWD");
    ftp_recv_response(client, response, sizeof(response));
    log_response("PWD", response);
    
    return 0;
}

/*
 * thay doi thu muc
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
 * phan tich phan hoi PASV de lay IP va port
 * dinh dang: 227 Entering Passive Mode (h1,h2,h3,h4,p1,p2)
 */
int parse_pasv_response(const char *response, char *ip, int *port) {
    int h1, h2, h3, h4, p1, p2;
    
    // tim dau ngoac mo
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
 * tao ket noi du lieu o che do thu dong
 */
int open_data_connection(FTPClient *client) {
    char response[BUFFER_SIZE];
    
    // gui lenh PASV
    ftp_send_cmd(client, "PASV");
    ftp_recv_response(client, response, sizeof(response));
    log_response("PASV", response);
    
    // phan tich phan hoi
    char data_ip[50];
    int data_port;
    if (parse_pasv_response(response, data_ip, &data_port) < 0) {
        printf("Error parsing PASV response\n");
        return -1;
    }
    
    // ket noi den cong du lieu
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
 * liet ke tap tin
 */
int ftp_list(FTPClient *client) {
    char response[BUFFER_SIZE];
    
    // mo ket noi du lieu
    int data_sock = open_data_connection(client);
    if (data_sock < 0) {
        printf("Cannot open data connection\n");
        return -1;
    }
    
    // gui lenh LIST
    ftp_send_cmd(client, "LIST");
    ftp_recv_response(client, response, sizeof(response));
    log_response("LIST", response);
    
    // nhan du lieu tu socket du lieu
    printf("\n--- File listing ---\n");
    char buffer[BUFFER_SIZE];
    int bytes;
    while ((bytes = recv(data_sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes] = '\0';
        printf("%s", buffer);
    }
    printf("----------------------\n");
    
    close(data_sock);
    
    // doc phan hoi cuoi cung
    ftp_recv_response(client, response, sizeof(response));
    log_response("LIST", response);
    
    return 0;
}

/*
 * tai tap tin xuong
 */
int ftp_retr(FTPClient *client, const char *filename, const char *local_path) {
    char cmd[CMD_SIZE];
    char response[BUFFER_SIZE];
    
    // mo ket noi du lieu
    int data_sock = open_data_connection(client);
    if (data_sock < 0) {
        printf("Cannot open data connection\n");
        return -1;
    }
    
    // dat che do nhi phan
    ftp_send_cmd(client, "TYPE I");
    ftp_recv_response(client, response, sizeof(response));
    
    // gui lenh RETR
    snprintf(cmd, sizeof(cmd), "RETR %s", filename);
    ftp_send_cmd(client, cmd);
    ftp_recv_response(client, response, sizeof(response));
    log_response(cmd, response);
    
    if (strncmp(response, "150", 3) != 0 && strncmp(response, "125", 3) != 0) {
        close(data_sock);
        return -1;
    }
    
    // mo tap tin cuc bo de ghi
    const char *save_path = (local_path && strlen(local_path) > 0) ? local_path : filename;
    FILE *fp = fopen(save_path, "wb");
    if (fp == NULL) {
        perror("Cannot create local file");
        close(data_sock);
        return -1;
    }
    
    // nhan va ghi tap tin
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
    
    // doc phan hoi cuoi cung
    ftp_recv_response(client, response, sizeof(response));
    log_response(cmd, response);
    
    return 0;
}

/*
 * tai tap tin len
 */
int ftp_stor(FTPClient *client, const char *local_path, const char *remote_name) {
    char cmd[CMD_SIZE];
    char response[BUFFER_SIZE];
    
    // mo tap tin cuc bo
    FILE *fp = fopen(local_path, "rb");
    if (fp == NULL) {
        perror("Cannot open local file");
        return -1;
    }
    
    // mo ket noi du lieu
    int data_sock = open_data_connection(client);
    if (data_sock < 0) {
        fclose(fp);
        printf("Cannot open data connection\n");
        return -1;
    }
    
    // dat che do nhi phan
    ftp_send_cmd(client, "TYPE I");
    ftp_recv_response(client, response, sizeof(response));
    
    // gui lenh STOR
    const char *name = (remote_name && strlen(remote_name) > 0) ? remote_name : local_path;
    // lay ten tap tin (bo duong dan)
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
    
    // doc va gui tap tin
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
    
    // doc phan hoi cuoi cung
    ftp_recv_response(client, response, sizeof(response));
    log_response(cmd, response);
    
    return 0;
}

/*
 * thoat
 */
int ftp_quit(FTPClient *client) {
    char response[BUFFER_SIZE];
    
    ftp_send_cmd(client, "QUIT");
    ftp_recv_response(client, response, sizeof(response));
    log_response("QUIT", response);
    
    ftp_disconnect(client);
    return 0;
}