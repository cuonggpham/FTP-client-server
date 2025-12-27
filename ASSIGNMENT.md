# Phân Công Công Việc - Dự Án FTP Client-Server

---

## Phạm Quốc Cường – Core Server & Network 

### Phần chịu trách nhiệm

- Khởi tạo FTP Server
- Socket TCP (listen / accept)
- Xử lý đa client (thread hoặc fork)
- Control connection
- Tương thích FileZilla (RFC 959 cơ bản)

### Chức năng cụ thể

**Lệnh FTP cơ bản:**
- `USER`, `PASS`
- `PWD`
- `CWD`
- `QUIT`

**Gửi mã phản hồi chuẩn FTP:**
- `220`, `331`, `230`, `530`, `257`, `250`, `221`…

**Hiển thị log server:**
```
14:32:10 USER <192.168.1.5>
```

### File/Module đề xuất

```
server/
 ├── main.c
 ├── ftp_server.c
 ├── ftp_server.h
 ├── session.c
 └── session.h
```

> **Vai trò:** "Xương sống" hệ thống

---

## Hồ Tuấn Huy - Account, File System & Security

### Phần chịu trách nhiệm

- Quản lý tài khoản FTP
- Làm việc với file system
- Bảo mật thư mục gốc (chroot giả)

### Chức năng cụ thể

**File accounts.txt:**
```
username|password|/home/ftp/user1
```

**Chức năng:**
- Thêm tài khoản mới
- Load tài khoản khi server start
- Kiểm tra đăng nhập

**Xử lý thư mục:**
- `LIST`
- `CWD`
- `PWD`

**Đảm bảo:**
- Client không thoát khỏi thư mục gốc
- Dùng `realpath()` để kiểm soát

### File/Module đề xuất

```
server/
 ├── account.c
 ├── account.h
 ├── filesystem.c
 └── filesystem.h
```

> **Vai trò:** "An toàn & Dữ liệu"

---

## Hà Trung Chiến - Data Connection & Client App

### Phần chịu trách nhiệm

- Data connection (PASV)
- Upload / Download file
- Viết FTP Client

### Phần SERVER

**Lệnh:**
- `PASV`
- `LIST`
- `RETR` (download)
- `STOR` (upload)

**Chức năng:**
- Tạo socket data riêng
- Truyền file block-wise

### Phần CLIENT

**Nhập lệnh từ bàn phím:**
- Gửi lệnh FTP chuẩn

**Hiển thị phản hồi:**
```
14:35:20 LIST 150 Opening data connection
```

**Hỗ trợ:**
- `pwd`
- `cd`
- `ls`
- `get file`
- `put file`
- `quit`

### File/Module đề xuất

```
client/
 ├── ftp_client.c
 ├── ftp_client.h
 ├── command_parser.c
 └── command_parser.h
```

> **Vai trò:** "Truyền dữ liệu & Giao diện người dùng"

---

## Bảng Phân Công Tổng Hợp

| Thành viên | Phần phụ trách |
|------------|----------------|
| **Cuong** | FTP Server core, socket, đa client, control connection |
| **Huy** | Quản lý tài khoản, file system, bảo mật thư mục |
| **Chien** | Data connection, upload/download, FTP Client |