## Ứng dụng Server

Ứng dụng server có các chức năng sau:

- Thêm tài khoản mới với **username**, **password** và **thư mục gốc**.  
  Danh sách tài khoản được lưu vào một file text và được đọc lại mỗi lần server khởi động.
- Nhận các lệnh từ client và hiển thị lệnh nhận được lên màn hình theo định dạng:  
  `hh:mm:ss <Lệnh> <Địa chỉ IP của client>`
- Xử lý các lệnh để client có thể thực hiện các thao tác:
  - Đăng nhập
  - Lấy danh sách file
  - Chuyển đổi thư mục làm việc
  - Kiểm tra thư mục làm việc hiện thời
  - Download file về client
  - Upload file từ client vào thư mục hiện thời

---

## Ứng dụng Client

Ứng dụng client có các chức năng sau:

- Gửi lệnh và thông tin đăng nhập đến server
- Hiển thị kết quả phản hồi của server theo định dạng:  
  `hh:mm:ss <Lệnh gửi đi> <Phản hồi của server>`
- Lệnh được nhập từ bàn phím, sinh viên phải có khả năng sử dụng các lệnh FTP để thực hiện các thao tác:
  - Xem thư mục đang làm việc
  - Đổi thư mục làm việc
  - Lấy danh sách file trong thư mục làm việc
  - Tải về một file
  - Tải lên một file vào thư mục đang làm việc
  - Gửi lệnh ngắt kết nối và kết thúc kết nối

---

## Yêu cầu bổ sung

- Server phải có khả năng làm việc được với **tối thiểu 1 client của bên thứ 3**  
  (ví dụ: **FileZilla**).
