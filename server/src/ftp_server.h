#ifndef FTP_SERVER_H
#define FTP_SERVER_H

#include <netinet/in.h>

#define BUFFER_SIZE 4096
#define CMD_SIZE 512

// Struct luu trang thai cua 1 phien lam viec
typedef struct {
    int ctrl_sock;              // Control socket
    int logged_in;              // Da dang nhap chua
    int account_index;          // Index cua tai khoan trong mang accounts
    char username[50];          // Username tam thoi (truoc khi nhap pass)
    char current_dir[256];      // Thu muc hien tai
    char root_dir[256];         // Thu muc goc cua user
    struct sockaddr_in client_addr;  // Dia chi client
    int data_listen_sock;       // Socket lang nghe data connection (PASV mode)
    int data_sock;              // Socket data connection hien tai
} FTPSession;

// Ham xu ly client - vong lap chinh
void handle_client(int client_sock, struct sockaddr_in client_addr);

// Cac ham xu ly tung lenh FTP
void cmd_user(FTPSession *session, const char *arg);
void cmd_pass(FTPSession *session, const char *arg);
void cmd_pwd(FTPSession *session);
void cmd_cwd(FTPSession *session, const char *arg);
void cmd_list(FTPSession *session);
void cmd_retr(FTPSession *session, const char *arg);
void cmd_stor(FTPSession *session, const char *arg);
void cmd_pasv(FTPSession *session);
void cmd_type(FTPSession *session, const char *arg);
void cmd_syst(FTPSession *session);
void cmd_quit(FTPSession *session);

// Ham gui response ve client
void send_response(int sock, const char *msg);

// Ham in log lenh nhan duoc
void log_command(const char *cmd, struct sockaddr_in *client_addr);

#endif
