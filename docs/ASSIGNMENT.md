# Phân Công Công Việc - Dự Án FTP Client-Server

---

## Phạm Quốc Cường – Thiết Kế Kiến Trúc & Core Server

### Phần chịu trách nhiệm

- Thiết kế kiến trúc FTP Server
- Xử lý socket TCP (listen/accept)
- Hỗ trợ đa client (pthread)
- Control connection
- Tiếp nhận & phân tích lệnh FTP
- Log server

### Chức năng đã triển khai

**File `server/src/server.c` - Main Entry Point:**
- Khởi tạo socket TCP và bind port (mặc định 2121)
- Vòng lặp chính `accept()` kết nối client
- Tạo thread mới cho mỗi client (`pthread_create`)
- Quản lý session ID duy nhất cho mỗi kết nối
- Gọi `handle_client()` trong mỗi thread
- Gọi `load_accounts()` để nạp tài khoản khi khởi động

**File `server/src/ftp_server.c` - FTP Protocol Handler:**
- `handle_client()` - Vòng lặp chính xử lý lệnh từ client
- `send_response()` - Gửi phản hồi FTP chuẩn
- `cmd_user()` - Xử lý lệnh USER
- `cmd_pass()` - Xử lý lệnh PASS (gọi `check_login()`)
- `cmd_type()` - Xử lý lệnh TYPE
- `cmd_syst()` - Xử lý lệnh SYST
- `cmd_quit()` - Xử lý lệnh QUIT

**File `server/helpers/logger.c` - Logging System:**
- `init_logger()` - Khởi tạo file log theo ngày
- `log_info()` - Ghi log thông tin
- `log_error()` - Ghi log lỗi
- `log_command()` - Ghi log lệnh FTP với session ID và IP client
- `close_logger()` - Đóng file log
- Log được lưu vào `./server/logs/server_YYYY-MM-DD.log`

### Cấu trúc File

```
server/
 ├── src/
 │   ├── server.c          # Main entry, socket, threading
 │   └── ftp_server.c      # Xử lý lệnh FTP (phần core)
 ├── include/
 │   └── ftp_server.h      # Header định nghĩa FTPSession
 └── helpers/
     ├── logger.c          # Hệ thống logging
     └── logger.h
```

> **Vai trò:** "Kiến trúc sư & Xương sống hệ thống"

---

## Hồ Tuấn Huy – Account, File System & Server Data Connection

### Phần chịu trách nhiệm

- Quản lý tài khoản FTP (USER/PASS)
- Lưu & đọc file account
- Xử lý file system phía server (LIST, CWD, PWD)
- Bảo mật & giới hạn thư mục
- **Data Connection phía Server (PASV mode)**
- **Xử lý RETR/STOR phía Server (gửi/nhận file)**

### Chức năng đã triển khai

**File `server/src/account.c` - Account Management:**
- `load_accounts()` - Đọc file `accounts.txt` khi server khởi động
  - Format: `username password home_dir`
  - Lưu vào mảng `accounts[]`
- `check_login()` - Kiểm tra thông tin đăng nhập
  - So sánh username/password
  - Trả về index tài khoản nếu đúng, -1 nếu sai
- `add_account()` - Thêm tài khoản mới
  - Kiểm tra trùng username
  - Giới hạn số tài khoản tối đa
- `save_accounts()` - Lưu danh sách tài khoản ra file

**File `server/src/account_add.c` - Add Account Tool:**
- Công cụ CLI để thêm tài khoản mới
- Tạo thư mục home cho user mới

**File System & Security (trong `ftp_server.c`):**
- `cmd_pwd()` - Xử lý lệnh PWD
- `cmd_cwd()` - Xử lý lệnh CWD (kiểm tra `realpath`)
- `cmd_list()` - Xử lý lệnh LIST (gửi qua data connection)
- Sử dụng `realpath()` để kiểm tra đường dẫn thực
- Chặn client thoát khỏi thư mục gốc (chroot giả)
- Mỗi user có thư mục riêng (`home_dir`)

**Data Connection phía Server (trong `ftp_server.c`):**
- `cmd_pasv()` - Xử lý lệnh PASV (tạo data socket, trả về IP:port)
- `accept_data_connection()` - Chấp nhận kết nối data từ client
- `cmd_retr()` - Xử lý lệnh RETR (gửi file cho client download)
- `cmd_stor()` - Xử lý lệnh STOR (nhận file từ client upload)

**File `server/data/accounts.txt`:**
```
user1 123456 ./server/data/user1
user2 password ./server/data/user2
```

### Cấu trúc File

```
server/
 ├── src/
 │   ├── account.c         # Quản lý tài khoản
 │   ├── account_add.c     # Tool thêm tài khoản
 │   └── ftp_server.c      # Phần PWD, CWD, LIST, PASV, RETR, STOR
 ├── include/
 │   ├── account.h         # Header định nghĩa Account
 │   └── ftp_server.h      # FTPSession structure
 └── data/
     ├── accounts.txt      # File lưu tài khoản
     ├── user1/            # Thư mục home user1
     └── user2/            # Thư mục home user2
```

> **Vai trò:** "An toàn, Dữ liệu & Truyền tải phía Server"

---

## Hà Trung Chiến – FTP Client Application

### Phần chịu trách nhiệm

- Xây dựng FTP Client hoàn chỉnh
- Kết nối điều khiển (Control Connection)
- Kết nối dữ liệu phía Client (Data Connection)
- Giao diện người dùng (Command Line Interface)
- Xử lý log và hiển thị phản hồi phía client

### Chức năng đã triển khai

**File `client/src/ftp_client.c` - FTP Client Library:**
- `ftp_connect()` - Kết nối đến FTP server
- `ftp_disconnect()` - Ngắt kết nối
- `ftp_send_cmd()` - Gửi lệnh đến server
- `ftp_recv_response()` - Nhận phản hồi từ server
- `ftp_login()` - Đăng nhập (USER + PASS)
- `ftp_pwd()` - Lấy thư mục hiện tại
- `ftp_cwd()` - Đổi thư mục
- `parse_pasv_response()` - Phân tích IP:port từ PASV response
- `open_data_connection()` - Tạo kết nối data (passive mode)
- `ftp_list()` - Liệt kê file (PASV + LIST)
- `ftp_retr()` - Download file (PASV + RETR)
- `ftp_stor()` - Upload file (PASV + STOR)
- `ftp_quit()` - Thoát
- `log_response()` - Ghi log phản hồi theo format `hh:mm:ss <Lệnh> <Phản hồi>`

**File `client/src/client.c` - Main Client Entry:**
- Nhận host/port từ tham số dòng lệnh
- Vòng lặp nhập lệnh từ bàn phím
- Hỗ trợ các lệnh tắt:
  - `PWD`, `CWD/CD`, `LIST/LS`
  - `RETR/GET`, `STOR/PUT`
  - `QUIT/EXIT`, `HELP`
- Hiển thị prompt `ftp>`

### Cấu trúc File

```
client/
 ├── src/
 │   ├── client.c          # Main entry, command loop
 │   └── ftp_client.c      # FTP protocol implementation
 └── include/
     └── ftp_client.h      # Header định nghĩa FTPClient
```

> **Vai trò:** "FTP Client & Giao diện người dùng"

---

## Bảng Phân Công Tổng Hợp

| Thành viên | Phần phụ trách |
|-----------|---------------|
| **Cuong (Leader)** | Thiết kế kiến trúc FTP Server, xử lý socket, đa client, control connection, log server |
| **Huy** | Quản lý tài khoản, file system (LIST, CWD, PWD), bảo mật thư mục, **PASV/RETR/STOR phía server** |
| **Chien** | Xây dựng **toàn bộ FTP Client** (kết nối, đăng nhập, truyền file, giao diện CLI) |

---

## Tổng Quan Cấu Trúc Dự Án

```
FTP-client-server/
├── server/
│   ├── src/
│   │   ├── server.c         # [Cuong] Main, socket, threading
│   │   ├── ftp_server.c     # [Cuong] Core commands + [Huy] File/Data commands
│   │   ├── account.c        # [Huy] Account management
│   │   └── account_add.c    # [Huy] Add account tool
│   ├── include/
│   │   ├── ftp_server.h     # [Cuong + Huy]
│   │   └── account.h        # [Huy]
│   ├── helpers/
│   │   ├── logger.c         # [Cuong] Logging system
│   │   └── logger.h
│   ├── data/
│   │   ├── accounts.txt     # [Huy] Account storage
│   │   └── user*/           # [Huy] User home directories
│   └── logs/                # [Cuong] Server logs
├── client/
│   ├── src/
│   │   ├── client.c         # [Chien] Main entry
│   │   └── ftp_client.c     # [Chien] FTP client library
│   └── include/
│       └── ftp_client.h     # [Chien]
├── Makefile
├── README.md
├── TASK.md
├── ASSIGNMENT.md
└── REPORT.md
```


