# Script Chi Tiết: Account, File System & Server Data Connection

> **Người thực hiện:** Huy  
> **Phần phụ trách:** Quản lý tài khoản, File System, Bảo mật thư mục, PASV/RETR/STOR phía Server

---

## Tổng Quan

Script này mô tả flow xử lý:
1. **Account Management** - Quản lý tài khoản
2. **File System Security** - Bảo vệ thư mục
3. **Server Data Connection** - PASV mode, truyền file

---

## 1. Account Structure

### File: `server/include/account.h`

```c
// Dòng 4-7: Constants
#define MAX_ACCOUNTS 100     // Số tài khoản tối đa
#define MAX_USERNAME 50      // Độ dài username
#define MAX_PASSWORD 50      // Độ dài password  
#define MAX_PATH_LEN 256     // Độ dài đường dẫn

// Dòng 9-14: Cấu trúc Account
typedef struct {
    char username[MAX_USERNAME];
    char password[MAX_PASSWORD];
    char home_dir[MAX_PATH_LEN];
} Account;

// Dòng 16-18: Biến toàn cục
extern Account accounts[MAX_ACCOUNTS];
extern int account_count;
```

---

## 2. Account Loading

### File: `server/src/account.c`

### 2.1 load_accounts()

```c
// Dòng 15-51: Đọc tài khoản từ file
int load_accounts(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) return -1;
    
    char line[512];
    account_count = 0;
    
    while (fgets(line, sizeof(line), fp) != NULL && account_count < MAX_ACCOUNTS) {
        line[strcspn(line, "\r\n")] = 0;  // Xóa newline
        if (strlen(line) == 0) continue;
        
        // Parse: username password home_dir
        char *token = strtok(line, " ");
        if (token == NULL) continue;
        strncpy(accounts[account_count].username, token, MAX_USERNAME - 1);
        
        token = strtok(NULL, " ");
        if (token == NULL) continue;
        strncpy(accounts[account_count].password, token, MAX_PASSWORD - 1);
        
        token = strtok(NULL, " ");
        if (token == NULL) continue;
        strncpy(accounts[account_count].home_dir, token, MAX_PATH_LEN - 1);
        
        account_count++;
    }
    fclose(fp);
    return account_count;
}
```

**File Format (`accounts.txt`):**
```
user1 123456 ./server/data/user1
user2 password ./server/data/user2
```

### 2.2 check_login()

```c
// Dòng 57-65: Xác thực đăng nhập
int check_login(const char *username, const char *password) {
    for (int i = 0; i < account_count; i++) {
        if (strcmp(accounts[i].username, username) == 0 &&
            strcmp(accounts[i].password, password) == 0) {
            return i;  // Trả về index
        }
    }
    return -1;  // Sai thông tin
}
```

### 2.3 add_account() & save_accounts()

```c
// Dòng 71-91: Thêm tài khoản
int add_account(const char *username, const char *password, const char *home_dir) {
    if (account_count >= MAX_ACCOUNTS) return -1;
    for (int i = 0; i < account_count; i++) {
        if (strcmp(accounts[i].username, username) == 0) return -1;
    }
    strncpy(accounts[account_count].username, username, MAX_USERNAME - 1);
    strncpy(accounts[account_count].password, password, MAX_PASSWORD - 1);
    strncpy(accounts[account_count].home_dir, home_dir, MAX_PATH_LEN - 1);
    account_count++;
    return 0;
}

// Dòng 96-112: Lưu file
int save_accounts(const char *filename) {
    FILE *fp = fopen(filename, "w");
    if (fp == NULL) return -1;
    for (int i = 0; i < account_count; i++) {
        fprintf(fp, "%s %s %s\n", accounts[i].username, 
                accounts[i].password, accounts[i].home_dir);
    }
    fclose(fp);
    return 0;
}
```

---

## 3. File System Commands

### File: `server/src/ftp_server.c`

### 3.1 cmd_pwd()

```c
// Dòng 65-75: Trả về thư mục hiện tại
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
```

### 3.2 cmd_cwd() - Bảo mật thư mục

```c
// Dòng 80-134: Đổi thư mục với bảo mật
void cmd_cwd(FTPSession *session, const char *arg) {
    if (!session->logged_in) {
        send_response(session->ctrl_sock, "530 Not logged in\r\n");
        return;
    }

    // CHẶN: Không cho thoát root
    if (strcmp(arg, "..") == 0 && strcmp(session->current_dir, "/") == 0) {
        send_response(session->ctrl_sock, "550 Permission denied\r\n");
        return;
    }

    char new_path[MAX_PATH_LEN];
    char full_path[MAX_PATH_LEN];

    // Xử lý đường dẫn
    if (arg[0] == '/') {
        strncpy(new_path, arg, sizeof(new_path) - 1);  // Tuyệt đối
    } else if (strcmp(arg, "..") == 0) {
        strncpy(new_path, session->current_dir, sizeof(new_path) - 1);
        char *last_slash = strrchr(new_path, '/');
        if (last_slash && last_slash != new_path) *last_slash = '\0';
        else strcpy(new_path, "/");
    } else {
        // Tương đối
        if (strcmp(session->current_dir, "/") == 0)
            snprintf(new_path, sizeof(new_path), "/%s", arg);
        else
            snprintf(new_path, sizeof(new_path), "%s/%s", session->current_dir, arg);
    }

    // Tạo đường dẫn thực: root_dir + current_dir
    snprintf(full_path, sizeof(full_path), "%s%s", session->root_dir, new_path);

    // Kiểm tra tồn tại
    struct stat st;
    if (stat(full_path, &st) == 0 && S_ISDIR(st.st_mode)) {
        strncpy(session->current_dir, new_path, sizeof(session->current_dir) - 1);
        send_response(session->ctrl_sock, "250 Directory changed\r\n");
    } else {
        send_response(session->ctrl_sock, "550 Directory not found\r\n");
    }
}
```

**Virtual Chroot:**
```
User thấy:    /files/docs          (current_dir)
Server thực:  ./data/user1/files/docs  (root_dir + current_dir)
```

---

## 4. PASV Mode - Data Connection Setup

### 4.1 cmd_pasv()

```c
// Dòng 139-197: Thiết lập passive mode
void cmd_pasv(FTPSession *session) {
    if (!session->logged_in) {
        send_response(session->ctrl_sock, "530 Not logged in\r\n");
        return;
    }

    // Đóng socket cũ
    if (session->data_listen_sock >= 0) {
        close(session->data_listen_sock);
    }

    // Tạo socket mới cho data
    session->data_listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(session->data_listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Bind port 0 = kernel chọn port tự do
    struct sockaddr_in data_addr;
    memset(&data_addr, 0, sizeof(data_addr));
    data_addr.sin_family = AF_INET;
    data_addr.sin_addr.s_addr = INADDR_ANY;
    data_addr.sin_port = 0;

    bind(session->data_listen_sock, (struct sockaddr*)&data_addr, sizeof(data_addr));
    listen(session->data_listen_sock, 1);

    // Lấy port được gán
    socklen_t len = sizeof(data_addr);
    getsockname(session->data_listen_sock, (struct sockaddr*)&data_addr, &len);
    int pasv_port = ntohs(data_addr.sin_port);

    // Lấy IP server
    struct sockaddr_in server_addr;
    getsockname(session->ctrl_sock, (struct sockaddr*)&server_addr, &len);
    unsigned char *ip = (unsigned char*)&server_addr.sin_addr.s_addr;

    // Trả về: 227 Entering Passive Mode (h1,h2,h3,h4,p1,p2)
    char response[100];
    snprintf(response, sizeof(response),
             "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)\r\n",
             ip[0], ip[1], ip[2], ip[3],
             (pasv_port >> 8) & 0xFF,   // High byte
             pasv_port & 0xFF);          // Low byte

    send_response(session->ctrl_sock, response);
}
```

**Tính port:** `port = p1 * 256 + p2`
```
VD: p1=78, p2=52 → port = 78*256 + 52 = 20020
```

### 4.2 accept_data_connection()

```c
// Dòng 202-214: Chờ client kết nối data
int accept_data_connection(FTPSession *session) {
    if (session->data_listen_sock < 0) return -1;

    struct sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);

    session->data_sock = accept(session->data_listen_sock, 
                                (struct sockaddr*)&client_addr, &len);

    close(session->data_listen_sock);  // Đóng listen socket
    session->data_listen_sock = -1;

    return session->data_sock;
}
```

---

## 5. cmd_list() - Liệt kê file

```c
// Dòng 219-294: Gửi danh sách file qua data connection
void cmd_list(FTPSession *session) {
    if (!session->logged_in) {
        send_response(session->ctrl_sock, "530 Not logged in\r\n");
        return;
    }

    // Chờ client kết nối data
    int dsock = accept_data_connection(session);
    if (dsock < 0) {
        send_response(session->ctrl_sock, "425 Can't open data connection\r\n");
        return;
    }

    send_response(session->ctrl_sock, "150 Opening data connection\r\n");

    // Tạo đường dẫn thực
    char full_path[MAX_PATH_LEN];
    snprintf(full_path, sizeof(full_path), "%s%s", 
             session->root_dir, session->current_dir);

    // Mở thư mục
    DIR *dir = opendir(full_path);
    if (dir == NULL) {
        close(dsock);
        send_response(session->ctrl_sock, "550 Failed to open directory\r\n");
        return;
    }

    // Đọc và gửi từng entry
    struct dirent *entry;
    struct stat st;
    char filepath[MAX_PATH_LEN];
    char line[512];

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        snprintf(filepath, sizeof(filepath), "%s/%s", full_path, entry->d_name);
        if (stat(filepath, &st) == 0) {
            // Format giống ls -l
            char perms[11] = "----------";
            perms[0] = S_ISDIR(st.st_mode) ? 'd' : '-';
            perms[1] = (st.st_mode & S_IRUSR) ? 'r' : '-';
            // ... (các permission khác)
            
            struct tm *t = localtime(&st.st_mtime);
            snprintf(line, sizeof(line), "%s 1 ftp ftp %8ld %s %2d %02d:%02d %s\r\n",
                     perms, (long)st.st_size, 
                     "Jan\0Feb\0Mar..." + t->tm_mon * 4,
                     t->tm_mday, t->tm_hour, t->tm_min, entry->d_name);
            send(dsock, line, strlen(line), 0);
        }
    }

    closedir(dir);
    close(dsock);
    send_response(session->ctrl_sock, "226 Transfer complete\r\n");
}
```

---

## 6. cmd_retr() - Download File

```c
// Dòng 299-368: Gửi file cho client
void cmd_retr(FTPSession *session, const char *filename) {
    if (!session->logged_in) {
        send_response(session->ctrl_sock, "530 Not logged in\r\n");
        return;
    }

    // Tạo đường dẫn đầy đủ
    char filepath[MAX_PATH_LEN];
    if (filename[0] == '/')
        snprintf(filepath, sizeof(filepath), "%s%s", session->root_dir, filename);
    else
        snprintf(filepath, sizeof(filepath), "%s%s/%s",
                 session->root_dir, session->current_dir, filename);

    // Mở file
    FILE *fp = fopen(filepath, "rb");
    if (fp == NULL) {
        send_response(session->ctrl_sock, "550 File not found\r\n");
        return;
    }

    // Chờ data connection
    int dsock = accept_data_connection(session);
    if (dsock < 0) {
        fclose(fp);
        send_response(session->ctrl_sock, "425 Can't open data connection\r\n");
        return;
    }

    send_response(session->ctrl_sock, "150 Opening data connection\r\n");

    // Gửi file
    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        send(dsock, buffer, bytes_read, 0);
    }

    fclose(fp);
    close(dsock);
    send_response(session->ctrl_sock, "226 Transfer complete\r\n");
}
```

---

## 7. cmd_stor() - Upload File

```c
// Dòng 373-453: Nhận file từ client
void cmd_stor(FTPSession *session, const char *filename) {
    if (!session->logged_in) {
        send_response(session->ctrl_sock, "530 Not logged in\r\n");
        return;
    }

    // Tạo đường dẫn đầy đủ
    char filepath[MAX_PATH_LEN];
    if (filename[0] == '/')
        snprintf(filepath, sizeof(filepath), "%s%s", session->root_dir, filename);
    else
        snprintf(filepath, sizeof(filepath), "%s%s/%s",
                 session->root_dir, session->current_dir, filename);

    // Chờ data connection
    int dsock = accept_data_connection(session);
    if (dsock < 0) {
        send_response(session->ctrl_sock, "425 Can't open data connection\r\n");
        return;
    }

    // Mở file để ghi
    FILE *fp = fopen(filepath, "wb");
    if (fp == NULL) {
        close(dsock);
        send_response(session->ctrl_sock, "550 Cannot create file\r\n");
        return;
    }

    send_response(session->ctrl_sock, "150 Opening data connection\r\n");

    // Nhận và ghi file
    char buffer[BUFFER_SIZE];
    ssize_t bytes_recv;
    while ((bytes_recv = recv(dsock, buffer, sizeof(buffer), 0)) > 0) {
        fwrite(buffer, 1, bytes_recv, fp);
    }

    fclose(fp);
    close(dsock);
    send_response(session->ctrl_sock, "226 Transfer complete\r\n");
}
```

---

## 8. PASV Flow Diagram

```
Client                              Server
  │                                    │
  │─── PASV ─────────────────────────► │ socket() → data_listen_sock
  │                                    │ bind(port=0) → kernel chọn port
  │                                    │ listen()
  │◄── 227 (...127,0,0,1,78,52) ───────│ getsockname() → port 20020
  │                                    │
  │─── connect(port 20020) ──────────► │ accept() → data_sock
  │                                    │
  │─── RETR file.txt ────────────────► │ fopen() + fread()
  │◄── 150 Opening... ─────────────────│
  │◄══════ FILE DATA ══════════════════│ send()
  │◄── 226 Transfer complete ──────────│ close()
```

---

## 9. Tổng Kết

### Files

| File | Chức năng |
|------|-----------|
| `server/src/account.c` | Quản lý tài khoản |
| `server/src/ftp_server.c` | cmd_pwd, cmd_cwd, cmd_list, cmd_pasv, cmd_retr, cmd_stor |
| `server/include/account.h` | Account struct |

### Hàm Quan Trọng

| Hàm | Mục đích |
|-----|----------|
| `load_accounts()` | Đọc file → mảng |
| `check_login()` | Xác thực |
| `cmd_pasv()` | Tạo data socket, trả về IP:port |
| `accept_data_connection()` | Accept từ client |
| `cmd_list()` | Gửi directory listing |
| `cmd_retr()` | Gửi file (download) |
| `cmd_stor()` | Nhận file (upload) |

### Cơ Chế Bảo Mật

| Cơ chế | Mô tả |
|--------|-------|
| Virtual Chroot | User bị giới hạn trong home |
| Path Mapping | `/` → `./data/userX` |
| Parent Block | Chặn `CWD ..` tại root |

---

> **Ghi chú:** Script này bao gồm cả Account Management, File System và Server-side Data Connection (PASV/RETR/STOR).
