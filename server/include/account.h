#ifndef ACCOUNT_H
#define ACCOUNT_H

#define MAX_ACCOUNTS 100
#define MAX_USERNAME 50
#define MAX_PASSWORD 50
#define MAX_PATH_LEN 256

typedef struct {
    char username[MAX_USERNAME];
    char password[MAX_PASSWORD];
    char home_dir[MAX_PATH_LEN];
} Account;

extern Account accounts[MAX_ACCOUNTS];
extern int account_count;

int load_accounts(const char *filename);

// kiem tra thong tin dang nhap, tra ve chi so neu thanh cong, -1 neu that bai
int check_login(const char *username, const char *password);

int add_account(const char *username, const char *password, const char *home_dir);

// luu danh sach tai khoan ra file
int save_accounts(const char *filename);

#endif
