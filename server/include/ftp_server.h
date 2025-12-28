#ifndef FTP_SERVER_H
#define FTP_SERVER_H

#include <netinet/in.h>

#define BUFFER_SIZE 4096
#define CMD_SIZE 512

// cau truc trang thai phien FTP
typedef struct {
    int session_id;
    int ctrl_sock;              // socket dieu khien
    int logged_in;              // 0/1 - trang thai dang nhap
    int account_index;          // index trong mang accounts[]
    char username[50];          // username tam thoi (chua dang nhap)
    char current_dir[256];
    char root_dir[256];
    struct sockaddr_in client_addr;  // dia chi client
    int data_listen_sock;       // socket listen (PASV)
    int data_sock;              // socket connect du lieu
} FTPSession;

// ham xu ly chinh cho client - vong lap chinh
void handle_client(int client_sock, struct sockaddr_in client_addr, int session_id);

// cac ham xu ly lenh FTP
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

// gui phan hoi den client
void send_response(int sock, const char *msg);

#endif
