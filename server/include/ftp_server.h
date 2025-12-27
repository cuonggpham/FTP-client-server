#ifndef FTP_SERVER_H
#define FTP_SERVER_H

#include <netinet/in.h>

#define BUFFER_SIZE 4096
#define CMD_SIZE 512

/* cau truc trang thai phien FTP */
typedef struct {
    int session_id;             /* dinh danh phien duy nhat */
    int ctrl_sock;              /* socket dieu khien */
    int logged_in;              /* co trang thai dang nhap */
    int account_index;          /* chi so tai khoan trong mang accounts */
    char username[50];          /* ten nguoi dung tam thoi (truoc mat khau) */
    char current_dir[256];      /* thu muc lam viec hien tai */
    char root_dir[256];         /* thu muc goc cua nguoi dung */
    struct sockaddr_in client_addr;  /* dia chi client */
    int data_listen_sock;       /* socket lang nghe du lieu (che do PASV) */
    int data_sock;              /* socket ket noi du lieu hien tai */
} FTPSession;

/* ham xu ly chinh cho client - vong lap chinh */
void handle_client(int client_sock, struct sockaddr_in client_addr, int session_id);

/* cac ham xu ly lenh FTP */
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

/* gui phan hoi den client */
void send_response(int sock, const char *msg);

#endif
