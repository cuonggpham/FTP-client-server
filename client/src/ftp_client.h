#ifndef FTP_CLIENT_H
#define FTP_CLIENT_H

#define BUFFER_SIZE 4096
#define CMD_SIZE 256

// Struct luu thong tin ket noi
typedef struct {
    int ctrl_sock;      // Control socket
    char server_ip[50]; // IP server
    int server_port;    // Port server
    int logged_in;      // Da dang nhap chua
} FTPClient;

// Khoi tao va ket noi den server
int ftp_connect(FTPClient *client, const char *host, int port);

// Ngat ket noi
void ftp_disconnect(FTPClient *client);

// Gui lenh va nhan phan hoi
int ftp_send_cmd(FTPClient *client, const char *cmd);
int ftp_recv_response(FTPClient *client, char *buffer, int size);

// Cac lenh FTP
int ftp_login(FTPClient *client, const char *user, const char *pass);
int ftp_pwd(FTPClient *client);
int ftp_cwd(FTPClient *client, const char *dir);
int ftp_list(FTPClient *client);
int ftp_retr(FTPClient *client, const char *filename, const char *local_path);
int ftp_stor(FTPClient *client, const char *local_path, const char *remote_name);
int ftp_quit(FTPClient *client);

// In log theo format: hh:mm:ss <Lenh> <Response>
void log_response(const char *cmd, const char *response);

#endif
