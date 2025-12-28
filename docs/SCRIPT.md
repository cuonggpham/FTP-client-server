# SCRIPT PHÁT TRIỂN DỰ ÁN FTP CLIENT-SERVER

> Script này mô tả chi tiết flow xử lý từ đầu đến cuối để code toàn bộ dự án FTP Client-Server sử dụng Socket TCP trong C.

---

## PHASE 1: CHUẨN BỊ DỰ ÁN

### 1.1 Khởi tạo cấu trúc thư mục

```bash
FTP-client-server/
├── server/
│   ├── include/          # Header files cho server
│   ├── src/              # Source code server
│   ├── helpers/          # Tiện ích (logger, utils)
│   └── data/             # Dữ liệu (accounts, user folders)
├── client/
│   ├── include/          # Header files cho client
│   └── src/              # Source code client
├── bin/                  # Chứa file executable sau khi build
├── build/                # Object files
└── docs/                 # Tài liệu tham khảo (RFC 959)
```

### 1.2 Tạo Makefile

**Mục đích:** Quản lý quá trình biên dịch tự động

**Các target cần định nghĩa:**
- `all`: Build toàn bộ (server + client)
- `server`: Build riêng server
- `client`: Build riêng client
- `clean`: Xóa file build

**Compiler flags:** `-Wall -Wextra -pthread` (cần pthread cho multi-threading)

---

## PHASE 2: PHÁT TRIỂN SERVER

### Bước 2.1: Tạo file header `ftp_server.h`

**Định nghĩa cấu trúc FTPSession:**
```c
typedef struct {
    int session_id;           // ID phiên duy nhất
    int ctrl_sock;            // Socket điều khiển
    int logged_in;            // Trạng thái đăng nhập (0/1)
    int account_index;        // Index trong mảng accounts
    char username[50];        // Username tạm (trước khi xác thực)
    char current_dir[256];    // Thư mục hiện tại (relative path)
    char root_dir[256];       // Thư mục gốc (absolute path)
    struct sockaddr_in client_addr;
    int data_listen_sock;     // Socket lắng nghe data (-1 nếu chưa dùng)
    int data_sock;            // Socket data (-1 nếu chưa kết nối)
} FTPSession;
```

**Khai báo các hàm xử lý lệnh FTP:**
```c
void handle_client(int client_sock, struct sockaddr_in client_addr, int session_id);
void cmd_user(FTPSession *session, const char *arg);
void cmd_pass(FTPSession *session, const char *arg);
void cmd_pwd(FTPSession *session);
void cmd_cwd(FTPSession *session, const char *arg);
void cmd_pasv(FTPSession *session);
void cmd_list(FTPSession *session);
void cmd_retr(FTPSession *session, const char *filename);
void cmd_stor(FTPSession *session, const char *filename);
void cmd_type(FTPSession *session, const char *arg);
void cmd_syst(FTPSession *session);
void cmd_quit(FTPSession *session);
```

---

### Bước 2.2: Tạo hệ thống quản lý tài khoản

**File:** `account.h` và `account.c`

**Cấu trúc Account:**
```c
typedef struct {
    char username[50];
    char password[50];
    char home_dir[256];  // Thư mục gốc của user
} Account;
```

**Các hàm cần implement:**
1. `load_accounts(filename)` - Đọc danh sách tài khoản từ file
2. `save_accounts(filename)` - Lưu danh sách tài khoản vào file
3. `add_account(username, password, home_dir)` - Thêm tài khoản mới
4. `check_login(username, password)` - Kiểm tra đăng nhập

**Format file accounts.txt:**
```
username password /path/to/home
user1 123456 ./server/data/user1
admin admin123 ./server/data/admin
```

---

### Bước 2.3: Implement server.c (Main entry point)

**Flow xử lý chính:**

```
┌─────────────────────────────────────────────────────────────────┐
│                        SERVER MAIN                               │
├─────────────────────────────────────────────────────────────────┤
│ 1. Parse command line arguments (port)                           │
│ 2. Initialize logger                                             │
│ 3. Load accounts từ file accounts.txt                            │
│ 4. Tạo socket TCP (AF_INET, SOCK_STREAM)                         │
│ 5. Set SO_REUSEADDR option                                       │
│ 6. Bind socket to port (mặc định 2121)                           │
│ 7. Listen với backlog = 10                                       │
│ 8. ┌─────────────────────────────────────────────────────────┐   │
│    │              VÒNG LẶP ACCEPT                            │   │
│    ├─────────────────────────────────────────────────────────┤   │
│    │ a. accept() chờ client kết nối                          │   │
│    │ b. Tạo session_id duy nhất (mutex lock)                 │   │
│    │ c. Allocate ClientInfo struct                           │   │
│    │ d. pthread_create() - Tạo thread mới cho client         │   │
│    │ e. pthread_detach() - Thread tự động cleanup khi xong   │   │
│    │ f. Quay lại bước (a)                                    │   │
│    └─────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

**Code quan trọng:**
```c
// Tạo session ID thread-safe
static pthread_mutex_t session_id_mutex = PTHREAD_MUTEX_INITIALIZER;

int get_next_session_id(void) {
    pthread_mutex_lock(&session_id_mutex);
    int id = ++next_session_id;
    pthread_mutex_unlock(&session_id_mutex);
    return id;
}

// Thread handler
void *client_thread(void *arg) {
    ClientInfo *info = (ClientInfo *)arg;
    handle_client(info->client_sock, info->client_addr, info->session_id);
    free(info);
    return NULL;
}
```

---

### Bước 2.4: Implement ftp_server.c (Xử lý lệnh FTP)

#### 2.4.1 Hàm handle_client() - Vòng lặp chính

```
┌───────────────────────────────────────────────────────────────────┐
│                     handle_client()                                │
├───────────────────────────────────────────────────────────────────┤
│ 1. Khởi tạo FTPSession với giá trị mặc định                       │
│    - session_id, ctrl_sock, client_addr                           │
│    - logged_in = 0                                                 │
│    - current_dir = "/"                                             │
│    - data_listen_sock = -1, data_sock = -1                        │
│                                                                    │
│ 2. Gửi welcome message: "220 FTP Server Ready\r\n"                │
│                                                                    │
│ 3. ┌─────────────────────────────────────────────────────────┐    │
│    │              VÒNG LẶP NHẬN LỆNH                         │    │
│    ├─────────────────────────────────────────────────────────┤    │
│    │ a. recv() đọc lệnh từ client                            │    │
│    │ b. Xóa \r\n ở cuối lệnh                                 │    │
│    │ c. Parse: tách command và argument                      │    │
│    │ d. Log lệnh ra console/file                             │    │
│    │ e. Switch-case xử lý từng lệnh:                         │    │
│    │    - USER → cmd_user()                                  │    │
│    │    - PASS → cmd_pass()                                  │    │
│    │    - PWD  → cmd_pwd()                                   │    │
│    │    - CWD  → cmd_cwd()                                   │    │
│    │    - PASV → cmd_pasv()                                  │    │
│    │    - LIST → cmd_list()                                  │    │
│    │    - RETR → cmd_retr()                                  │    │
│    │    - STOR → cmd_stor()                                  │    │
│    │    - TYPE → cmd_type()                                  │    │
│    │    - SYST → cmd_syst()                                  │    │
│    │    - QUIT → cmd_quit(), break loop                      │    │
│    │    - Khác → "502 Command not implemented"               │    │
│    │ f. Quay lại bước (a)                                    │    │
│    └─────────────────────────────────────────────────────────┘    │
│                                                                    │
│ 4. Đóng data sockets nếu còn mở                                   │
│ 5. Đóng control socket                                            │
└───────────────────────────────────────────────────────────────────┘
```

---

#### 2.4.2 Implement các lệnh xác thực

**cmd_user():**
```
Input:  USER username
Output: 331 Username OK, need password

Flow:
1. Kiểm tra có argument không → 501 nếu thiếu
2. Lưu username vào session->username
3. Gửi response 331
```

**cmd_pass():**
```
Input:  PASS password
Output: 230 User logged in / 530 Login incorrect

Flow:
1. Kiểm tra đã gửi USER chưa → 503 nếu chưa
2. Gọi check_login(username, password)
3. Nếu thành công:
   - session->logged_in = 1
   - session->account_index = index
   - session->root_dir = accounts[index].home_dir
   - session->current_dir = "/"
   - Gửi 230
4. Nếu thất bại: Gửi 530
```

---

#### 2.4.3 Implement các lệnh điều hướng

**cmd_pwd():**
```
Input:  PWD
Output: 257 "/path" is current directory

Flow:
1. Kiểm tra đăng nhập → 530 nếu chưa
2. Gửi: 257 "{current_dir}" is current directory
```

**cmd_cwd():**
```
Input:  CWD path
Output: 250 Directory changed / 550 Directory not found

Flow:
1. Kiểm tra đăng nhập → 530 nếu chưa
2. Kiểm tra có argument → 501 nếu thiếu
3. Xử lý path:
   a. Nếu path = ".." và current_dir = "/" → 550 Permission denied
   b. Nếu path bắt đầu bằng "/" → Đường dẫn tuyệt đối trong sandbox
   c. Nếu path = ".." → Lên thư mục cha
   d. Ngược lại → Đường dẫn tương đối

4. Tạo full_path = root_dir + new_path
5. Kiểm tra thư mục tồn tại bằng stat()
6. Nếu hợp lệ: cập nhật current_dir, gửi 250
7. Nếu không: gửi 550
```

---

#### 2.4.4 Implement Passive Mode (PASV)

**cmd_pasv():**
```
Input:  PASV
Output: 227 Entering Passive Mode (h1,h2,h3,h4,p1,p2)

Flow:
┌─────────────────────────────────────────────────────────────────┐
│ 1. Kiểm tra đăng nhập                                           │
│ 2. Đóng data socket cũ nếu còn mở                               │
│ 3. Tạo data_listen_sock = socket(AF_INET, SOCK_STREAM, 0)       │
│ 4. Set SO_REUSEADDR                                             │
│ 5. Bind với port = 0 (OS tự cấp port ngẫu nhiên)                │
│ 6. Listen với backlog = 1                                       │
│ 7. Lấy port được cấp bằng getsockname()                         │
│ 8. Lấy IP của server từ control socket                          │
│ 9. Tính p1 = port >> 8, p2 = port & 0xFF                        │
│ 10. Gửi: 227 Entering Passive Mode (ip1,ip2,ip3,ip4,p1,p2)      │
└─────────────────────────────────────────────────────────────────┘
```

**accept_data_connection():**
```
Flow:
1. Kiểm tra data_listen_sock != -1
2. accept() chờ client kết nối
3. Đóng data_listen_sock
4. Trả về data_sock
```

---

#### 2.4.5 Implement LIST

**cmd_list():**
```
Input:  LIST
Output: 150 → [directory listing] → 226

Flow:
┌─────────────────────────────────────────────────────────────────┐
│ 1. Kiểm tra đăng nhập                                           │
│ 2. Kiểm tra PASV đã được gọi                                    │
│ 3. Gửi LIST command confirmation                                │
│ 4. Accept data connection                                       │
│ 5. Gửi "150 Opening data connection"                            │
│ 6. Tạo full_path = root_dir + current_dir                       │
│ 7. opendir(full_path)                                           │
│ 8. Loop qua từng entry:                                         │
│    - Skip "." và ".."                                           │
│    - Lấy thông tin file bằng stat()                             │
│    - Tạo chuỗi format giống ls -l:                              │
│      drwxr-xr-x 1 ftp ftp 4096 Dec 27 14:30 dirname             │
│      -rw-r--r-- 1 ftp ftp 1234 Dec 27 14:30 filename            │
│    - Gửi qua data socket                                        │
│ 9. closedir()                                                   │
│ 10. Đóng data socket                                            │
│ 11. Gửi "226 Transfer complete"                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

#### 2.4.6 Implement RETR (Download)

**cmd_retr():**
```
Input:  RETR filename
Output: 150 → [file data] → 226

Flow:
┌─────────────────────────────────────────────────────────────────┐
│ 1. Kiểm tra đăng nhập                                           │
│ 2. Kiểm tra có filename                                         │
│ 3. Kiểm tra PASV đã được gọi                                    │
│ 4. Tạo filepath = root_dir + current_dir + "/" + filename       │
│ 5. Mở file với fopen(filepath, "rb")                            │
│    - Nếu lỗi → 550 File not found                               │
│ 6. Accept data connection                                       │
│ 7. Gửi "150 Opening data connection"                            │
│ 8. Loop đọc file:                                               │
│    while ((bytes = fread(buffer, 1, 4096, fp)) > 0) {           │
│        send(data_sock, buffer, bytes, 0);                       │
│    }                                                            │
│ 9. Đóng file                                                    │
│ 10. Đóng data socket                                            │
│ 11. Kiểm tra lỗi:                                               │
│     - Có lỗi I/O → 426 Transfer aborted                         │
│     - Thành công → 226 Transfer complete                        │
└─────────────────────────────────────────────────────────────────┘
```

---

#### 2.4.7 Implement STOR (Upload)

**cmd_stor():**
```
Input:  STOR filename
Output: 150 → [receive file data] → 226

Flow:
┌─────────────────────────────────────────────────────────────────┐
│ 1. Kiểm tra đăng nhập                                           │
│ 2. Kiểm tra có filename                                         │
│ 3. Kiểm tra PASV đã được gọi                                    │
│ 4. Tạo filepath = root_dir + current_dir + "/" + filename       │
│ 5. Accept data connection                                       │
│ 6. Mở file với fopen(filepath, "wb")                            │
│    - Nếu lỗi permission → 450                                   │
│    - Nếu lỗi khác → 550                                         │
│ 7. Gửi "150 Opening data connection"                            │
│ 8. Loop nhận data:                                              │
│    while ((bytes = recv(data_sock, buffer, 4096, 0)) > 0) {     │
│        fwrite(buffer, 1, bytes, fp);                            │
│    }                                                            │
│ 9. Đóng file                                                    │
│ 10. Đóng data socket                                            │
│ 11. Kiểm tra lỗi:                                               │
│     - recv() < 0 → unlink(filepath), gửi 426                    │
│     - I/O error → unlink(filepath), gửi 426                     │
│     - Thành công → 226 Transfer complete                        │
└─────────────────────────────────────────────────────────────────┘
```

---

### Bước 2.5: Implement Logger

**File:** `helpers/logger.c` và `helpers/logger.h`

**Format log:**
```
[YYYY-MM-DD HH:MM:SS] [SID=123] [127.0.0.1] COMMAND argument
```

**Các hàm:**
```c
int init_logger(void);      // Mở file log
void close_logger(void);    // Đóng file log
void log_info(const char *format, ...);   // Log thông tin
void log_error(const char *format, ...);  // Log lỗi
```

---

## PHASE 3: PHÁT TRIỂN CLIENT

### Bước 3.1: Tạo file header `ftp_client.h`

**Cấu trúc FTPClient:**
```c
typedef struct {
    int ctrl_sock;       // Socket điều khiển
    char server_ip[50];  // IP server
    int server_port;     // Port server
    int logged_in;       // Trạng thái đăng nhập
} FTPClient;
```

**Khai báo các hàm:**
```c
int ftp_connect(FTPClient *client, const char *host, int port);
void ftp_disconnect(FTPClient *client);
int ftp_login(FTPClient *client, const char *user, const char *pass);
int ftp_pwd(FTPClient *client);
int ftp_cwd(FTPClient *client, const char *dir);
int ftp_list(FTPClient *client);
int ftp_retr(FTPClient *client, const char *filename, const char *local_path);
int ftp_stor(FTPClient *client, const char *local_path, const char *remote_name);
int ftp_quit(FTPClient *client);
```

---

### Bước 3.2: Implement ftp_client.c

#### 3.2.1 Kết nối và ngắt kết nối

**ftp_connect():**
```
Flow:
1. Tạo socket TCP
2. Thiết lập địa chỉ server (inet_pton)
3. connect() đến server
4. Nhận welcome message (220)
5. Lưu thông tin kết nối vào struct
6. Return 0 nếu thành công, -1 nếu thất bại
```

**ftp_disconnect():**
```
Flow:
1. Đóng ctrl_sock
2. Reset các trường trong struct
```

---

#### 3.2.2 Hàm tiện ích

**ftp_send_cmd():**
```c
int ftp_send_cmd(FTPClient *client, const char *cmd) {
    char buffer[512];
    snprintf(buffer, sizeof(buffer), "%s\r\n", cmd);
    return send(client->ctrl_sock, buffer, strlen(buffer), 0);
}
```

**ftp_recv_response():**
```c
int ftp_recv_response(FTPClient *client, char *buffer, int size) {
    int bytes = recv(client->ctrl_sock, buffer, size - 1, 0);
    if (bytes > 0) buffer[bytes] = '\0';
    return bytes;
}
```

**parse_pasv_response():**
```
Input:  "227 Entering Passive Mode (127,0,0,1,195,80)"
Output: ip = "127.0.0.1", port = 50000 (195*256 + 80)

Flow:
1. Tìm dấu '(' trong response
2. sscanf lấy 6 số (h1,h2,h3,h4,p1,p2)
3. Ghép h1.h2.h3.h4 thành IP
4. Tính port = p1*256 + p2
```

---

#### 3.2.3 Implement các lệnh client

**ftp_login():**
```
Flow:
1. Gửi "USER username"
2. Nhận response (331 expected)
3. Gửi "PASS password"
4. Nhận response
5. Nếu 230 → logged_in = 1, return 0
6. Nếu 530 → return -1
```

**ftp_list():**
```
Flow:
1. Gửi "PASV"
2. Nhận và parse response → lấy IP, port
3. Tạo data socket, connect đến IP:port
4. Gửi "LIST"
5. Nhận response (150)
6. Loop recv() từ data socket, in ra màn hình
7. Đóng data socket
8. Nhận response cuối (226)
```

**ftp_retr():**
```
Flow:
1. Gửi "PASV", parse response
2. Connect data socket
3. Gửi "RETR filename"
4. Nhận response (150)
5. Mở local file để ghi
6. Loop recv() từ data socket, ghi vào file
7. Đóng file và data socket
8. Nhận response cuối (226)
```

**ftp_stor():**
```
Flow:
1. Gửi "PASV", parse response
2. Connect data socket
3. Gửi "STOR filename"
4. Nhận response (150)
5. Mở local file để đọc
6. Loop fread() → send() qua data socket
7. Đóng file và data socket
8. Nhận response cuối (226)
```

---

### Bước 3.3: Implement client.c (Main entry point)

**Flow xử lý:**
```
┌─────────────────────────────────────────────────────────────────┐
│                        CLIENT MAIN                               │
├─────────────────────────────────────────────────────────────────┤
│ 1. Parse arguments (host, port)                                  │
│ 2. ftp_connect() kết nối đến server                              │
│ 3. Nhập username/password từ bàn phím                            │
│ 4. ftp_login() xác thực                                          │
│ 5. Hiển thị help                                                 │
│ 6. ┌─────────────────────────────────────────────────────────┐   │
│    │              VÒNG LẶP NHẬP LỆNH                         │   │
│    ├─────────────────────────────────────────────────────────┤   │
│    │ a. In prompt "ftp> "                                    │   │
│    │ b. fgets() đọc lệnh từ stdin                            │   │
│    │ c. Parse: tách command và argument                      │   │
│    │ d. Switch-case xử lý:                                   │   │
│    │    - PWD  → ftp_pwd()                                   │   │
│    │    - CWD  → ftp_cwd()                                   │   │
│    │    - LIST → ftp_list()                                  │   │
│    │    - RETR → ftp_retr()                                  │   │
│    │    - STOR → ftp_stor()                                  │   │
│    │    - QUIT → ftp_quit(), exit loop                       │   │
│    │    - HELP → print_help()                                │   │
│    │ e. Quay lại bước (a)                                    │   │
│    └─────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

---

## PHASE 4: TESTING & DEBUGGING

### 4.1 Build dự án

```bash
make clean
make all
```

**Kết quả:**
- `bin/ftp_server` - Server executable
- `bin/ftp_client` - Client executable

### 4.2 Test cơ bản

**Terminal 1 - Chạy server:**
```bash
./bin/ftp_server 2121
```

**Terminal 2 - Chạy client:**
```bash
./bin/ftp_client 127.0.0.1 2121
```

**Test sequence:**
```
Username: user1
Password: 123456

ftp> PWD
ftp> LIST
ftp> CWD documents
ftp> RETR file.txt
ftp> STOR local_file.txt
ftp> QUIT
```

### 4.3 Test với FileZilla

**Kết nối:**
- Host: 127.0.0.1
- Port: 2121
- Username: user1
- Password: 123456

**Kiểm tra:**
- [x] Kết nối thành công
- [x] Liệt kê thư mục
- [x] Chuyển thư mục
- [x] Download file
- [x] Upload file

---

## PHASE 5: CÁC ĐIỂM CẦN CHÚ Ý

### 5.1 Response Format

Mọi response phải kết thúc bằng `\r\n`:
```c
send_response(sock, "220 FTP Server Ready\r\n");
```

### 5.2 Xử lý đường dẫn

- **current_dir**: Luôn bắt đầu bằng `/`, là đường dẫn trong sandbox
- **root_dir**: Đường dẫn thực tế trên hệ thống
- **Đường dẫn thực** = root_dir + current_dir + "/" + filename

### 5.3 Bảo mật Sandbox

- Không cho phép `CWD ..` khi đang ở root `/`
- Kiểm tra path traversal attack (`../../../etc/passwd`)

### 5.4 Xử lý lỗi

- Luôn kiểm tra return value của socket functions
- Đóng socket khi có lỗi
- Ghi log chi tiết để debug

### 5.5 Thread Safety

- Sử dụng mutex cho session ID counter
- Mỗi thread có FTPSession riêng (không chia sẻ state)

---

## TÓM TẮT THỨ TỰ CODE

| Thứ tự | File | Mô tả |
|--------|------|-------|
| 1 | `Makefile` | Cấu hình build |
| 2 | `server/include/account.h` | Định nghĩa Account |
| 3 | `server/src/account.c` | Implement quản lý tài khoản |
| 4 | `server/helpers/logger.h` | Định nghĩa logger |
| 5 | `server/helpers/logger.c` | Implement logger |
| 6 | `server/include/ftp_server.h` | Định nghĩa FTPSession, khai báo hàm |
| 7 | `server/src/ftp_server.c` | Implement xử lý lệnh FTP |
| 8 | `server/src/server.c` | Main entry, multi-threading |
| 9 | `client/include/ftp_client.h` | Định nghĩa FTPClient |
| 10 | `client/src/ftp_client.c` | Implement các lệnh client |
| 11 | `client/src/client.c` | Main entry, UI/UX |

---

## KẾT LUẬN

Script này đã mô tả chi tiết toàn bộ flow phát triển dự án FTP Client-Server từ đầu đến cuối, bao gồm:

1. **Cấu trúc dự án** - Tổ chức thư mục và file hợp lý
2. **Server architecture** - Multi-threaded, xử lý nhiều client đồng thời
3. **FTP Protocol** - Các lệnh theo chuẩn RFC 959
4. **Data transfer** - Passive mode với dynamic port
5. **Security** - Sandbox (chroot giả) cho mỗi user
6. **Client** - Giao diện dòng lệnh đơn giản
7. **Testing** - Kiểm tra với custom client và FileZilla

Tuân theo script này sẽ giúp phát triển dự án một cách có hệ thống và dễ bảo trì.
