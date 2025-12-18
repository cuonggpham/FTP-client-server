/*
 * Chay: ./server [port]
 * Mac dinh port 2121
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "account.h"
#include "ftp_server.h"

#define DEFAULT_PORT 2121
#define ACCOUNT_FILE "./data/accounts.txt"

int main(int argc, char *argv[]) {
    // Lay port tu tham so hoac dung mac dinh
    int port = DEFAULT_PORT;
    if (argc > 1) {
        port = atoi(argv[1]);
    }
    
    // Doc danh sach tai khoan
    printf("=== FTP SERVER ===\n");
    printf("Dang doc file tai khoan...\n");
    
    if (load_accounts(ACCOUNT_FILE) < 0) {
        printf("Khong the doc file tai khoan, tao file moi...\n");
        // Them tai khoan mac dinh
        add_account("user1", "123456", "./data/user1");
        save_accounts(ACCOUNT_FILE);
    }
    
    // Tao socket
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("Khong the tao socket");
        return 1;
    }
    
    // Cho phep reuse address
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Thiet lap dia chi server
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    // Bind socket
    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind that bai");
        close(server_sock);
        return 1;
    }
    
    // Listen
    if (listen(server_sock, 5) < 0) {
        perror("Listen that bai");
        close(server_sock);
        return 1;
    }
    
    printf("FTP Server dang chay tren port %d\n", port);
    printf("Cho ket noi tu client...\n\n");
    
    // Vong lap chinh - xu ly tung client (khong da luong)
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        // Chap nhan ket noi moi
        int client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock < 0) {
            perror("Accept that bai");
            continue;
        }
        
        printf("Client ket noi: %s:%d\n", 
               inet_ntoa(client_addr.sin_addr), 
               ntohs(client_addr.sin_port));
        
        // Xu ly client (blocking - chi xu ly 1 client tai 1 thoi diem)
        handle_client(client_sock, client_addr);
        
        printf("Client ngat ket noi\n\n");
    }
    
    close(server_sock);
    return 0;
}