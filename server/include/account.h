#ifndef ACCOUNT_H
#define ACCOUNT_H

#define MAX_ACCOUNTS 100
#define MAX_USERNAME 50
#define MAX_PASSWORD 50
#define MAX_PATH_LEN 256

/* Account information structure */
typedef struct {
    char username[MAX_USERNAME];
    char password[MAX_PASSWORD];
    char home_dir[MAX_PATH_LEN];
} Account;

/* Global variables for account list */
extern Account accounts[MAX_ACCOUNTS];
extern int account_count;

/* Load accounts from file */
int load_accounts(const char *filename);

/* Check login credentials, returns index if success, -1 if failed */
int check_login(const char *username, const char *password);

/* Add new account */
int add_account(const char *username, const char *password, const char *home_dir);

/* Save accounts to file */
int save_accounts(const char *filename);

#endif
