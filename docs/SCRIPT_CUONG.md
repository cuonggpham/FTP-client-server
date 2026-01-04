# Script Chi Tiết: FTP Server Core - Socket, Đa Client, Control Connection

> **Người thực hiện:** Cương  
> **Phần phụ trách:** FTP Server core, socket handling, multi-client architecture, control connection

---

# 📢 SCRIPT THUYẾT TRÌNH (Dạng lời nói)

> Phần này được viết dưới dạng **lời nói thuyết trình** để dễ dàng trình bày trước lớp/ban giám khảo.

---

## 🎤 MỞ ĐẦU (30 giây)

**[Slide 1: Giới thiệu]**

> Xin chào thầy/cô và các bạn. Em là **Cương**, hôm nay em sẽ trình bày về phần **FTP Server Core** mà em đã thực hiện trong đồ án này.
>
> Phần của em tập trung vào **3 nội dung chính**:
> 1. **Socket Server** - Cách tạo và cấu hình socket TCP
> 2. **Multi-Client** - Xử lý nhiều client đồng thời bằng đa luồng
> 3. **Control Connection** - Quản lý phiên làm việc và xử lý các lệnh FTP

---

## 🎤 PHẦN 1: SOCKET SERVER CORE (3-4 phút)

**[Slide 2: Cấu trúc tổng quan]**

> Đầu tiên, em xin trình bày về **cấu trúc Socket Server**.
>
> Server của chúng em được viết bằng ngôn ngữ **C**, chạy trên hệ điều hành **Linux**, sử dụng **POSIX Socket API** để giao tiếp mạng.

**[Slide 2.1: Mô hình TCP/IP - Giải thích chi tiết]**

> Trước khi đi vào code, em xin giải thích về **mô hình mạng TCP/IP** mà FTP sử dụng:
>
> **Mô hình 4 tầng:**
> ```
> ┌─────────────────────────────────────┐
> │  Tầng 4: Application (FTP, HTTP)   │  ← FTP protocol ở đây
> ├─────────────────────────────────────┤
> │  Tầng 3: Transport (TCP, UDP)      │  ← Socket API giao tiếp ở đây
> ├─────────────────────────────────────┤
> │  Tầng 2: Internet (IP)             │  ← Địa chỉ IP
> ├─────────────────────────────────────┤
> │  Tầng 1: Network Access (Ethernet) │  ← Card mạng
> └─────────────────────────────────────┘
> ```
>
> **Tại sao chọn TCP thay vì UDP?**
> - **TCP (Transmission Control Protocol):**
>   - Đảm bảo dữ liệu đến **đúng thứ tự**
>   - **Tin cậy** - nếu mất gói tin sẽ gửi lại
>   - Có **kiểm soát luồng** (flow control)
>   - Phù hợp cho FTP vì cần truyền file **chính xác 100%**
>
> - **UDP (User Datagram Protocol):**
>   - Nhanh hơn nhưng **không đảm bảo** thứ tự và độ tin cậy
>   - Phù hợp cho video streaming, game online

**[Slide 2.2: TCP 3-Way Handshake]**

> Khi client kết nối, TCP thực hiện **bắt tay 3 bước**:
>
> ```
> Client                              Server
>   │                                    │
>   │ ──── SYN (seq=100) ──────────────► │  Bước 1: Client gửi SYN
>   │                                    │
>   │ ◄─── SYN-ACK (seq=300, ack=101) ── │  Bước 2: Server trả lời SYN-ACK
>   │                                    │
>   │ ──── ACK (ack=301) ──────────────► │  Bước 3: Client xác nhận
>   │                                    │
>   │ ═══════ CONNECTION ESTABLISHED ═══ │
> ```
>
> Sau 3 bước này, kết nối TCP được thiết lập và có thể truyền dữ liệu 2 chiều.

**[Slide 2.3: Socket là gì?]**

> **Socket** là một điểm cuối (endpoint) của kết nối mạng, được định danh bởi:
> - **IP Address**: Xác định máy tính (ví dụ: `192.168.1.100`)
> - **Port Number**: Xác định ứng dụng trên máy đó (ví dụ: `2121`)
>
> Trong Linux/Unix, socket được xử lý như một **file descriptor** (số nguyên), cho phép dùng các hàm I/O như `read()`, `write()`, `close()`.
>
> ```
> Socket = IP Address + Port Number
> VD: 192.168.1.100:2121
> ```

**[Slide 3: Tạo Socket]**

> Để tạo một server socket, em sử dụng hàm `socket()` với các tham số:
> - `AF_INET`: Sử dụng giao thức **IPv4** (Address Family Internet)
>   - Còn có `AF_INET6` cho IPv6, `AF_UNIX` cho giao tiếp local
> - `SOCK_STREAM`: Sử dụng giao thức **TCP** - đảm bảo truyền tin cậy
>   - Còn có `SOCK_DGRAM` cho UDP
> - Tham số cuối là `0`: Sử dụng protocol mặc định (TCP cho SOCK_STREAM)
>
> **Kết quả:** Trả về một **file descriptor** (số nguyên dương) đại diện cho socket, hoặc **-1** nếu lỗi.

```c
int server_sock = socket(AF_INET, SOCK_STREAM, 0);
if (server_sock < 0) {
    perror("Cannot create socket");  // In lỗi chi tiết
    return -1;
}
// Thành công: server_sock = 3, 4, 5,... (số dương)
```

**[Slide 4: Socket Options]**

> Tiếp theo, em thiết lập option `SO_REUSEADDR`. Đây là một option rất quan trọng.
>
> **Vấn đề TIME_WAIT:**
> - Khi đóng kết nối TCP, socket không giải phóng ngay mà vào trạng thái `TIME_WAIT`
> - Trạng thái này kéo dài **2-4 phút** (gấp đôi Maximum Segment Lifetime)
> - Mục đích: Đảm bảo các gói tin trễ không ảnh hưởng kết nối mới
>
> **Nếu KHÔNG có `SO_REUSEADDR`:**
> ```
> $ ./server
> ^C  (Ctrl+C để dừng)
> $ ./server
> Error: Address already in use  ← Port vẫn đang trong TIME_WAIT!
> ```
>
> **Có `SO_REUSEADDR`:**
> ```
> $ ./server
> ^C
> $ ./server
> Server running on port 2121  ← OK, bind lại được ngay!
> ```

```c
int opt = 1;
setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
```

**[Slide 5: Cấu hình địa chỉ - sockaddr_in]**

> Cấu trúc `sockaddr_in` lưu địa chỉ socket:
>
> ```c
> struct sockaddr_in {
>     sa_family_t    sin_family;   // AF_INET (IPv4)
>     in_port_t      sin_port;     // Port number (network byte order)
>     struct in_addr sin_addr;     // IP address
> };
> ```
>
> **Thiết lập từng thành phần:**
> - `sin_family = AF_INET`: Dùng IPv4
> - `sin_addr.s_addr = INADDR_ANY (0.0.0.0)`: Lắng nghe trên TẤT CẢ interfaces
>   - Bao gồm: localhost (127.0.0.1), WiFi (192.168.1.x), Ethernet...
> - `sin_port = htons(2121)`: Port 2121

**[Slide 5.1: Byte Order - Tại sao cần htons()?]**

> Máy tính và mạng **lưu trữ số khác nhau**:
>
> | Kiểu | Tên gọi | Cách lưu số 2121 (0x0849) |
> |------|---------|---------------------------|
> | Little-Endian | Host byte order | `49 08` (byte thấp trước) |
> | Big-Endian | Network byte order | `08 49` (byte cao trước) |
>
> Intel/AMD dùng Little-Endian, nhưng chuẩn mạng là Big-Endian.
>
> **Các hàm chuyển đổi:**
> - `htons()`: Host TO Network Short (16-bit, dùng cho port)
> - `htonl()`: Host TO Network Long (32-bit, dùng cho IP)
> - `ntohs()`, `ntohl()`: Chiều ngược lại
>
> **Nếu quên dùng `htons()`:** Server sẽ lắng nghe sai port!
> ```
> 2121 = 0x0849
> Không có htons(): Máy gửi 0x4908 = 18696 ← Sai port!
> Có htons(): Máy gửi 0x0849 = 2121 ✓
> ```

**[Slide 5.2: Bind - Gán địa chỉ cho socket]**

> Hàm `bind()` **liên kết socket với địa chỉ IP:Port**:
>
> ```
> ┌─────────────────────┐
> │   Server Socket     │
> │   (chưa có địa chỉ) │
> └──────────┬──────────┘
>            │ bind(sock, 0.0.0.0:2121)
>            ▼
> ┌─────────────────────┐
> │   Server Socket     │
> │   0.0.0.0:2121      │  ← Giờ socket "sở hữu" port 2121
> └─────────────────────┘
> ```
>
> **Sau khi bind thành công:**
> - Socket được gán địa chỉ 0.0.0.0:2121
> - Không process nào khác có thể bind vào port 2121
> - Có thể kiểm tra: `netstat -tlnp | grep 2121`

**[Slide 6: Listen - Bắt đầu lắng nghe]**

> Hàm `listen()` chuyển socket sang trạng thái **LISTEN**:
>
> ```c
> listen(server_sock, 10);  // backlog = 10
> ```
>
> **Tham số backlog = 10:**
> - Đây là **hàng đợi kết nối** (connection queue)
> - Khi client gọi `connect()`, kernel đưa vào hàng đợi
> - Hàm `accept()` lấy kết nối ra khỏi hàng đợi
>
> ```
> Client A ─┐
> Client B ─┼─► ┌──────────────┐     ┌─────────────┐
> Client C ─┤   │  Backlog     │────►│   accept()  │
> ...       │   │  Queue (10)  │     │             │
> Client K ─┘   └──────────────┘     └─────────────┘
>               │← max 10 kết nối chờ →│
> ```
>
> **Nếu hàng đợi đầy (> 10):**
> - Client mới sẽ nhận được lỗi `ECONNREFUSED`
> - Hoặc TCP sẽ retry sau một khoảng thời gian

---

## 🎤 PHẦN 2: MULTI-CLIENT - ĐA LUỒNG (3-4 phút)

**[Slide 7: Tại sao cần đa luồng?]**

> Bây giờ em xin trình bày về cách xử lý **nhiều client đồng thời**.
>
> **Vấn đề với single-thread:**
> ```
> Client A kết nối → Server xử lý
> Client A tải file 100MB (mất 30 giây)
>    ↓
> Client B kết nối → PHẢI ĐỢI 30 giây! ← Không chấp nhận được
> ```
>
> **Giải pháp đa luồng:**
> ```
> Client A kết nối → Thread 1 xử lý A
> Client B kết nối → Thread 2 xử lý B (song song!)
> Client C kết nối → Thread 3 xử lý C (song song!)
> ```
>
> **Ưu điểm:**
> - Mỗi client có thread riêng, **không chờ đợi lẫn nhau**
> - Main thread chỉ lo việc accept kết nối mới
> - Tận dụng CPU đa nhân hiệu quả

**[Slide 8: Vòng lặp Accept]**

> Đây là vòng lặp chính của server:
>
> ```
> Main Thread
>      │
>      ▼
> ┌─────────────────────────────────────────────┐
> │  while (1) {                                │
> │      client_sock = accept(server_sock);     │ ◄── BLOCKING
> │      // Tạo thread mới cho client           │
> │      pthread_create(..., client_thread);    │
> │  }                                          │
> └─────────────────────────────────────────────┘
> ```
>
> **Đặc điểm của `accept()`:**
> 1. **Blocking**: Chương trình dừng lại và đợi cho đến khi có client
> 2. Trả về **socket MỚI** dành riêng cho client đó
> 3. **Socket gốc (`server_sock`)** vẫn tiếp tục lắng nghe
>
> ```
> server_sock (port 2121) ──accept()──► client_sock_1 (cho Client A)
>                         ──accept()──► client_sock_2 (cho Client B)
>                         ──accept()──► client_sock_3 (cho Client C)
> ```

**[Slide 9: Session ID - Thread-safe]**

> Mỗi client được gán một **Session ID duy nhất** để phân biệt trong log.
>
> **Vấn đề race condition:** Khi nhiều client kết nối cùng lúc, nếu nhiều thread đọc/ghi biến `next_session_id` đồng thời → 2 client có thể có **cùng ID**!
>
> **Giải pháp:** Sử dụng **mutex** để bảo vệ biến `next_session_id`:
> - Thread lock mutex → đọc và tăng giá trị → unlock mutex
> - Thread khác phải đợi cho đến khi mutex được unlock

```c
static int get_next_session_id(void) {
    pthread_mutex_lock(&session_id_mutex);    // Khóa
    int id = ++next_session_id;               // Đọc và tăng
    pthread_mutex_unlock(&session_id_mutex);  // Mở khóa
    return id;
}
```

**[Slide 10: Tạo Thread mới]**

> Với mỗi client mới, em tạo một **thread riêng** để xử lý:
> 1. `malloc(ClientInfo)`: Cấp phát bộ nhớ lưu thông tin client
> 2. `pthread_create()`: Tạo thread mới, truyền hàm `client_thread` và thông tin client
> 3. `pthread_detach()`: Thread sẽ **tự động cleanup** khi kết thúc, không cần `join`
>
> **Lưu ý quan trọng:** Phải dùng `malloc()` vì nếu dùng biến local, khi main loop tiếp tục, biến sẽ bị **ghi đè**.

```c
pthread_t tid;
pthread_create(&tid, NULL, client_thread, (void *)info);
pthread_detach(tid);  // Tự động cleanup khi kết thúc
```

**[Slide 11: Flow xử lý client]**

> Tóm tắt flow khi một client kết nối:
> 1. `accept()` → nhận client socket
> 2. `get_next_session_id()` → tạo Session ID
> 3. `malloc(ClientInfo)` → lưu thông tin
> 4. `pthread_create()` → tạo thread mới
> 5. Thread mới gọi `handle_client()` → xử lý lệnh FTP
> 6. Client gửi `QUIT` hoặc ngắt kết nối
> 7. `free(info)` → giải phóng bộ nhớ
> 8. Thread tự động kết thúc

---

## 🎤 PHẦN 3: CONTROL CONNECTION (3-4 phút)

**[Slide 12: FTPSession Structure]**

> Bây giờ em xin trình bày về **quản lý phiên làm việc**.
>
> Mỗi client có một cấu trúc `FTPSession` riêng, lưu trữ:
> - `session_id`: ID phiên duy nhất
> - `ctrl_sock`: Socket điều khiển để gửi/nhận lệnh
> - `logged_in`: Trạng thái đăng nhập (0 = chưa, 1 = đã đăng nhập)
> - `username`: Tên đăng nhập
> - `root_dir`: Thư mục gốc của user (ví dụ: `./data/user1`)
> - `current_dir`: Thư mục làm việc hiện tại (ví dụ: `/files`)

**[Slide 13: Command Processing Loop]**

> Vòng lặp xử lý lệnh chính:
> 1. `recv()` nhận dữ liệu từ client - đây là hàm **blocking**
> 2. Xóa ký tự xuống dòng `\r\n`
> 3. Ghi log lệnh nhận được
> 4. **Parse lệnh**: Tách thành `cmd` (lệnh) và `arg` (tham số)
>    - Ví dụ: `"USER user1"` → cmd = `"USER"`, arg = `"user1"`
> 5. Điều hướng đến **handler tương ứng**

```c
while (running) {
    int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0) break;  // Client ngắt kết nối
    
    char *cmd = strtok(buffer, " ");   // Lệnh
    char *arg = strtok(NULL, "");      // Tham số
    
    if (strcasecmp(cmd, "USER") == 0) {
        cmd_user(&session, arg);
    } else if (strcasecmp(cmd, "PASS") == 0) {
        cmd_pass(&session, arg);
    }
    // ... các lệnh khác
}
```

**[Slide 14: Xử lý đăng nhập USER/PASS]**

> Quy trình đăng nhập theo chuẩn FTP gồm 2 bước:
>
> **Bước 1 - USER:**
> - Client gửi: `USER user1`
> - Server lưu username tạm thời vào session
> - Server trả về: `331 Username OK, need password`
>
> **Bước 2 - PASS:**
> - Client gửi: `PASS 123456`
> - Server gọi `check_login()` kiểm tra trong danh sách tài khoản
> - Nếu đúng: `logged_in = 1`, thiết lập `root_dir`, trả về `230 User logged in`
> - Nếu sai: Trả về `530 Login incorrect`

**[Slide 15: Bảo mật - Chống Path Traversal]**

> Một vấn đề bảo mật quan trọng là **Path Traversal Attack**.
>
> **Ví dụ:** Nếu user ở thư mục `/`, gửi lệnh `CWD ..` để đi lên thư mục cha → có thể truy cập ra ngoài thư mục được phép!
>
> **Giải pháp của em:**
> - Khi user đang ở root (`/`) và gõ `CWD ..`, server trả về `550 Permission denied`
> - Mọi đường dẫn đều được **ghép với root_dir** trước khi truy cập file system

```c
if (strcmp(arg, "..") == 0 && strcmp(session->current_dir, "/") == 0) {
    send_response(session->ctrl_sock, "550 Permission denied\r\n");
    return;
}
```

---

## 🎤 PHẦN 4: DEMO THỰC TẾ (2-3 phút)

**[Demo: Chạy Server]**

> Bây giờ em xin demo thực tế. Đầu tiên, em chạy server:

```bash
./bin/ftp_server
```

> Như các bạn thấy, server đã khởi động:
> - Load được 4 tài khoản từ file
> - Đang lắng nghe trên port 2121
> - Log được ghi vào thư mục `./server/logs/`

**[Demo: Client kết nối]**

> Tiếp theo, em mở một terminal khác và chạy client:

```bash
./bin/ftp_client
```

> Như các bạn thấy:
> - Client kết nối thành công, nhận được message `220 FTP Server Ready`
> - Em nhập username `cuong` và password
> - Server trả về `230 User logged in` - đăng nhập thành công!

**[Demo: Nhiều client đồng thời]**

> Bây giờ em mở thêm một client nữa trong terminal thứ 3:

```bash
./bin/ftp_client
```

> Như các bạn thấy trong log của server:
> - Client 1 có Session ID = 1
> - Client 2 có Session ID = 2
> - **Cả 2 client đều được xử lý đồng thời** nhờ cơ chế đa luồng

**[Demo: Xem log server]**

> Cuối cùng, em mở file log để xem:

```
[21:13:49] [INFO] === FTP SERVER ===
[21:26:13] [INFO] [SID=1] Client connected: 127.0.0.1:57468
[21:26:15] [CMD] [SID=1] USER cuong 127.0.0.1:57468
[21:26:15] [CMD] [SID=1] PASS **** 127.0.0.1:57468
[21:28:37] [INFO] [SID=2] Client connected: 127.0.0.1:43424
```

> Log ghi lại đầy đủ:
> - Thời gian
> - Session ID
> - Lệnh nhận được
> - IP:Port của client
> - **Password được ẩn thành `****`** để bảo mật

---

## 🎤 KẾT LUẬN (30 giây)

**[Slide cuối: Tổng kết]**

> Tóm lại, phần em đã thực hiện:
> 1. **Socket Server TCP** với đầy đủ các bước: socket → bind → listen → accept
> 2. **Xử lý đa client** bằng pthread, có mutex bảo vệ Session ID
> 3. **Control Connection** quản lý phiên, parse và xử lý các lệnh FTP cơ bản
> 4. **Logging** chi tiết với timestamp, Session ID, và ẩn password
> 5. **Bảo mật** chống path traversal attack
>
> Em xin cảm ơn thầy/cô và các bạn đã lắng nghe. Em sẵn sàng trả lời các câu hỏi!

---

# ❓ CÂU HỎI VẤN ĐÁP (Q&A)

> Dưới đây là các câu hỏi thường gặp khi bảo vệ đồ án và câu trả lời gợi ý.

---

## 📌 NHÓM 1: CÂU HỎI VỀ SOCKET

### **Câu 1: Tại sao dùng TCP thay vì UDP cho FTP?**

**Trả lời:**
> FTP cần truyền file **chính xác 100%**, không được mất dữ liệu. TCP cung cấp:
> - **Tin cậy (Reliability)**: Nếu gói tin bị mất, TCP tự động gửi lại
> - **Đúng thứ tự (Ordering)**: Các byte đến đúng thứ tự gửi
> - **Kiểm soát luồng (Flow control)**: Tránh gửi quá nhanh làm nghẽn
>
> UDP nhanh hơn nhưng không đảm bảo các yếu tố trên, phù hợp cho video streaming, game online - những ứng dụng có thể chấp nhận mất vài frame.

---

### **Câu 2: Hàm `bind()` làm gì? Tại sao cần thiết?**

**Trả lời:**
> `bind()` **gán địa chỉ IP:Port cho socket**. 
>
> - Sau khi tạo socket bằng `socket()`, socket chưa có địa chỉ
> - `bind()` liên kết socket với địa chỉ cụ thể (ví dụ: `0.0.0.0:2121`)
> - Sau `bind()`, **không process nào khác** có thể sử dụng port đó
> - Client dùng port này để kết nối đến server

---

### **Câu 3: `INADDR_ANY` (0.0.0.0) nghĩa là gì?**

**Trả lời:**
> `INADDR_ANY` nghĩa là server lắng nghe trên **tất cả network interfaces**:
> - `127.0.0.1` (localhost) - kết nối từ chính máy đó
> - `192.168.1.x` (WiFi) - kết nối từ mạng LAN
> - `10.0.0.x` (Ethernet) - kết nối từ mạng nội bộ
>
> Nếu chỉ bind vào `127.0.0.1`, client từ máy khác sẽ không kết nối được.

---

### **Câu 4: Tại sao cần `htons()` khi gán port?**

**Trả lời:**
> Máy tính (Intel/AMD) dùng **Little-Endian** nhưng mạng dùng **Big-Endian**.
>
> Ví dụ port `2121 = 0x0849`:
> - Little-Endian lưu: `49 08` (byte thấp trước)
> - Big-Endian lưu: `08 49` (byte cao trước)
>
> `htons()` = **Host TO Network Short** - chuyển đổi 16-bit từ host sang network byte order.
>
> Nếu quên dùng, server sẽ lắng nghe **sai port** (0x4908 = 18696 thay vì 2121).

---

### **Câu 5: Tham số `backlog` trong `listen()` có ý nghĩa gì?**

**Trả lời:**
> `backlog` là **số kết nối tối đa** có thể chờ trong hàng đợi.
>
> - Khi client gọi `connect()`, kernel đưa vào hàng đợi
> - Hàm `accept()` lấy kết nối ra khỏi hàng đợi để xử lý
> - Nếu hàng đợi đầy, client mới sẽ bị **từ chối** (ECONNREFUSED)
>
> Trong code em dùng `backlog = 10`, nghĩa là tối đa 10 client có thể chờ đồng thời.

---

## 📌 NHÓM 2: CÂU HỎI VỀ ĐA LUỒNG (MULTI-THREADING)

### **Câu 6: Tại sao cần đa luồng? Không dùng được không?**

**Trả lời:**
> **Có thể không dùng**, nhưng sẽ có vấn đề:
>
> - Nếu chỉ có 1 thread, khi client A đang download file 100MB (30 giây), client B phải **đợi 30 giây** mới được xử lý
> - Với đa luồng, mỗi client có thread riêng, **xử lý song song**
> - Tận dụng được CPU đa nhân
>
> Các phương án thay thế: `select()`, `poll()`, `epoll()` (I/O multiplexing) - nhưng phức tạp hơn.

---

### **Câu 7: Mutex là gì? Tại sao cần mutex cho Session ID?**

**Trả lời:**
> **Mutex** (Mutual Exclusion) là cơ chế **khóa** đảm bảo chỉ một thread truy cập tài nguyên tại một thời điểm.
>
> **Vấn đề race condition:**
> ```
> Thread 1: đọc next_session_id = 5
> Thread 2: đọc next_session_id = 5  (cùng lúc!)
> Thread 1: next_session_id = 6, trả về 6
> Thread 2: next_session_id = 7, trả về 7
> → Bỏ mất ID 6 cho Thread 2, cả hai có thể cùng ID!
> ```
>
> **Với mutex:**
> ```
> Thread 1: lock → đọc 5 → tăng 6 → trả về 6 → unlock
> Thread 2: (đợi) → lock → đọc 6 → tăng 7 → trả về 7 → unlock
> → Mỗi thread có ID khác nhau!
> ```

---

### **Câu 8: Tại sao dùng `malloc()` cho ClientInfo thay vì biến local?**

**Trả lời:**
> Vì biến local sẽ bị **ghi đè** khi vòng lặp tiếp tục.
>
> ```c
> while (1) {
>     ClientInfo info;  // Biến local trên stack
>     info.client_sock = accept(...);
>     pthread_create(&tid, NULL, thread_func, &info);
>     // Vòng lặp tiếp tục → info bị ghi đè!
>     // Thread mới đọc dữ liệu SAI!
> }
> ```
>
> Với `malloc()`:
> ```c
> ClientInfo *info = malloc(sizeof(ClientInfo));
> // Memory trên heap, tồn tại cho đến khi free()
> // Thread có thể đọc an toàn
> ```

---

### **Câu 9: `pthread_detach()` khác gì `pthread_join()`?**

**Trả lời:**
> - **`pthread_join()`**: Main thread **đợi** cho đến khi thread con kết thúc, sau đó mới tiếp tục
> - **`pthread_detach()`**: Thread con chạy **độc lập**, tự động cleanup khi kết thúc, main thread không cần đợi
>
> Trong FTP server, chúng ta dùng `detach()` vì:
> - Main thread cần tiếp tục `accept()` client mới ngay lập tức
> - Không cần đợi client cũ ngắt kết nối

---

## 📌 NHÓM 3: CÂU HỎI VỀ FTP PROTOCOL

### **Câu 10: FTP response code có quy tắc gì?**

**Trả lời:**
> Mã phản hồi FTP gồm **3 chữ số**, chữ số đầu cho biết loại:
>
> | Chữ số đầu | Ý nghĩa |
> |------------|---------|
> | **1xx** | Positive Preliminary (đang xử lý) |
> | **2xx** | Positive Completion (thành công) |
> | **3xx** | Positive Intermediate (cần thêm thông tin) |
> | **4xx** | Transient Negative (lỗi tạm thời) |
> | **5xx** | Permanent Negative (lỗi vĩnh viễn) |
>
> Ví dụ: `230` = thành công, `530` = lỗi vĩnh viễn (sai mật khẩu)

---

### **Câu 11: Tại sao FTP cần 2 kết nối (Control + Data)?**

**Trả lời:**
> - **Control Connection (port 21/2121)**: Gửi lệnh và nhận response
> - **Data Connection (port ngẫu nhiên)**: Truyền nội dung file, listing
>
> **Lý do tách riêng:**
> - Có thể gửi lệnh trong khi đang truyền file (ví dụ: ABORT để hủy)
> - Control connection luôn mở, data connection mở/đóng theo từng lệnh
> - Thiết kế từ những năm 1970, khi băng thông hạn chế

---

### **Câu 12: PASV mode hoạt động như thế nào?**

**Trả lời:**
> **Passive Mode (PASV):**
> 1. Client gửi `PASV` qua control connection
> 2. Server mở port ngẫu nhiên (ví dụ: 20020) và trả về: `227 (127,0,0,1,78,52)`
>    - IP: 127.0.0.1
>    - Port: 78 × 256 + 52 = 20020
> 3. Client **chủ động kết nối** đến port 20020
> 4. Sau đó mới gửi LIST/RETR/STOR
>
> **Tại sao dùng PASV thay vì Active mode?**
> - Client thường sau NAT/firewall, không thể nhận kết nối từ bên ngoài
> - PASV để client chủ động connect, dễ đi qua firewall hơn

---

## 📌 NHÓM 4: CÂU HỎI VỀ BẢO MẬT

### **Câu 13: Path Traversal Attack là gì? Cách phòng chống?**

**Trả lời:**
> **Path Traversal** là tấn công dùng `..` để truy cập file ngoài thư mục được phép.
>
> Ví dụ: User có home `/data/user1`, gửi `CWD ../../etc/passwd` → đọc file hệ thống!
>
> **Phòng chống trong code:**
> ```c
> // 1. Chặn CWD .. khi đang ở root
> if (strcmp(arg, "..") == 0 && strcmp(current_dir, "/") == 0) {
>     return "550 Permission denied";
> }
>
> // 2. Mọi path đều ghép với root_dir
> full_path = root_dir + current_dir + filename;
> // Không bao giờ truy cập trực tiếp path từ client
> ```

---

### **Câu 14: Tại sao ẩn password trong log?**

**Trả lời:**
> - **Bảo mật**: Nếu ai đọc được file log, sẽ biết password
> - **Compliance**: Các chuẩn bảo mật (PCI-DSS, GDPR) yêu cầu không lưu password dạng plaintext
> - **Best practice**: Log chỉ cần biết "có lệnh PASS", không cần biết giá trị
>
> Code:
> ```c
> if (strncasecmp(cmd, "PASS ", 5) == 0) {
>     log("PASS ****");  // Thay password bằng ****
> }
> ```

---

### **Câu 15: FTP có an toàn không? Làm sao để bảo mật hơn?**

**Trả lời:**
> FTP cơ bản **KHÔNG an toàn** vì:
> - Username/password gửi dạng **plaintext** (có thể bắt gói tin)
> - Dữ liệu file cũng không mã hóa
>
> **Giải pháp:**
> - **FTPS** (FTP over SSL/TLS): Mã hóa bằng SSL certificate
> - **SFTP** (SSH File Transfer Protocol): Chạy trên SSH, bảo mật hơn
> - Trong đồ án này, em chưa implement SSL vì độ phức tạp cao

---

## 📌 NHÓM 5: CÂU HỎI VỀ CODE CỤ THỂ

### **Câu 16: `strcasecmp()` khác gì `strcmp()`?**

**Trả lời:**
> - `strcmp()`: So sánh **phân biệt** hoa thường ("USER" ≠ "user")
> - `strcasecmp()`: So sánh **không phân biệt** hoa thường ("USER" = "user" = "User")
>
> FTP protocol quy định lệnh **không phân biệt** hoa thường, nên dùng `strcasecmp()`.

---

### **Câu 17: Tại sao dùng `strncpy()` thay vì `strcpy()`?**

**Trả lời:**
> - `strcpy()`: Copy không giới hạn → **buffer overflow** nếu source dài hơn destination
> - `strncpy()`: Copy tối đa n ký tự → an toàn hơn
>
> ```c
> char dest[50];
> strncpy(dest, source, sizeof(dest) - 1);
> dest[sizeof(dest) - 1] = '\0';  // Đảm bảo null-terminated
> ```
>
> **Lưu ý:** `strncpy()` không tự thêm `\0` nếu source >= n, nên phải thêm thủ công.

---

### **Câu 18: `recv()` trả về 0 nghĩa là gì?**

**Trả lời:**
> - `recv() > 0`: Nhận được n bytes dữ liệu
> - `recv() == 0`: Client **đã đóng kết nối** (gọi `close()` hoặc CTRL+C)
> - `recv() < 0`: Có **lỗi** (kiểm tra `errno`)
>
> Trong code:
> ```c
> int bytes = recv(sock, buffer, size, 0);
> if (bytes <= 0) {
>     // Client đóng kết nối hoặc lỗi → thoát vòng lặp
>     break;
> }
> ```

---

## 📌 NHÓM 6: CÂU HỎI MỞ RỘNG

### **Câu 19: Nếu có 1000 client kết nối cùng lúc thì sao?**

**Trả lời:**
> **Vấn đề:**
> - Mỗi thread tốn ~2MB stack memory → 1000 thread = 2GB RAM
> - Context switching nhiều thread làm giảm hiệu năng
>
> **Giải pháp thực tế:**
> - Dùng **Thread Pool**: Tạo sẵn N threads, tái sử dụng
> - Dùng **I/O Multiplexing**: `select()`, `poll()`, `epoll()` - 1 thread xử lý nhiều socket
> - Dùng **Async I/O**: libevent, libuv
>
> Đồ án này dùng 1 thread/client cho đơn giản, phù hợp với số lượng client nhỏ.

---

### **Câu 20: Có thể dùng ngôn ngữ khác thay vì C không?**

**Trả lời:**
> **Có**, nhưng mỗi ngôn ngữ có ưu/nhược:
>
> | Ngôn ngữ | Ưu điểm | Nhược điểm |
> |----------|---------|------------|
> | **C** | Hiệu năng cao, hiểu sâu hệ thống | Dễ lỗi memory, phức tạp |
> | **Python** | Dễ viết, thư viện socket có sẵn | Chậm hơn C |
> | **Go** | Goroutines nhẹ, xử lý concurrent tốt | Ít phổ biến |
> | **Rust** | An toàn bộ nhớ, hiệu năng như C | Học khó |
>
> Chọn C vì môn học yêu cầu và để hiểu sâu về hệ thống.

---

# 📋 TÀI LIỆU KỸ THUẬT CHI TIẾT

> Phần dưới đây là **tài liệu kỹ thuật chi tiết** với code và giải thích từng dòng.

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
