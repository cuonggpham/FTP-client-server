# Script Chi Tiết: FTP Server Core - Socket, Đa Client, Control Connection

> **Người thực hiện:** Cương  
> **Phần phụ trách:** FTP Server core, socket handling, multi-client architecture, control connection

---

## Tổng Quan

Script này mô tả chi tiết flow xử lý của FTP Server, bao gồm:
1. **Socket Server Core** - Thiết lập socket và lắng nghe kết nối
2. **Đa Client (Multi-threading)** - Xử lý nhiều client đồng thời
3. **Control Connection** - Quản lý phiên làm việc và xử lý lệnh FTP

---

## 1. Server Core - Khởi Động và Socket Setup

### File: `server/src/server.c`

### 1.1 Headers và Constants

```c
// Dòng 6-15: Include các thư viện cần thiết
#include <stdio.h>          // Input/output chuẩn
#include <stdlib.h>         // Cấp phát bộ nhớ, exit
#include <string.h>         // Xử lý chuỗi
#include <unistd.h>         // close(), read(), write()
#include <errno.h>          // Mã lỗi hệ thống
#include <signal.h>         // Xử lý signal
#include <sys/socket.h>     // Socket API
#include <netinet/in.h>     // Cấu trúc sockaddr_in
#include <arpa/inet.h>      // inet_ntoa() - chuyển IP thành chuỗi
#include <pthread.h>        // Thư viện đa luồng

// Dòng 21-22: Định nghĩa hằng số
#define DEFAULT_PORT 2121                        // Cổng mặc định của server
#define ACCOUNT_FILE "./server/data/accounts.txt" // File lưu tài khoản
```

**Giải thích:**
- `sys/socket.h`: Cung cấp các hàm `socket()`, `bind()`, `listen()`, `accept()`
- `netinet/in.h`: Định nghĩa cấu trúc `sockaddr_in` cho địa chỉ IPv4
- `pthread.h`: Hỗ trợ tạo thread để xử lý đa client
- Port `2121` được chọn thay vì `21` vì port 21 cần quyền root

---

### 1.2 Session ID Management (Thread-safe)

```c
// Dòng 24-34: Biến đếm session toàn cục với mutex

static int next_session_id = 0;                           // Bộ đếm session
static pthread_mutex_t session_id_mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex bảo vệ

// Hàm tạo session ID duy nhất (an toàn đa luồng)
static int get_next_session_id(void) {
    pthread_mutex_lock(&session_id_mutex);    // Khóa mutex
    int id = ++next_session_id;               // Tăng và lấy giá trị mới
    pthread_mutex_unlock(&session_id_mutex);  // Mở khóa mutex
    return id;
}
```

**Flow xử lý:**
```
Thread 1: lock mutex → đọc next_session_id (0) → tăng lên 1 → trả về 1 → unlock
Thread 2: (đợi) → lock mutex → đọc next_session_id (1) → tăng lên 2 → trả về 2 → unlock
```

**Tại sao cần mutex?**
- Khi nhiều client kết nối cùng lúc, nhiều thread đọc/ghi biến `next_session_id` đồng thời
- Không có mutex có thể dẫn đến race condition: 2 client có cùng session ID

---

### 1.3 Client Info Structure

```c
// Dòng 36-41: Cấu trúc truyền dữ liệu cho thread
typedef struct {
    int client_sock;                  // File descriptor của socket client
    struct sockaddr_in client_addr;   // Địa chỉ IP và port của client
    int session_id;                   // ID phiên làm việc duy nhất
} ClientInfo;
```

**Giải thích:**
- `client_sock`: Socket descriptor, dùng để gửi/nhận dữ liệu với client
- `client_addr`: Chứa IP (`sin_addr`) và port (`sin_port`) của client
- `session_id`: Dùng để log và quản lý phiên, mỗi client có ID riêng

---

### 1.4 Hàm main() - Server Startup

```c
// Dòng 60-66: Đọc port từ command line hoặc dùng mặc định
int main(int argc, char *argv[]) {
    int port = DEFAULT_PORT;    // Mặc định 2121
    if (argc > 1) {
        port = atoi(argv[1]);   // Nếu có tham số, chuyển thành số
    }
```

**Cách chạy:**
```bash
./server          # Chạy với port 2121
./server 2222     # Chạy với port 2222
```

---

### 1.5 Load Account File

```c
// Dòng 73-82: Đọc danh sách tài khoản
log_info("Loading account file...");

if (load_accounts(ACCOUNT_FILE) < 0) {
    log_info("Cannot load account file, creating new file...");
    add_account("user1", "123456", "./data/user1");  // Tạo tài khoản mặc định
    save_accounts(ACCOUNT_FILE);                     // Lưu vào file
}
```

**Flow:**
```
1. Thử đọc file accounts.txt
2. Nếu file không tồn tại hoặc lỗi:
   - Tạo tài khoản mặc định: user1/123456
   - Lưu vào file mới
3. Nếu thành công: danh sách accounts được load vào bộ nhớ
```

---

### 1.6 Tạo Socket Server

```c
// Dòng 84-90: Tạo socket TCP
int server_sock = socket(AF_INET, SOCK_STREAM, 0);
if (server_sock < 0) {
    log_error("Cannot create socket: %s", strerror(errno));
    close_logger();
    return 1;
}
```

**Giải thích tham số `socket()`:**
| Tham số | Giá trị | Ý nghĩa |
|---------|---------|---------|
| `AF_INET` | 2 | Sử dụng IPv4 |
| `SOCK_STREAM` | 1 | Sử dụng TCP (đảm bảo thứ tự, tin cậy) |
| `0` | 0 | Protocol mặc định (TCP cho SOCK_STREAM) |

**Kết quả:** Trả về file descriptor (số nguyên dương) hoặc -1 nếu lỗi

---

### 1.7 Socket Options

```c
// Dòng 92-94: Cho phép tái sử dụng địa chỉ
int opt = 1;
setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
```

**Tại sao cần `SO_REUSEADDR`?**
- Khi server dừng, port có thể ở trạng thái `TIME_WAIT` trong 2-4 phút
- Không có option này: `bind()` sẽ thất bại với "Address already in use"
- Có option này: Có thể bind lại port ngay lập tức

---

### 1.8 Thiết Lập Địa Chỉ Server

```c
// Dòng 96-101: Cấu hình địa chỉ server
struct sockaddr_in server_addr;
memset(&server_addr, 0, sizeof(server_addr));  // Xóa sạch bộ nhớ
server_addr.sin_family = AF_INET;              // IPv4
server_addr.sin_addr.s_addr = INADDR_ANY;      // Lắng nghe trên tất cả interface
server_addr.sin_port = htons(port);            // Chuyển port sang network byte order
```

**Giải thích `htons()`:**
- `htons` = Host TO Network Short (16-bit)
- Computer dùng Little-Endian: `2121 = 0x0849` → lưu `49 08`
- Network dùng Big-Endian: `2121 = 0x0849` → gửi `08 49`
- `htons()` chuyển đổi thứ tự byte cho port

**`INADDR_ANY` (0.0.0.0):**
- Lắng nghe trên tất cả network interfaces
- Cả localhost (127.0.0.1) lẫn IP thực (192.168.x.x)

---

### 1.9 Bind Socket

```c
// Dòng 103-109: Gán địa chỉ cho socket
if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
    log_error("Bind failed: %s", strerror(errno));
    close(server_sock);
    close_logger();
    return 1;
}
```

**Bind làm gì?**
- Gán địa chỉ IP:Port cho socket
- Sau bind, socket này "sở hữu" port 2121
- Các process khác không thể bind vào port này nữa

**Lỗi phổ biến:**
- `EADDRINUSE`: Port đang được sử dụng
- `EACCES`: Không có quyền (port < 1024 cần root)

---

### 1.10 Listen

```c
// Dòng 111-117: Bắt đầu lắng nghe kết nối
if (listen(server_sock, 10) < 0) {
    log_error("Listen failed: %s", strerror(errno));
    close(server_sock);
    close_logger();
    return 1;
}
```

**Tham số `backlog = 10`:**
- Số lượng kết nối chờ trong hàng đợi
- Khi client gọi `connect()`, kernel đưa vào hàng đợi
- `accept()` lấy kết nối ra khỏi hàng đợi
- Nếu hàng đợi đầy, client mới sẽ bị từ chối

```
+-------------+    connect()    +-----------+    accept()    +-------------+
|   Client    | -------------->  | Backlog   | ------------->  |   Server    |
|   (new)     |                 | Queue(10) |                |   Thread    |
+-------------+                 +-----------+                +-------------+
```

---

## 2. Multi-Client Architecture (Đa Luồng)

### 2.1 Vòng Lặp Accept Chính

```c
// Dòng 123-165: Main accept loop
while (1) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    // BLOCKING: Đợi cho đến khi có kết nối mới
    int client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
    if (client_sock < 0) {
        log_error("Accept failed: %s", strerror(errno));
        continue;  // Tiếp tục đợi kết nối khác
    }
```

**`accept()` hoạt động:**
1. **Blocking**: Hàm này sẽ chờ (block) cho đến khi có client kết nối
2. **Kết quả**: Trả về socket MỚI dành riêng cho client này
3. **Socket gốc**: `server_sock` vẫn tiếp tục lắng nghe

```
Server Socket (port 2121)        Client Sockets
       │                              │
       ├─── accept() ──────────────► client_sock_1 ◄─── Client A
       │                              │
       ├─── accept() ──────────────► client_sock_2 ◄─── Client B
       │                              │
       └─── accept() ──────────────► client_sock_3 ◄─── Client C
```

---

### 2.2 Tạo Session ID và Log

```c
    // Dòng 135-141: Tạo session ID và ghi log
    int session_id = get_next_session_id();
    
    log_info("[SID=%d] Client connected: %s:%d", 
           session_id,
           inet_ntoa(client_addr.sin_addr),  // Chuyển IP thành chuỗi "192.168.1.100"
           ntohs(client_addr.sin_port));     // Chuyển port về host byte order
```

**Ví dụ log:**
```
14:30:25 [INFO] [SID=1] Client connected: 192.168.1.100:54321
14:30:26 [INFO] [SID=2] Client connected: 192.168.1.101:54322
```

---

### 2.3 Cấp Phát Memory cho Client Info

```c
    // Dòng 143-152: Tạo cấu trúc lưu thông tin client
    ClientInfo *info = (ClientInfo *)malloc(sizeof(ClientInfo));
    if (info == NULL) {
        log_error("Cannot allocate memory: %s", strerror(errno));
        close(client_sock);
        continue;
    }
    info->client_sock = client_sock;
    info->client_addr = client_addr;
    info->session_id = session_id;
```

**Tại sao dùng `malloc()`?**
- Thread mới cần truy cập thông tin client
- Nếu dùng biến local, khi main loop tiếp tục, biến sẽ bị ghi đè
- `malloc()` đảm bảo memory tồn tại cho đến khi `free()`

---

### 2.4 Tạo Thread Mới

```c
    // Dòng 154-164: Tạo thread xử lý client
    pthread_t tid;
    if (pthread_create(&tid, NULL, client_thread, (void *)info) != 0) {
        log_error("Cannot create thread: %s", strerror(errno));
        close(client_sock);
        free(info);
        continue;
    }
    
    // Tách thread - tự động cleanup khi kết thúc
    pthread_detach(tid);
}
```

**`pthread_create()` tham số:**
| Tham số | Ý nghĩa |
|---------|---------|
| `&tid` | Con trỏ lưu thread ID |
| `NULL` | Sử dụng attributes mặc định |
| `client_thread` | Hàm sẽ chạy trong thread mới |
| `(void *)info` | Tham số truyền cho hàm |

**`pthread_detach()`:**
- Thread sẽ tự động giải phóng resources khi kết thúc
- Không cần `pthread_join()` để đợi thread kết thúc

---

### 2.5 Client Thread Function

```c
// Dòng 46-58: Hàm xử lý cho mỗi thread client
void *client_thread(void *arg) {
    ClientInfo *info = (ClientInfo *)arg;  // Cast về đúng kiểu
    
    // Gọi hàm xử lý chính
    handle_client(info->client_sock, info->client_addr, info->session_id);
    
    // Log khi client ngắt kết nối
    log_info("[SID=%d] Client disconnected: %s:%d", 
           info->session_id,
           inet_ntoa(info->client_addr.sin_addr), 
           ntohs(info->client_addr.sin_port));
    
    free(info);  // Giải phóng memory đã malloc
    return NULL;
}
```

**Flow hoàn chỉnh của một client:**
```
1. accept() → client_sock
2. malloc(ClientInfo) → info
3. pthread_create() → thread mới
4. Thread: handle_client() → xử lý lệnh FTP trong vòng lặp
5. Client gửi QUIT hoặc ngắt kết nối
6. handle_client() return
7. free(info)
8. Thread tự động cleanup
```

---

## 3. Control Connection - Session Management

### File: `server/src/ftp_server.c`

### 3.1 FTPSession Structure

```c
// File: server/include/ftp_server.h (Dòng 9-21)
typedef struct {
    int session_id;             // ID phiên duy nhất (1, 2, 3,...)
    int ctrl_sock;              // Socket điều khiển (gửi/nhận lệnh)
    int logged_in;              // 0 = chưa đăng nhập, 1 = đã đăng nhập
    int account_index;          // Chỉ số tài khoản trong mảng accounts[]
    char username[50];          // Username tạm (trước khi xác thực)
    char current_dir[256];      // Thư mục làm việc hiện tại (VD: "/files")
    char root_dir[256];         // Thư mục gốc của user (VD: "./data/user1")
    struct sockaddr_in client_addr;  // Địa chỉ IP:Port của client
    int data_listen_sock;       // Socket lắng nghe data (PASV mode)
    int data_sock;              // Socket kết nối data hiện tại
} FTPSession;
```

**Mối quan hệ giữa các thành viên:**
```
Thư mục thực tế = root_dir + current_dir
VD: "./data/user1" + "/files" = "./data/user1/files"

Session cho user1:
├── root_dir: "./data/user1"     (không đổi sau login)
├── current_dir: "/"              (có thể thay đổi bằng CWD)
└── Đường dẫn thực: "./data/user1/"
```

---

### 3.2 Khởi Tạo Session - handle_client()

```c
// Dòng 491-503: Hàm xử lý chính cho mỗi client
void handle_client(int client_sock, struct sockaddr_in client_addr, int session_id) {
    // Khởi tạo phiên FTP
    FTPSession session;
    memset(&session, 0, sizeof(session));      // Xóa sạch tất cả fields -> 0
    session.session_id = session_id;
    session.ctrl_sock = client_sock;
    session.client_addr = client_addr;
    session.logged_in = 0;                     // Chưa đăng nhập
    session.data_listen_sock = -1;             // -1 = chưa có kết nối data
    session.data_sock = -1;
    
    // Gửi thông điệp chào mừng theo RFC 959
    send_response(client_sock, "220 FTP Server Ready\r\n");
```

**Giải thích trạng thái ban đầu:**
| Field | Giá trị | Ý nghĩa |
|-------|---------|---------|
| `logged_in` | 0 | Client chưa xác thực |
| `username` | "" | Chưa nhập username |
| `current_dir` | "" | Sẽ được set sau khi login |
| `root_dir` | "" | Sẽ được set sau khi login |
| `data_listen_sock` | -1 | Chưa có PASV socket |

---

### 3.3 Command Processing Loop

```c
// Dòng 505-577: Vòng lặp xử lý lệnh chính
char buffer[CMD_SIZE];  // Buffer 512 bytes
int running = 1;

while (running) {
    memset(buffer, 0, sizeof(buffer));
    
    // BLOCKING: Đợi lệnh từ client
    int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0) {
        break;  // Client đóng kết nối hoặc lỗi
    }
    
    // Xóa ký tự xuống dòng (\r\n)
    buffer[strcspn(buffer, "\r\n")] = 0;
    
    // Ghi log lệnh nhận được
    log_command(session.session_id, buffer, inet_ntoa(client_addr.sin_addr));
```

**Ví dụ xử lý buffer:**
```
Nhận được: "USER user1\r\n"
strcspn() tìm vị trí \r = 10
buffer[10] = 0
Kết quả: "USER user1\0..."
```

---

### 3.4 Tách Lệnh và Tham Số

```c
    // Dòng 523-527: Parse command
    char *cmd = strtok(buffer, " ");   // Tách phần đầu (lệnh)
    char *arg = strtok(NULL, "");      // Phần còn lại (tham số)
    
    if (cmd == NULL) continue;         // Bỏ qua dòng trống
```

**Ví dụ với `strtok()`:**
```
Input: "USER user1"
Lần 1: strtok(buffer, " ") → cmd = "USER", buffer trở thành "USER\0user1"
Lần 2: strtok(NULL, "") → arg = "user1"

Input: "PWD"
Lần 1: strtok(buffer, " ") → cmd = "PWD"
Lần 2: strtok(NULL, "") → arg = NULL
```

---

### 3.5 Command Router

```c
    // Dòng 529-576: Điều hướng lệnh tới handler tương ứng
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
        cmd_cwd(&session, "..");  // CDUP = CWD ..
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
        send_response(client_sock, "211 End\r\n");
    }
    else if (strcasecmp(cmd, "NOOP") == 0) {
        send_response(client_sock, "200 OK\r\n");
    }
    else if (strcasecmp(cmd, "QUIT") == 0) {
        cmd_quit(&session);
        running = 0;  // Thoát vòng lặp
    }
    else {
        send_response(client_sock, "502 Command not implemented\r\n");
    }
}
```

**`strcasecmp()` - So sánh không phân biệt hoa thường:**
- `strcasecmp("USER", "user")` → 0 (bằng nhau)
- `strcasecmp("USER", "PASS")` → khác 0

---

### 3.6 Cleanup - Đóng Kết Nối

```c
// Dòng 579-581: Dọn dẹp khi client ngắt kết nối
    close(client_sock);
}
```

**Khi nào vòng lặp kết thúc:**
1. Client gửi `QUIT` → `running = 0`
2. Client đóng kết nối → `recv()` trả về 0
3. Lỗi network → `recv()` trả về -1

---

## 4. Response Handling

### 4.1 Hàm send_response()

```c
// Dòng 20-22: Gửi phản hồi đến client
void send_response(int sock, const char *msg) {
    send(sock, msg, strlen(msg), 0);
}
```

**FTP Response Format (RFC 959):**
```
<3-digit code> <message>\r\n
```

**Các mã phản hồi phổ biến:**
| Code | Ý nghĩa |
|------|---------|
| 220 | Server ready |
| 221 | Goodbye |
| 226 | Transfer complete |
| 227 | Entering Passive Mode |
| 230 | User logged in |
| 250 | Directory changed |
| 257 | Current directory |
| 331 | Need password |
| 425 | Can't open data connection |
| 501 | Syntax error |
| 502 | Command not implemented |
| 530 | Not logged in |
| 550 | File/directory not found |

---

## 5. Flow Diagram Tổng Hợp

### 5.1 Server Startup Flow

```
┌──────────────────────────────────────────────────────────────┐
│                         SERVER STARTUP                        │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  1. Parse Arguments                                          │
│     └─> port = argv[1] hoặc 2121                            │
│                                                              │
│  2. init_logger()                                            │
│     └─> Mở file log                                          │
│                                                              │
│  3. load_accounts()                                          │
│     └─> Đọc accounts.txt → mảng accounts[]                   │
│                                                              │
│  4. socket(AF_INET, SOCK_STREAM, 0)                         │
│     └─> Tạo socket TCP                                       │
│                                                              │
│  5. setsockopt(SO_REUSEADDR)                                │
│     └─> Cho phép reuse port                                  │
│                                                              │
│  6. bind(server_sock, addr, port)                           │
│     └─> Gán địa chỉ 0.0.0.0:2121                            │
│                                                              │
│  7. listen(server_sock, 10)                                 │
│     └─> Bắt đầu lắng nghe, backlog = 10                     │
│                                                              │
│  8. Main Accept Loop (while true)                           │
│     └─> Đợi và xử lý kết nối mới                            │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

### 5.2 Client Connection Flow

```
┌──────────────────────────────────────────────────────────────┐
│                    CLIENT CONNECTION FLOW                     │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  Main Thread                    Worker Thread                │
│  ───────────                    ─────────────                │
│      │                                                       │
│      ▼                                                       │
│  accept() ◄─── [blocking] ────── Client connects            │
│      │                                                       │
│      ▼                                                       │
│  get_next_session_id()                                       │
│      │ SID = 1                                               │
│      ▼                                                       │
│  malloc(ClientInfo)                                          │
│      │                                                       │
│      ▼                                                       │
│  pthread_create() ─────────────────►  client_thread()       │
│      │                                      │                │
│      ▼                                      ▼                │
│  pthread_detach()                  handle_client()           │
│      │                                      │                │
│      ▼                                      ▼                │
│  Loop back to                      "220 FTP Ready"           │
│  accept() ◄───────                          │                │
│                                             ▼                │
│                                    Command Loop              │
│                                    ┌────────────┐            │
│                                    │   recv()   │◄─── USER   │
│                                    │   parse    │             │
│                                    │   handler  │             │
│                                    │   send()   │───► 331    │
│                                    └─────┬──────┘            │
│                                          │ QUIT              │
│                                          ▼                   │
│                                    close(sock)               │
│                                    free(info)                │
│                                    Thread exits              │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

### 5.3 Command Processing Flow

```
┌──────────────────────────────────────────────────────────────┐
│                   COMMAND PROCESSING FLOW                     │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  Client                    Server (Thread)                   │
│  ──────                    ───────────────                   │
│                                                              │
│  "USER user1\r\n" ──────────►  recv()                       │
│                                  │                           │
│                                  ▼                           │
│                              Parse: cmd="USER", arg="user1"  │
│                                  │                           │
│                                  ▼                           │
│                              cmd_user(&session, "user1")     │
│                                  │                           │
│                                  ▼                           │
│                              session.username = "user1"      │
│                              session.logged_in = 0           │
│                                  │                           │
│  "331 Need password" ◄──────────┘                           │
│                                                              │
│  "PASS 123456\r\n" ─────────►  recv()                       │
│                                  │                           │
│                                  ▼                           │
│                              cmd_pass(&session, "123456")    │
│                                  │                           │
│                                  ▼                           │
│                              check_login("user1", "123456")  │
│                                  │                           │
│                                  ▼ (idx >= 0: thành công)    │
│                              session.logged_in = 1           │
│                              session.root_dir = "./data/user1"│
│                              session.current_dir = "/"       │
│                                  │                           │
│  "230 User logged in" ◄─────────┘                           │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 6. Chi Tiết Các Command Handler

### 6.1 cmd_user() - Nhận Username

```c
// Dòng 27-36
void cmd_user(FTPSession *session, const char *arg) {
    // Kiểm tra có tham số không
    if (arg == NULL || strlen(arg) == 0) {
        send_response(session->ctrl_sock, "501 Syntax error\r\n");
        return;
    }
    
    // Lưu username tạm thời
    strncpy(session->username, arg, sizeof(session->username) - 1);
    session->logged_in = 0;  // Chưa xác thực
    
    send_response(session->ctrl_sock, "331 Username OK, need password\r\n");
}
```

**Flow:**
```
Input: "USER user1"
  │
  ├─ arg == NULL? → NO
  ├─ strlen("user1") == 0? → NO
  │
  ▼
session->username = "user1"
session->logged_in = 0
  │
  ▼
Output: "331 Username OK, need password\r\n"
```

---

### 6.2 cmd_pass() - Xác Thực Password

```c
// Dòng 41-60
void cmd_pass(FTPSession *session, const char *arg) {
    // Kiểm tra đã nhập USER chưa
    if (strlen(session->username) == 0) {
        send_response(session->ctrl_sock, "503 Login with USER first\r\n");
        return;
    }
    
    // Gọi hàm kiểm tra đăng nhập
    int idx = check_login(session->username, arg);
    
    if (idx >= 0) {
        // Đăng nhập thành công
        session->logged_in = 1;
        session->account_index = idx;
        
        // Thiết lập thư mục gốc và thư mục hiện tại
        strncpy(session->root_dir, accounts[idx].home_dir, sizeof(session->root_dir) - 1);
        strncpy(session->current_dir, "/", sizeof(session->current_dir) - 1);
        
        send_response(session->ctrl_sock, "230 User logged in\r\n");
    } else {
        // Đăng nhập thất bại
        send_response(session->ctrl_sock, "530 Login incorrect\r\n");
    }
}
```

**Flow thành công:**
```
accounts[] = [
  {username: "user1", password: "123456", home_dir: "./data/user1"},
  {username: "admin", password: "admin123", home_dir: "./data/admin"}
]

Input: "PASS 123456" (với session->username = "user1")
  │
  ▼
check_login("user1", "123456")
  │ Duyệt mảng accounts[]
  │ So sánh username và password
  ▼
idx = 0 (tìm thấy ở vị trí 0)
  │
  ▼
session->logged_in = 1
session->account_index = 0
session->root_dir = "./data/user1"
session->current_dir = "/"
  │
  ▼
Output: "230 User logged in\r\n"
```

---

### 6.3 cmd_pwd() - In Thư Mục Hiện Tại

```c
// Dòng 65-75
void cmd_pwd(FTPSession *session) {
    // Kiểm tra đã đăng nhập chưa
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

**Flow:**
```
session->current_dir = "/files"
  │
  ▼
snprintf() → "257 \"/files\" is current directory\r\n"
  │
  ▼
send() → Client
```

---

### 6.4 cmd_cwd() - Thay Đổi Thư Mục

```c
// Dòng 80-133
void cmd_cwd(FTPSession *session, const char *arg) {
    // Kiểm tra đăng nhập
    if (!session->logged_in) {
        send_response(session->ctrl_sock, "530 Not logged in\r\n");
        return;
    }
    
    // Kiểm tra tham số
    if (arg == NULL || strlen(arg) == 0) {
        send_response(session->ctrl_sock, "501 Syntax error\r\n");
        return;
    }
    
    // KHÔNG cho phép đi lên thư mục cha khi đang ở root
    if (strcmp(arg, "..") == 0 && strcmp(session->current_dir, "/") == 0) {
        send_response(session->ctrl_sock, "550 Permission denied\r\n");
        return;
    }
    
    char new_path[MAX_PATH_LEN];
    char full_path[MAX_PATH_LEN];
    
    // Xử lý đường dẫn tuyệt đối hoặc tương đối
    if (arg[0] == '/') {
        // Đường dẫn tuyệt đối: /files
        strncpy(new_path, arg, sizeof(new_path) - 1);
    } else if (strcmp(arg, "..") == 0) {
        // Đi lên thư mục cha
        strncpy(new_path, session->current_dir, sizeof(new_path) - 1);
        char *last_slash = strrchr(new_path, '/');
        if (last_slash != NULL && last_slash != new_path) {
            *last_slash = '\0';  // Xóa phần cuối
        } else {
            strcpy(new_path, "/");
        }
    } else {
        // Đường dẫn tương đối: files
        if (strcmp(session->current_dir, "/") == 0) {
            snprintf(new_path, sizeof(new_path), "/%s", arg);
        } else {
            snprintf(new_path, sizeof(new_path), "%s/%s", session->current_dir, arg);
        }
    }
    
    // Tạo đường dẫn thực tế
    snprintf(full_path, sizeof(full_path), "%s%s", session->root_dir, new_path);
    
    // Kiểm tra thư mục tồn tại
    struct stat st;
    if (stat(full_path, &st) == 0 && S_ISDIR(st.st_mode)) {
        strncpy(session->current_dir, new_path, sizeof(session->current_dir) - 1);
        send_response(session->ctrl_sock, "250 Directory changed\r\n");
    } else {
        send_response(session->ctrl_sock, "550 Directory not found\r\n");
    }
}
```

**Ví dụ các trường hợp:**

```
Case 1: CWD files (từ /)
─────────────────────────
current_dir = "/"
arg = "files"
  │
  ▼
new_path = "/" + "files" = "/files"
full_path = "./data/user1" + "/files" = "./data/user1/files"
  │
  ▼
stat() kiểm tra → tồn tại
  │
  ▼
current_dir = "/files"
Output: "250 Directory changed\r\n"

Case 2: CWD .. (từ /files)
──────────────────────────
current_dir = "/files"
arg = ".."
  │
  ▼
new_path = "/files"
strrchr() tìm '/' cuối → vị trí 0
last_slash != new_path? → FALSE (cùng vị trí 0)
new_path = "/"
  │
  ▼
current_dir = "/"
Output: "250 Directory changed\r\n"

Case 3: CWD .. (từ /) - Bị chặn
───────────────────────────────
current_dir = "/"
arg = ".."
  │
  ▼
strcmp(arg, "..") == 0 && strcmp(current_dir, "/") == 0 → TRUE
  │
  ▼
Output: "550 Permission denied\r\n"
```

---

### 6.5 cmd_quit() - Ngắt Kết Nối

```c
// Dòng 483-485
void cmd_quit(FTPSession *session) {
    send_response(session->ctrl_sock, "221 Goodbye\r\n");
}
```

**Sau khi gọi cmd_quit():**
```c
// Trong handle_client()
else if (strcasecmp(cmd, "QUIT") == 0) {
    cmd_quit(&session);
    running = 0;  // ← Đặt flag để thoát vòng lặp
}
```

---

## 7. Tổng Kết

### 7.1 Các File Liên Quan

| File | Chức năng |
|------|-----------|
| `server/src/server.c` | Main entry point, socket setup, multi-threading |
| `server/src/ftp_server.c` | Session management, command handlers |
| `server/include/ftp_server.h` | FTPSession structure, function declarations |

### 7.2 Các Hàm Quan Trọng

| Hàm | Mục đích |
|-----|----------|
| `main()` | Khởi động server, tạo socket, accept loop |
| `client_thread()` | Thread wrapper cho mỗi client |
| `handle_client()` | Vòng lặp xử lý lệnh chính |
| `send_response()` | Gửi FTP response đến client |
| `cmd_*()` | Xử lý các lệnh FTP cụ thể |

### 7.3 Luồng Dữ Liệu

```
Client ──TCP──► Server Socket (port 2121)
                    │
                    ▼
               accept() → New Socket
                    │
                    ▼
               pthread_create() → Worker Thread
                    │
                    ▼
               FTPSession (per-client state)
                    │
                    ├──► Control Channel: Commands/Responses
                    │
                    └──► Data Channel: File transfers (PASV mode)
```

---

## 8. Sequence Diagram

```
┌─────────┐                           ┌─────────────┐
│ Client  │                           │   Server    │
└────┬────┘                           └──────┬──────┘
     │                                       │
     │ ─────── TCP Connect ────────────────► │
     │                                       │ accept()
     │                                       │ pthread_create()
     │ ◄────── 220 FTP Server Ready ─────── │
     │                                       │
     │ ─────── USER user1 ─────────────────► │
     │ ◄────── 331 Need password ────────── │
     │                                       │
     │ ─────── PASS 123456 ────────────────► │
     │ ◄────── 230 User logged in ────────── │
     │                                       │
     │ ─────── PWD ────────────────────────► │
     │ ◄────── 257 "/" is current dir ────── │
     │                                       │
     │ ─────── CWD files ──────────────────► │
     │ ◄────── 250 Directory changed ─────── │
     │                                       │
     │ ─────── QUIT ───────────────────────► │
     │ ◄────── 221 Goodbye ────────────────── │
     │                                       │ close()
     │                                       │ thread exit
     ▼                                       ▼
```

---

> **Ghi chú:** Script này tập trung vào phần core server, socket handling, multi-client và control connection. Các phần khác như Data Connection (PASV/RETR/STOR) và Account Management được mô tả trong các script khác.
