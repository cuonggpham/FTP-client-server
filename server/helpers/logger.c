#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <pthread.h>

#include "logger.h"

#define LOG_DIR "./server/logs"
#define MAX_LOG_LINE 1024

static FILE *log_file = NULL;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * Get current timestamp string
 */
static void get_timestamp(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    snprintf(buffer, size, "%04d-%02d-%02d %02d:%02d:%02d",
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
             t->tm_hour, t->tm_min, t->tm_sec);
}

/*
 * Get log level string
 */
static const char* get_level_string(LogLevel level) {
    switch (level) {
        case LOG_INFO:  return "INFO";
        case LOG_CMD:   return "CMD";
        case LOG_ERROR: return "ERROR";
        default:        return "UNKNOWN";
    }
}

/*
 * Initialize the logger
 */
int init_logger(void) {
    // Create log directory if it doesn't exist
    struct stat st = {0};
    if (stat(LOG_DIR, &st) == -1) {
        if (mkdir(LOG_DIR, 0755) != 0) {
            perror("Failed to create log directory");
            return -1;
        }
    }
    
    // Create log filename with current date
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char log_filename[256];
    snprintf(log_filename, sizeof(log_filename), 
             "%s/server_%04d-%02d-%02d.log",
             LOG_DIR, t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
    
    // Open log file in append mode
    pthread_mutex_lock(&log_mutex);
    log_file = fopen(log_filename, "a");
    pthread_mutex_unlock(&log_mutex);
    
    if (log_file == NULL) {
        perror("Failed to open log file");
        return -1;
    }
    
    // Log initialization
    log_info("Logger initialized");
    
    return 0;
}

/*
 * Log a message with specified level
 */
void log_message(LogLevel level, const char *format, ...) {
    if (log_file == NULL) return;
    
    char timestamp[32];
    get_timestamp(timestamp, sizeof(timestamp));
    
    char message[MAX_LOG_LINE];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    
    pthread_mutex_lock(&log_mutex);
    fprintf(log_file, "[%s] [%s] %s\n", 
            timestamp, get_level_string(level), message);
    fflush(log_file);
    pthread_mutex_unlock(&log_mutex);
}

/*
 * Log an FTP command with session ID and client IP
 */
void log_command(int session_id, const char *cmd, const char *client_ip) {
    if (log_file == NULL) return;
    
    char timestamp[32];
    get_timestamp(timestamp, sizeof(timestamp));
    
    pthread_mutex_lock(&log_mutex);
    fprintf(log_file, "[%s] [CMD] [SID=%d] %s %s\n", timestamp, session_id, cmd, client_ip);
    fflush(log_file);
    pthread_mutex_unlock(&log_mutex);
}

/*
 * Log info message
 */
void log_info(const char *format, ...) {
    if (log_file == NULL) return;
    
    char timestamp[32];
    get_timestamp(timestamp, sizeof(timestamp));
    
    char message[MAX_LOG_LINE];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    
    pthread_mutex_lock(&log_mutex);
    fprintf(log_file, "[%s] [INFO] %s\n", timestamp, message);
    fflush(log_file);
    pthread_mutex_unlock(&log_mutex);
}

/*
 * Log error message
 */
void log_error(const char *format, ...) {
    if (log_file == NULL) return;
    
    char timestamp[32];
    get_timestamp(timestamp, sizeof(timestamp));
    
    char message[MAX_LOG_LINE];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    
    pthread_mutex_lock(&log_mutex);
    fprintf(log_file, "[%s] [ERROR] %s\n", timestamp, message);
    fflush(log_file);
    pthread_mutex_unlock(&log_mutex);
}

/*
 * Close the logger
 */
void close_logger(void) {
    pthread_mutex_lock(&log_mutex);
    if (log_file != NULL) {
        log_info("Logger closed");
        fclose(log_file);
        log_file = NULL;
    }
    pthread_mutex_unlock(&log_mutex);
}
