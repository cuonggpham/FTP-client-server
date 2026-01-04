#ifndef FTP_CLIENT_H
#define FTP_CLIENT_H

#define BUFFER_SIZE 4096
#define CMD_SIZE 256

typedef struct {
    int ctrl_sock;      // socket dieu khien
    char server_ip[50]; // dia chi IP server
    int server_port;    // port server
    int logged_in;      // co trang thai dang nhap
} FTPClient;

// khoi tao va ket noi den may chu
int ftp_connect(FTPClient *client, const char *host, int port);

// ngat ket noi khoi may chu
void ftp_disconnect(FTPClient *client);

// gui lenh va nhan phan hoi
int ftp_send_cmd(FTPClient *client, const char *cmd);
int ftp_recv_response(FTPClient *client, char *buffer, int size);

// cac lenh FTP
int ftp_login(FTPClient *client, const char *user, const char *pass);
int ftp_pwd(FTPClient *client);
int ftp_cwd(FTPClient *client, const char *dir);
int ftp_list(FTPClient *client);
int ftp_retr(FTPClient *client, const char *filename, const char *local_path);
int ftp_stor(FTPClient *client, const char *local_path, const char *remote_name);
int ftp_quit(FTPClient *client);

// ghi log voi dinh dang: hh:mm:ss <Lenh> <Phan hoi>
void log_response(const char *cmd, const char *response);

#endif
