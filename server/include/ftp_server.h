#ifndef FTP_SERVER_H
#define FTP_SERVER_H

#include <netinet/in.h>

#define BUFFER_SIZE 4096
#define CMD_SIZE 512

/* FTP session state structure */
typedef struct {
    int ctrl_sock;              /* Control socket */
    int logged_in;              /* Login status flag */
    int account_index;          /* Index of account in accounts array */
    char username[50];          /* Temporary username (before password) */
    char current_dir[256];      /* Current working directory */
    char root_dir[256];         /* User's root directory */
    struct sockaddr_in client_addr;  /* Client address */
    int data_listen_sock;       /* Data listening socket (PASV mode) */
    int data_sock;              /* Current data connection socket */
} FTPSession;

/* Main client handler - main loop */
void handle_client(int client_sock, struct sockaddr_in client_addr);

/* FTP command handlers */
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

/* Send response to client */
void send_response(int sock, const char *msg);

/* Log received command */
void log_command(const char *cmd, struct sockaddr_in *client_addr);

#endif
