#ifndef FTP_CLIENT_H
#define FTP_CLIENT_H

#define BUFFER_SIZE 4096
#define CMD_SIZE 256

/* FTP client connection structure */
typedef struct {
    int ctrl_sock;      /* Control socket */
    char server_ip[50]; /* Server IP address */
    int server_port;    /* Server port */
    int logged_in;      /* Login status flag */
} FTPClient;

/* Initialize and connect to server */
int ftp_connect(FTPClient *client, const char *host, int port);

/* Disconnect from server */
void ftp_disconnect(FTPClient *client);

/* Send command and receive response */
int ftp_send_cmd(FTPClient *client, const char *cmd);
int ftp_recv_response(FTPClient *client, char *buffer, int size);

/* FTP commands */
int ftp_login(FTPClient *client, const char *user, const char *pass);
int ftp_pwd(FTPClient *client);
int ftp_cwd(FTPClient *client, const char *dir);
int ftp_list(FTPClient *client);
int ftp_retr(FTPClient *client, const char *filename, const char *local_path);
int ftp_stor(FTPClient *client, const char *local_path, const char *remote_name);
int ftp_quit(FTPClient *client);

/* Log with format: hh:mm:ss <Command> <Response> */
void log_response(const char *cmd, const char *response);

#endif
