#ifndef ACCOUNT_H
#define ACCOUNT_H

#define MAX_ACCOUNTS 100
#define MAX_USERNAME 50
#define MAX_PASSWORD 50
#define MAX_PATH_LEN 256

// Struct luu thong tin tai khoan
typedef struct {
    char username[MAX_USERNAME];
    char password[MAX_PASSWORD];
    char home_dir[MAX_PATH_LEN];
} Account;

// Bien toan cuc luu danh sach tai khoan
extern Account accounts[MAX_ACCOUNTS];
extern int account_count;

// Ham doc tai khoan tu file
int load_accounts(const char *filename);

// Ham kiem tra dang nhap, tra ve index neu thanh cong, -1 neu that bai
int check_login(const char *username, const char *password);

// Ham them tai khoan moi
int add_account(const char *username, const char *password, const char *home_dir);

// Ham luu tai khoan ra file
int save_accounts(const char *filename);

#endif
