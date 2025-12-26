#ifndef LOGGER_H
#define LOGGER_H

/*
 * Logger Helper Module
 * Provides thread-safe logging to file for FTP server
 */

// Log levels
typedef enum {
    LOG_INFO,
    LOG_CMD,
    LOG_ERROR
} LogLevel;

/*
 * Initialize the logger
 * Creates log directory if needed and opens log file
 * Returns 0 on success, -1 on failure
 */
int init_logger(void);

/*
 * Log a message with specified level
 * Format: [YYYY-MM-DD HH:MM:SS] [LEVEL] message
 */
void log_message(LogLevel level, const char *format, ...);

/*
 * Log an FTP command with session ID and client IP
 * Format: [YYYY-MM-DD HH:MM:SS] [CMD] [S:id] command client_ip
 */
void log_command(int session_id, const char *cmd, const char *client_ip);

/*
 * Log info message
 */
void log_info(const char *format, ...);

/*
 * Log error message
 */
void log_error(const char *format, ...);

/*
 * Close the logger and cleanup resources
 */
void close_logger(void);

#endif // LOGGER_H
