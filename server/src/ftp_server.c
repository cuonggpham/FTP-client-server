#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../include/ftp_server.h"
#include "../include/account.h"
#include "../helpers/logger.h"

/*
 * gui phan hoi den client
 */
void send_response(int sock, const char *msg) {
    send(sock, msg, strlen(msg), 0);
}

/*
 * Xu ly lenh USER - nhan username
 */
void cmd_user(FTPSession *session, const char *arg) {
    if (arg == NULL || strlen(arg) == 0) {
        send_response(session->ctrl_sock, "501 Syntax error\r\n");
        return;
    }

    strncpy(session->username, arg, sizeof(session->username) - 1);
    session->logged_in = 0;
    send_response(session->ctrl_sock, "331 Username OK, need password\r\n");
}

/*
 * Xu ly lenh PASS - kiem tra password
 */
void cmd_pass(FTPSession *session, const char *arg) {
    if (strlen(session->username) == 0) {
        send_response(session->ctrl_sock, "503 Login with USER first\r\n");
        return;
    }

    int idx = check_login(session->username, arg);
    if (idx >= 0) {
        session->logged_in = 1;
        session->account_index = idx; // luu vi tri tai khoan

        // Thiet lap thu muc goc va thu muc hien tai
        strncpy(session->root_dir, accounts[idx].home_dir, sizeof(session->root_dir) - 1);
        strncpy(session->current_dir, "/", sizeof(session->current_dir) - 1);

        send_response(session->ctrl_sock, "230 User logged in\r\n");
    } else {
        send_response(session->ctrl_sock, "530 Login incorrect\r\n");
    }
}

/*
 * xu ly lenh PWD - tra ve thu muc hien tai
 */
void cmd_pwd(FTPSession *session) {
    if (!session->logged_in) {
        send_response(session->ctrl_sock, "530 Not logged in\r\n");
        return;
    }

    char response[256];
    snprintf(response, sizeof(response), "257 \"%s\" is current directory\r\n",
             session->current_dir);
    send_response(session->ctrl_sock, response);
}

/*
 * xu ly lenh CWD - thay doi thu muc
 */
void cmd_cwd(FTPSession *session, const char *arg) {
    if (!session->logged_in) {
        send_response(session->ctrl_sock, "530 Not logged in\r\n");
        return;
    }

    if (arg == NULL || strlen(arg) == 0) {
        send_response(session->ctrl_sock, "501 Syntax error\r\n");
        return;
    }

    // khong cho phep di len parent cua root
    if (strcmp(arg, "..") == 0 && strcmp(session->current_dir, "/") == 0) {
        send_response(session->ctrl_sock, "550 Permission denied\r\n");
        return;
    }

    char new_path[MAX_PATH_LEN]; // client
    char full_path[MAX_PATH_LEN]; // server

    // xu ly duong dan tuyet doi hoac tuong doi
    if (arg[0] == '/') {
        // duong dan tuyet doi
        strncpy(new_path, arg, sizeof(new_path) - 1);
    } else if (strcmp(arg, "..") == 0) {
        // di len thu muc cha
        strncpy(new_path, session->current_dir, sizeof(new_path) - 1);
        char *last_slash = strrchr(new_path, '/');
        if (last_slash != NULL && last_slash != new_path) {
            *last_slash = '\0';
        } else {
            strcpy(new_path, "/");
        }
    } else {
        // duong dan tuong doi
        if (strcmp(session->current_dir, "/") == 0) {
            snprintf(new_path, sizeof(new_path), "/%s", arg);
        } else {
            snprintf(new_path, sizeof(new_path), "%s/%s", session->current_dir, arg);
        }
    }

    // kiem tra khong thoat khoi thu muc goc
    // tao duong dan thuc te tren server
    snprintf(full_path, sizeof(full_path), "%s%s", session->root_dir, new_path);

    // kiem tra thu muc ton tai
    struct stat st;
    if (stat(full_path, &st) == 0 && S_ISDIR(st.st_mode)) {
        strncpy(session->current_dir, new_path, sizeof(session->current_dir) - 1);
        send_response(session->ctrl_sock, "250 Directory changed\r\n");
    } else {
        send_response(session->ctrl_sock, "550 Directory not found\r\n");
    }
}

/*
 * xu ly lenh PASV - thiet lap che do thu dong
 */
void cmd_pasv(FTPSession *session) {
    if (!session->logged_in) {
        send_response(session->ctrl_sock, "530 Not logged in\r\n");
        return;
    }

    // dong socket hien tai neu co
    if (session->data_listen_sock >= 0) {
        close(session->data_listen_sock);
    }

    // tao socket moi cho ket noi du lieu
    session->data_listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (session->data_listen_sock < 0) {
        send_response(session->ctrl_sock, "425 Can't open data connection\r\n");
        return;
    }

    // cho phep tai su dung dia chi
    int opt = 1;
    setsockopt(session->data_listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // bind vao cong ngau nhien
    struct sockaddr_in data_addr;
    memset(&data_addr, 0, sizeof(data_addr));
    data_addr.sin_family = AF_INET;
    data_addr.sin_addr.s_addr = INADDR_ANY;
    data_addr.sin_port = 0;  // Kernel tu chon port

    if (bind(session->data_listen_sock, (struct sockaddr*)&data_addr, sizeof(data_addr)) < 0) {
        close(session->data_listen_sock);
        session->data_listen_sock = -1;
        send_response(session->ctrl_sock, "425 Can't open data connection\r\n");
        return;
    }

    listen(session->data_listen_sock, 1);

    // lay port duoc gan
    socklen_t len = sizeof(data_addr);
    getsockname(session->data_listen_sock, (struct sockaddr*)&data_addr, &len);
    int pasv_port = ntohs(data_addr.sin_port);

    // lay IP may chu (su dung IP cua socket dieu khien)
    struct sockaddr_in server_addr;
    len = sizeof(server_addr);
    getsockname(session->ctrl_sock, (struct sockaddr*)&server_addr, &len);
    unsigned char *ip = (unsigned char*)&server_addr.sin_addr.s_addr;

    // tra loi client: 227 Entering Passive Mode (h1,h2,h3,h4,p1,p2)
    char response[100];
    snprintf(response, sizeof(response),
             "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)\r\n",
             ip[0], ip[1], ip[2], ip[3],
             (pasv_port >> 8) & 0xFF,
             pasv_port & 0xFF);

    send_response(session->ctrl_sock, response);
}

/*
 * chap nhan ket noi du lieu tu client sau PASV
 */
int accept_data_connection(FTPSession *session) {
    if (session->data_listen_sock < 0) return -1;

    struct sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);

    session->data_sock = accept(session->data_listen_sock, (struct sockaddr*)&client_addr, &len);

    close(session->data_listen_sock);
    session->data_listen_sock = -1;

    return session->data_sock;
}

/*
 * xu ly lenh LIST - liet ke tap tin trong thu muc hien tai
 */
void cmd_list(FTPSession *session) {
    if (!session->logged_in) {
        send_response(session->ctrl_sock, "530 Not logged in\r\n");
        return;
    }

    // chap nhan ket noi du lieu
    int dsock = accept_data_connection(session);
    if (dsock < 0) {
        send_response(session->ctrl_sock, "425 Can't open data connection\r\n");
        return;
    }

    send_response(session->ctrl_sock, "150 Opening data connection\r\n");

    // tao duong dan thuc te
    char full_path[MAX_PATH_LEN];
    snprintf(full_path, sizeof(full_path), "%s%s", session->root_dir, session->current_dir);

    // mo thu muc va doc noi dung
    DIR *dir = opendir(full_path);
    if (dir == NULL) {
        close(dsock);
        session->data_sock = -1;
        send_response(session->ctrl_sock, "550 Failed to open directory\r\n");
        return;
    }

    struct dirent *entry;
    char line[512];
    struct stat st;
    char filepath[MAX_PATH_LEN];

    while ((entry = readdir(dir)) != NULL) {
        // bo qua . va ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        snprintf(filepath, sizeof(filepath), "%s/%s", full_path, entry->d_name);

        if (stat(filepath, &st) == 0) {
            // dinh dang giong ls -l
            char perms[11] = "----------";
            perms[0] = S_ISDIR(st.st_mode) ? 'd' : '-';
            perms[1] = (st.st_mode & S_IRUSR) ? 'r' : '-';
            perms[2] = (st.st_mode & S_IWUSR) ? 'w' : '-';
            perms[3] = (st.st_mode & S_IXUSR) ? 'x' : '-';
            perms[4] = (st.st_mode & S_IRGRP) ? 'r' : '-';
            perms[5] = (st.st_mode & S_IWGRP) ? 'w' : '-';
            perms[6] = (st.st_mode & S_IXGRP) ? 'x' : '-';
            perms[7] = (st.st_mode & S_IROTH) ? 'r' : '-';
            perms[8] = (st.st_mode & S_IWOTH) ? 'w' : '-';
            perms[9] = (st.st_mode & S_IXOTH) ? 'x' : '-';

            struct tm *t = localtime(&st.st_mtime);

            snprintf(line, sizeof(line), "%s 1 ftp ftp %8ld %3s %2d %02d:%02d %s\r\n",
                     perms,
                     (long)st.st_size,
                     "Jan\0Feb\0Mar\0Apr\0May\0Jun\0Jul\0Aug\0Sep\0Oct\0Nov\0Dec" + t->tm_mon * 4,
                     t->tm_mday,
                     t->tm_hour,
                     t->tm_min,
                     entry->d_name);

            send(dsock, line, strlen(line), 0);
        }
    }

    closedir(dir);
    close(dsock);
    session->data_sock = -1;

    send_response(session->ctrl_sock, "226 Transfer complete\r\n");
}

/*
 * xu ly lenh RETR - tai tap tin tu may chu
 */
void cmd_retr(FTPSession *session, const char *filename) {
    if (!session->logged_in) {
        send_response(session->ctrl_sock, "530 Not logged in\r\n");
        return;
    }

    if (filename == NULL || strlen(filename) == 0) {
        send_response(session->ctrl_sock, "501 Syntax error\r\n");
        return;
    }

    // tao duong dan day du
    char filepath[MAX_PATH_LEN];
    if (filename[0] == '/') {
        snprintf(filepath, sizeof(filepath), "%s%s", session->root_dir, filename);
    } else {
        snprintf(filepath, sizeof(filepath), "%s%s/%s",
                 session->root_dir, session->current_dir, filename);
    }

    // mo tap tin
    FILE *fp = fopen(filepath, "rb");
    if (fp == NULL) {
        send_response(session->ctrl_sock, "550 File not found\r\n");
        return;
    }

    // chap nhan ket noi du lieu
    int dsock = accept_data_connection(session);
    if (dsock < 0) {
        fclose(fp);
        send_response(session->ctrl_sock, "425 Can't open data connection\r\n");
        return;
    }

    send_response(session->ctrl_sock, "150 Opening data connection\r\n");

    // gui tap tin
    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    int transfer_aborted = 0;
    int io_error = 0;

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        ssize_t sent = send(dsock, buffer, bytes_read, 0);
        if (sent <= 0) {
            // client dong ket noi hoac loi mang
            transfer_aborted = 1;
            break;
        }
    }

    // kiem tra fread co gap loi khong (khong phai EOF)
    if (ferror(fp)) {
        io_error = 1;
    }

    fclose(fp);
    close(dsock);
    session->data_sock = -1;

    // gui phan hoi phu hop dua tren ket qua truyen
    if (transfer_aborted) {
        send_response(session->ctrl_sock, "426 Connection closed; transfer aborted\r\n");
    } else if (io_error) {
        send_response(session->ctrl_sock, "450 Requested file action not taken\r\n");
    } else {
        send_response(session->ctrl_sock, "226 Transfer complete\r\n");
    }
}

/*
 * xu ly lenh STOR - tai tap tin len may chu
 */
void cmd_stor(FTPSession *session, const char *filename) {
    if (!session->logged_in) {
        send_response(session->ctrl_sock, "530 Not logged in\r\n");
        return;
    }

    if (filename == NULL || strlen(filename) == 0) {
        send_response(session->ctrl_sock, "501 Syntax error\r\n");
        return;
    }

    // tao duong dan day du
    char filepath[MAX_PATH_LEN];
    if (filename[0] == '/') {
        snprintf(filepath, sizeof(filepath), "%s%s", session->root_dir, filename);
    } else {
        snprintf(filepath, sizeof(filepath), "%s%s/%s",
                 session->root_dir, session->current_dir, filename);
    }

    // chap nhan ket noi du lieu
    int dsock = accept_data_connection(session);
    if (dsock < 0) {
        send_response(session->ctrl_sock, "425 Can't open data connection\r\n");
        return;
    }

    // mo tap tin de ghi
    FILE *fp = fopen(filepath, "wb");
    if (fp == NULL) {
        close(dsock);
        session->data_sock = -1;

        // phan biet giua tu choi quyen va khong tim thay tap tin
        if (errno == EACCES || errno == EPERM) {
            send_response(session->ctrl_sock, "450 Requested file action not taken\r\n");
        } else {
            send_response(session->ctrl_sock, "550 Cannot create file\r\n");
        }
        return;
    }

    send_response(session->ctrl_sock, "150 Opening data connection\r\n");

    // nhan tap tin
    char buffer[BUFFER_SIZE];
    ssize_t bytes_recv;
    int transfer_aborted = 0;
    int io_error = 0;

    while ((bytes_recv = recv(dsock, buffer, sizeof(buffer), 0)) > 0) {
        size_t written = fwrite(buffer, 1, bytes_recv, fp);
        if (written < (size_t)bytes_recv) {
            // dia day hoac loi I/O
            io_error = 1;
            break;
        }
    }

    // kiem tra recv co gap loi khong (khong phai EOF)
    if (bytes_recv < 0) {
        transfer_aborted = 1;
    }

    fclose(fp);
    close(dsock);
    session->data_sock = -1;

    // gui phan hoi phu hop dua tren ket qua truyen
    if (transfer_aborted) {
        send_response(session->ctrl_sock, "426 Connection closed; transfer aborted\r\n");
        // xoa tap tin khong hoan chinh
        unlink(filepath);
    } else if (io_error) {
        send_response(session->ctrl_sock, "450 Requested file action not taken\r\n");
        // Delete incomplete file
        unlink(filepath);
    } else {
        send_response(session->ctrl_sock, "226 Transfer complete\r\n");
    }
}

/*
 * xu ly lenh TYPE - dat che do truyen
 */
void cmd_type(FTPSession *session, const char *arg) {
    if (!session->logged_in) {
        send_response(session->ctrl_sock, "530 Not logged in\r\n");
        return;
    }

    // chap nhan ca che do ASCII va Binary
    if (arg && (arg[0] == 'A' || arg[0] == 'a')) {
        send_response(session->ctrl_sock, "200 Type set to A\r\n");
    } else if (arg && (arg[0] == 'I' || arg[0] == 'i')) {
        send_response(session->ctrl_sock, "200 Type set to I\r\n");
    } else {
        send_response(session->ctrl_sock, "504 Type not supported\r\n");
    }
}

/*
 * xu ly lenh SYST - tra ve thong tin he thong
 */
void cmd_syst(FTPSession *session) {
    send_response(session->ctrl_sock, "215 UNIX Type: L8\r\n");
}

/*
 * xu ly lenh QUIT - ngat ket noi
 */
void cmd_quit(FTPSession *session) {
    send_response(session->ctrl_sock, "221 Goodbye\r\n");
}

// ham xu ly chinh cho moi client
void handle_client(int client_sock, struct sockaddr_in client_addr, int session_id) {
    FTPSession session; // khoi tao session
    memset(&session, 0, sizeof(session));
    session.session_id = session_id;
    session.ctrl_sock = client_sock;
    session.client_addr = client_addr;
    session.logged_in = 0;
    session.data_listen_sock = -1;  // chua co ket noi du lieu
    session.data_sock = -1;

    send_response(client_sock, "220 FTP Server Ready\r\n");

    char buffer[CMD_SIZE];
    int running = 1;
    while (running) {
        memset(buffer, 0, sizeof(buffer));

        // blocking: doi lenh tu client
        int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) {
            break;  // client dong ket noi
        }

        buffer[strcspn(buffer, "\r\n")] = 0; // loai bo \r\n

        log_command(session.session_id, buffer, inet_ntoa(client_addr.sin_addr));

        // tach lenh va tham so
        char *cmd = strtok(buffer, " ");
        char *arg = strtok(NULL, "");
        if (cmd == NULL) continue;

        // xu ly tung lenh
        if (strcasecmp(cmd, "USER") == 0) {
            cmd_user(&session, arg);
        }
        else if (strcasecmp(cmd, "PASS") == 0) {
            cmd_pass(&session, arg);
        }
        else if (strcasecmp(cmd, "PWD") == 0 || strcasecmp(cmd, "XPWD") == 0) {
            cmd_pwd(&session);
        }
        else if (strcasecmp(cmd, "CWD") == 0) {
            cmd_cwd(&session, arg);
        }
        else if (strcasecmp(cmd, "CDUP") == 0) {
            cmd_cwd(&session, "..");
        }
        else if (strcasecmp(cmd, "PASV") == 0) {
            cmd_pasv(&session);
        }
        else if (strcasecmp(cmd, "LIST") == 0 || strcasecmp(cmd, "NLST") == 0) {
            cmd_list(&session);
        }
        else if (strcasecmp(cmd, "RETR") == 0) {
            cmd_retr(&session, arg);
        }
        else if (strcasecmp(cmd, "STOR") == 0) {
            cmd_stor(&session, arg);
        }
        else if (strcasecmp(cmd, "TYPE") == 0) {
            cmd_type(&session, arg);
        }
        else if (strcasecmp(cmd, "SYST") == 0) {
            cmd_syst(&session);
        }
        else if (strcasecmp(cmd, "FEAT") == 0) {
            // tra ve danh sach tinh nang trong
            send_response(client_sock, "211 End\r\n");
        }
        else if (strcasecmp(cmd, "NOOP") == 0) {
            send_response(client_sock, "200 OK\r\n");
        }
        else if (strcasecmp(cmd, "QUIT") == 0) {
            cmd_quit(&session);
            running = 0;
        }
        else {
            send_response(client_sock, "502 Command not implemented\r\n");
        }
    }

    close(client_sock);
}
