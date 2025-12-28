# Script Chi Tiết: FTP Client Application

> **Người thực hiện:** Chiến  
> **Phần phụ trách:** Xây dựng toàn bộ FTP Client (kết nối, đăng nhập, truyền file, giao diện CLI)

---

## Tổng Quan

Script này mô tả flow xử lý của FTP Client:
1. **FTPClient Structure** - Cấu trúc client
2. **Control Connection** - Kết nối điều khiển
3. **Data Connection (Client-side)** - Kết nối dữ liệu passive mode
4. **File Transfer** - Download/Upload từ phía client
5. **CLI Application** - Giao diện dòng lệnh

---

## 1. FTPClient Structure

### File: `client/include/ftp_client.h`

```c
// Dòng 4-5: Constants
#define BUFFER_SIZE 4096    // Buffer cho truyền dữ liệu
#define CMD_SIZE 256        // Kích thước lệnh

// Dòng 7-13: Cấu trúc FTPClient
typedef struct {
    int ctrl_sock;      // Socket điều khiển (port 2121)
    char server_ip[50]; // IP server
    int server_port;    // Port server
    int logged_in;      // Trạng thái đăng nhập
} FTPClient;
```

**Mô hình 2 kết nối:**
```
┌─────────┐                          ┌─────────────┐
│ Client  │                          │   Server    │
├─────────┤                          ├─────────────┤
│ctrl_sock│◄──── Control (2121) ────►│  port 2121  │
│         │      Commands/Responses  │             │
│         │                          │             │
│data_sock│◄──── Data (PASV port) ──►│  port random│
│         │      File transfers      │             │
└─────────┘                          └─────────────┘
```

---

## 2. Control Connection

### File: `client/src/ftp_client.c`

### 2.1 ftp_connect() - Kết nối đến Server

```c
// Dòng 32-69: Thiết lập kết nối TCP
int ftp_connect(FTPClient *client, const char *host, int port) {
    // Tạo socket TCP
    client->ctrl_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client->ctrl_sock < 0) {
        perror("Cannot create socket");
        return -1;
    }
    
    // Thiết lập địa chỉ server
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    // Chuyển IP string thành binary
    if (inet_pton(AF_INET, host, &server_addr.sin_addr) <= 0) {
        perror("Invalid IP address");
        close(client->ctrl_sock);
        return -1;
    }
    
    // Kết nối đến server
    if (connect(client->ctrl_sock, (struct sockaddr*)&server_addr, 
                sizeof(server_addr)) < 0) {
        perror("Cannot connect to server");
        close(client->ctrl_sock);
        return -1;
    }
    
    // Lưu thông tin kết nối
    strncpy(client->server_ip, host, sizeof(client->server_ip) - 1);
    client->server_port = port;
    client->logged_in = 0;
    
    // Nhận thông điệp chào mừng
    char buffer[BUFFER_SIZE];
    ftp_recv_response(client, buffer, sizeof(buffer));
    log_response("CONNECT", buffer);  // "220 FTP Server Ready"
    
    return 0;
}
```

**Flow kết nối:**
```
1. socket()    → Tạo socket descriptor
2. inet_pton() → Chuyển "127.0.0.1" thành binary
3. connect()   → TCP 3-way handshake
4. recv()      → Nhận "220 FTP Server Ready"
```

### 2.2 ftp_send_cmd() & ftp_recv_response()

```c
// Dòng 85-89: Gửi lệnh FTP
int ftp_send_cmd(FTPClient *client, const char *cmd) {
    char buffer[CMD_SIZE];
    snprintf(buffer, sizeof(buffer), "%s\r\n", cmd);  // Thêm CRLF
    return send(client->ctrl_sock, buffer, strlen(buffer), 0);
}

// Dòng 94-101: Nhận phản hồi
int ftp_recv_response(FTPClient *client, char *buffer, int size) {
    memset(buffer, 0, size);
    int bytes = recv(client->ctrl_sock, buffer, size - 1, 0);
    if (bytes > 0) {
        buffer[bytes] = '\0';
    }
    return bytes;
}
```

**Ví dụ:**
```
ftp_send_cmd(client, "USER user1")
→ Gửi: "USER user1\r\n"
→ Nhận: "331 Username OK, need password\r\n"
```

### 2.3 ftp_login() - Đăng nhập

```c
// Dòng 106-128: USER + PASS
int ftp_login(FTPClient *client, const char *user, const char *pass) {
    char cmd[CMD_SIZE];
    char response[BUFFER_SIZE];
    
    // Gửi USER
    snprintf(cmd, sizeof(cmd), "USER %s", user);
    ftp_send_cmd(client, cmd);
    ftp_recv_response(client, response, sizeof(response));
    log_response(cmd, response);  // "331 Need password"
    
    // Gửi PASS
    snprintf(cmd, sizeof(cmd), "PASS %s", pass);
    ftp_send_cmd(client, cmd);
    ftp_recv_response(client, response, sizeof(response));
    log_response("PASS ****", response);  // Ẩn password trong log
    
    // Kiểm tra kết quả
    if (strncmp(response, "230", 3) == 0) {
        client->logged_in = 1;
        return 0;  // Thành công
    }
    return -1;  // Thất bại
}
```

---

## 3. Data Connection (Client-side)

### 3.1 parse_pasv_response() - Đọc IP:port từ PASV

```c
// Dòng 162-177: Parse "227 Entering Passive Mode (h1,h2,h3,h4,p1,p2)"
int parse_pasv_response(const char *response, char *ip, int *port) {
    int h1, h2, h3, h4, p1, p2;
    
    // Tìm dấu ngoặc mở
    const char *start = strchr(response, '(');
    if (start == NULL) return -1;
    
    // Đọc 6 số
    if (sscanf(start, "(%d,%d,%d,%d,%d,%d)", 
               &h1, &h2, &h3, &h4, &p1, &p2) != 6) {
        return -1;
    }
    
    // Ghép thành IP và port
    sprintf(ip, "%d.%d.%d.%d", h1, h2, h3, h4);
    *port = p1 * 256 + p2;  // High byte * 256 + Low byte
    
    return 0;
}
```

**Ví dụ:**
```
Input:  "227 Entering Passive Mode (127,0,0,1,78,52)\r\n"
Output: ip = "127.0.0.1"
        port = 78 * 256 + 52 = 20020
```

### 3.2 open_data_connection() - Mở kết nối data

```c
// Dòng 182-214: PASV + connect data port
int open_data_connection(FTPClient *client) {
    char response[BUFFER_SIZE];
    
    // 1. Gửi PASV
    ftp_send_cmd(client, "PASV");
    ftp_recv_response(client, response, sizeof(response));
    log_response("PASV", response);
    
    // 2. Parse IP:port
    char data_ip[50];
    int data_port;
    if (parse_pasv_response(response, data_ip, &data_port) < 0) {
        printf("Error parsing PASV response\n");
        return -1;
    }
    
    // 3. Tạo socket mới cho data
    int data_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (data_sock < 0) return -1;
    
    // 4. Kết nối đến data port
    struct sockaddr_in data_addr;
    memset(&data_addr, 0, sizeof(data_addr));
    data_addr.sin_family = AF_INET;
    data_addr.sin_port = htons(data_port);
    inet_pton(AF_INET, data_ip, &data_addr.sin_addr);
    
    if (connect(data_sock, (struct sockaddr*)&data_addr, sizeof(data_addr)) < 0) {
        close(data_sock);
        return -1;
    }
    
    return data_sock;  // Trả về socket để truyền dữ liệu
}
```

---

## 4. File Transfer Commands

### 4.1 ftp_list() - Liệt kê file

```c
// Dòng 219-251: PASV + LIST
int ftp_list(FTPClient *client) {
    char response[BUFFER_SIZE];
    
    // Mở data connection trước
    int data_sock = open_data_connection(client);
    if (data_sock < 0) {
        printf("Cannot open data connection\n");
        return -1;
    }
    
    // Gửi LIST qua control connection
    ftp_send_cmd(client, "LIST");
    ftp_recv_response(client, response, sizeof(response));
    log_response("LIST", response);  // "150 Opening..."
    
    // Nhận listing qua data connection
    printf("\n--- File listing ---\n");
    char buffer[BUFFER_SIZE];
    int bytes;
    while ((bytes = recv(data_sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes] = '\0';
        printf("%s", buffer);
    }
    printf("--------------------\n");
    
    close(data_sock);
    
    // Nhận response cuối
    ftp_recv_response(client, response, sizeof(response));
    log_response("LIST", response);  // "226 Transfer complete"
    
    return 0;
}
```

### 4.2 ftp_retr() - Download file

```c
// Dòng 256-310: PASV + TYPE I + RETR
int ftp_retr(FTPClient *client, const char *filename, const char *local_path) {
    char cmd[CMD_SIZE];
    char response[BUFFER_SIZE];
    
    // 1. Mở data connection
    int data_sock = open_data_connection(client);
    if (data_sock < 0) return -1;
    
    // 2. Đặt chế độ binary
    ftp_send_cmd(client, "TYPE I");
    ftp_recv_response(client, response, sizeof(response));
    
    // 3. Gửi RETR
    snprintf(cmd, sizeof(cmd), "RETR %s", filename);
    ftp_send_cmd(client, cmd);
    ftp_recv_response(client, response, sizeof(response));
    log_response(cmd, response);
    
    // Kiểm tra 150/125
    if (strncmp(response, "150", 3) != 0 && strncmp(response, "125", 3) != 0) {
        close(data_sock);
        return -1;
    }
    
    // 4. Mở file local để ghi
    const char *save_path = (local_path && strlen(local_path) > 0) 
                            ? local_path : filename;
    FILE *fp = fopen(save_path, "wb");
    if (fp == NULL) {
        close(data_sock);
        return -1;
    }
    
    // 5. Nhận và ghi file
    char buffer[BUFFER_SIZE];
    int bytes;
    long total = 0;
    while ((bytes = recv(data_sock, buffer, sizeof(buffer), 0)) > 0) {
        fwrite(buffer, 1, bytes, fp);
        total += bytes;
    }
    
    fclose(fp);
    close(data_sock);
    printf("Downloaded %ld bytes -> %s\n", total, save_path);
    
    // 6. Nhận 226
    ftp_recv_response(client, response, sizeof(response));
    log_response(cmd, response);
    
    return 0;
}
```

### 4.3 ftp_stor() - Upload file

```c
// Dòng 315-374: PASV + TYPE I + STOR
int ftp_stor(FTPClient *client, const char *local_path, const char *remote_name) {
    char cmd[CMD_SIZE];
    char response[BUFFER_SIZE];
    
    // 1. Mở file local
    FILE *fp = fopen(local_path, "rb");
    if (fp == NULL) {
        perror("Cannot open local file");
        return -1;
    }
    
    // 2. Mở data connection
    int data_sock = open_data_connection(client);
    if (data_sock < 0) {
        fclose(fp);
        return -1;
    }
    
    // 3. Đặt chế độ binary
    ftp_send_cmd(client, "TYPE I");
    ftp_recv_response(client, response, sizeof(response));
    
    // 4. Lấy tên file (bỏ đường dẫn)
    const char *name = (remote_name && strlen(remote_name) > 0) 
                       ? remote_name : local_path;
    const char *base = strrchr(name, '/');
    if (base) name = base + 1;
    
    // 5. Gửi STOR
    snprintf(cmd, sizeof(cmd), "STOR %s", name);
    ftp_send_cmd(client, cmd);
    ftp_recv_response(client, response, sizeof(response));
    log_response(cmd, response);
    
    if (strncmp(response, "150", 3) != 0 && strncmp(response, "125", 3) != 0) {
        fclose(fp);
        close(data_sock);
        return -1;
    }
    
    // 6. Đọc và gửi file
    char buffer[BUFFER_SIZE];
    size_t bytes;
    long total = 0;
    while ((bytes = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        send(data_sock, buffer, bytes, 0);
        total += bytes;
    }
    
    fclose(fp);
    close(data_sock);
    printf("Uploaded %ld bytes\n", total);
    
    // 7. Nhận 226
    ftp_recv_response(client, response, sizeof(response));
    log_response(cmd, response);
    
    return 0;
}
```

---

## 5. CLI Application

### File: `client/src/client.c`

### 5.1 Main Entry

```c
// Dòng 26-66: Khởi động client
int main(int argc, char *argv[]) {
    // Đọc host/port từ tham số
    const char *host = (argc > 1) ? argv[1] : "127.0.0.1";
    int port = (argc > 2) ? atoi(argv[2]) : 2121;
    
    printf("=== FTP CLIENT ===\n");
    printf("Connecting to %s:%d...\n", host, port);
    
    // Khởi tạo client
    FTPClient client;
    memset(&client, 0, sizeof(client));
    
    if (ftp_connect(&client, host, port) < 0) {
        printf("Cannot connect to server!\n");
        return 1;
    }
    printf("Connection successful!\n\n");
    
    // Đăng nhập
    char username[50], password[50];
    printf("Username: ");
    fgets(username, sizeof(username), stdin);
    username[strcspn(username, "\r\n")] = 0;
    
    printf("Password: ");
    fgets(password, sizeof(password), stdin);
    password[strcspn(password, "\r\n")] = 0;
    
    if (ftp_login(&client, username, password) < 0) {
        printf("Login failed!\n");
        ftp_disconnect(&client);
        return 1;
    }
    
    printf("Login successful!\n");
    print_help();
    // ...
}
```

### 5.2 Command Loop

```c
// Dòng 72-128: Vòng lặp xử lý lệnh
char input[256];
int running = 1;

while (running) {
    printf("ftp> ");
    fflush(stdout);
    
    if (fgets(input, sizeof(input), stdin) == NULL) break;
    input[strcspn(input, "\r\n")] = 0;
    if (strlen(input) == 0) continue;
    
    // Tách lệnh và tham số
    char *cmd = strtok(input, " ");
    char *arg = strtok(NULL, "");
    
    // Xử lý từng lệnh
    if (strcasecmp(cmd, "PWD") == 0) {
        ftp_pwd(&client);
    }
    else if (strcasecmp(cmd, "CWD") == 0 || strcasecmp(cmd, "CD") == 0) {
        if (arg) ftp_cwd(&client, arg);
        else printf("Syntax: CWD <directory>\n");
    }
    else if (strcasecmp(cmd, "LIST") == 0 || strcasecmp(cmd, "LS") == 0) {
        ftp_list(&client);
    }
    else if (strcasecmp(cmd, "RETR") == 0 || strcasecmp(cmd, "GET") == 0) {
        if (arg) ftp_retr(&client, arg, NULL);
        else printf("Syntax: RETR <filename>\n");
    }
    else if (strcasecmp(cmd, "STOR") == 0 || strcasecmp(cmd, "PUT") == 0) {
        if (arg) ftp_stor(&client, arg, NULL);
        else printf("Syntax: STOR <local_filename>\n");
    }
    else if (strcasecmp(cmd, "QUIT") == 0 || strcasecmp(cmd, "EXIT") == 0) {
        ftp_quit(&client);
        running = 0;
    }
    else if (strcasecmp(cmd, "HELP") == 0) {
        print_help();
    }
    else {
        printf("Invalid command. Type HELP.\n");
    }
}
printf("Exited!\n");
```

### 5.3 log_response() - Ghi log với timestamp

```c
// Dòng 15-27: Format log
void log_response(const char *cmd, const char *response) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    
    // Xóa CRLF để hiển thị gọn
    char clean_resp[256];
    strncpy(clean_resp, response, sizeof(clean_resp) - 1);
    clean_resp[strcspn(clean_resp, "\r\n")] = 0;
    
    printf("%02d:%02d:%02d %s -> %s\n", 
           t->tm_hour, t->tm_min, t->tm_sec,
           cmd, clean_resp);
}
```

**Output mẫu:**
```
14:30:25 CONNECT -> 220 FTP Server Ready
14:30:26 USER user1 -> 331 Username OK, need password
14:30:27 PASS **** -> 230 User logged in
14:30:28 PWD -> 257 "/" is current directory
14:30:30 LIST -> 150 Opening data connection
14:30:30 LIST -> 226 Transfer complete
```

---

## 6. Download Flow Diagram

```
Client                              Server
  │                                    │
  │─── PASV ─────────────────────────► │
  │◄── 227 (...127,0,0,1,78,52) ───────│
  │                                    │
  │ parse: ip=127.0.0.1, port=20020    │
  │ socket() + connect(20020) ────────►│ accept()
  │                                    │
  │─── TYPE I ───────────────────────► │
  │◄── 200 Type set to I ──────────────│
  │                                    │
  │─── RETR file.txt ────────────────► │
  │◄── 150 Opening data connection ────│
  │                                    │
  │◄══════ FILE DATA ══════════════════│
  │        ↓ fwrite() to local file    │
  │                                    │
  │◄── 226 Transfer complete ──────────│
  │                                    │
```

---

## 7. Tổng Kết

### Files

| File | Chức năng |
|------|-----------|
| `client/src/client.c` | Main entry, command loop, CLI |
| `client/src/ftp_client.c` | FTP protocol library |
| `client/include/ftp_client.h` | FTPClient struct |

### Hàm Quan Trọng

| Hàm | Mục đích |
|-----|----------|
| `ftp_connect()` | Kết nối TCP đến server |
| `ftp_login()` | USER + PASS |
| `open_data_connection()` | PASV + connect data port |
| `parse_pasv_response()` | Parse IP:port từ 227 |
| `ftp_list()` | Liệt kê file |
| `ftp_retr()` | Download file |
| `ftp_stor()` | Upload file |
| `log_response()` | Log với timestamp |

### FTP Response Codes

| Code | Ý nghĩa |
|------|---------|
| 150 | Opening data connection |
| 226 | Transfer complete |
| 227 | Entering Passive Mode |
| 230 | User logged in |
| 331 | Need password |
| 425 | Can't open data connection |
| 550 | File not found |

---

> **Ghi chú:** Script này tập trung vào toàn bộ FTP Client: kết nối, đăng nhập, data connection (client-side), truyền file, và giao diện CLI.
