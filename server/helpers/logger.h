#ifndef LOGGER_H
#define LOGGER_H

/*
 * module ho tro logger
 * cung cap ghi log an toan da luong vao file cho may chu FTP
 */

// cac muc log
typedef enum {
    LOG_INFO,
    LOG_CMD,
    LOG_ERROR
} LogLevel;

/*
 * khoi tao logger
 * tao thu muc log neu can va mo file log
 * tra ve 0 neu thanh cong, -1 neu that bai
 */
int init_logger(void);

/*
 * ghi log thong diep voi muc chi dinh
 * dinh dang: [YYYY-MM-DD HH:MM:SS] [LEVEL] thong diep
 */
void log_message(LogLevel level, const char *format, ...);

/*
 * ghi log lenh FTP voi session ID va IP client
 * dinh dang: [YYYY-MM-DD HH:MM:SS] [CMD] [S:id] lenh IP_client
 */
void log_command(int session_id, const char *cmd, const char *client_ip);

/*
 * ghi log thong tin
 */
void log_info(const char *format, ...);

/*
 * ghi log loi
 */
void log_error(const char *format, ...);

/*
 * dong logger va giai phong tai nguyen
 */
void close_logger(void);

#endif // LOGGER_H
