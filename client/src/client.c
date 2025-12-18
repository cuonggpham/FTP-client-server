/*
 * Chay: ./client [host] [port]
 * Mac dinh: 127.0.0.1:2121
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ftp_client.h"

#define DEFAULT_HOST "127.0.0.1"
#define DEFAULT_PORT 2121

void print_help() {
    printf("\n=== Cac lenh FTP ===\n");
    printf("  PWD           - Xem thu muc hien tai\n");
    printf("  CWD <dir>     - Doi thu muc\n");
    printf("  LIST          - Liet ke file trong thu muc\n");
    printf("  RETR <file>   - Download file tu server\n");
    printf("  STOR <file>   - Upload file len server\n");
    printf("  QUIT          - Thoat\n");
    printf("  HELP          - Xem huong dan\n");
    printf("====================\n\n");
}

int main(int argc, char *argv[]) {
    // Lay host va port tu tham so
    const char *host = DEFAULT_HOST;
    int port = DEFAULT_PORT;
    
    if (argc > 1) host = argv[1];
    if (argc > 2) port = atoi(argv[2]);
    
    printf("=== FTP CLIENT ===\n");
    printf("Ket noi den %s:%d...\n", host, port);
    
    // Khoi tao va ket noi
    FTPClient client;
    memset(&client, 0, sizeof(client));
    
    if (ftp_connect(&client, host, port) < 0) {
        printf("Khong the ket noi den server!\n");
        return 1;
    }
    
    printf("Ket noi thanh cong!\n\n");
    
    // Dang nhap
    char username[50], password[50];
    
    printf("Username: ");
    fgets(username, sizeof(username), stdin);
    username[strcspn(username, "\r\n")] = 0;
    
    printf("Password: ");
    fgets(password, sizeof(password), stdin);
    password[strcspn(password, "\r\n")] = 0;
    
    if (ftp_login(&client, username, password) < 0) {
        printf("Dang nhap that bai!\n");
        ftp_disconnect(&client);
        return 1;
    }
    
    printf("Dang nhap thanh cong!\n");
    print_help();
    
    // Vong lap nhap lenh
    char input[256];
    int running = 1;
    
    while (running) {
        printf("ftp> ");
        fflush(stdout);
        
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }
        
        // Bo ky tu xuong dong
        input[strcspn(input, "\r\n")] = 0;
        
        // Bo qua lenh rong
        if (strlen(input) == 0) continue;
        
        // Tach lenh va tham so
        char *cmd = strtok(input, " ");
        char *arg = strtok(NULL, "");
        
        // Xu ly tung lenh
        if (strcasecmp(cmd, "PWD") == 0) {
            ftp_pwd(&client);
        }
        else if (strcasecmp(cmd, "CWD") == 0 || strcasecmp(cmd, "CD") == 0) {
            if (arg == NULL) {
                printf("Cu phap: CWD <thu_muc>\n");
            } else {
                ftp_cwd(&client, arg);
            }
        }
        else if (strcasecmp(cmd, "LIST") == 0 || strcasecmp(cmd, "LS") == 0) {
            ftp_list(&client);
        }
        else if (strcasecmp(cmd, "RETR") == 0 || strcasecmp(cmd, "GET") == 0) {
            if (arg == NULL) {
                printf("Cu phap: RETR <ten_file>\n");
            } else {
                ftp_retr(&client, arg, NULL);
            }
        }
        else if (strcasecmp(cmd, "STOR") == 0 || strcasecmp(cmd, "PUT") == 0) {
            if (arg == NULL) {
                printf("Cu phap: STOR <ten_file_local>\n");
            } else {
                ftp_stor(&client, arg, NULL);
            }
        }
        else if (strcasecmp(cmd, "QUIT") == 0 || strcasecmp(cmd, "EXIT") == 0) {
            ftp_quit(&client);
            running = 0;
        }
        else if (strcasecmp(cmd, "HELP") == 0) {
            print_help();
        }
        else {
            printf("Lenh khong hop le. Go HELP de xem huong dan.\n");
        }
    }
    
    printf("Da thoat!\n");
    return 0;
}