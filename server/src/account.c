#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/account.h"

Account accounts[MAX_ACCOUNTS];
int account_count = 0;

/*
 * Doc danh sach tai khoan tu file
 * Format moi dong: username password home_dir
 * Tra ve so tai khoan doc duoc
 */
int load_accounts(const char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (fp == NULL)
    {
        printf("Cannot open account file: %s\n", filename);
        return -1;
    }

    char line[512];
    account_count = 0;

    while (fgets(line, sizeof(line), fp) != NULL && account_count < MAX_ACCOUNTS)
    {
        // Bo ky tu xuong dong
        line[strcspn(line, "\r\n")] = 0;

        // Bo qua dong trong
        if (strlen(line) == 0)
            continue;

        // Tach chuoi theo dau cach (username password home_dir)
        char *username_token = strtok(line, " ");
        if (username_token == NULL)
            continue;

        char *password_token = strtok(NULL, " ");
        if (password_token == NULL)
            continue;

        char *homedir_token = strtok(NULL, " ");
        if (homedir_token == NULL)
            continue;

        // Chi copy va tang account_count khi co du 3 field
        strncpy(accounts[account_count].username, username_token, MAX_USERNAME - 1);
        accounts[account_count].username[MAX_USERNAME - 1] = '\0';

        strncpy(accounts[account_count].password, password_token, MAX_PASSWORD - 1);
        accounts[account_count].password[MAX_PASSWORD - 1] = '\0';

        strncpy(accounts[account_count].home_dir, homedir_token, MAX_PATH_LEN - 1);
        accounts[account_count].home_dir[MAX_PATH_LEN - 1] = '\0';

        account_count++;
    }

    fclose(fp);
    printf("Loaded %d accounts from file\n", account_count);
    return account_count;
}

/*
 * Kiem tra dang nhap
 * Tra ve index cua tai khoan neu dung, -1 neu sai
 */
int check_login(const char *username, const char *password)
{
    for (int i = 0; i < account_count; i++)
    {
        if (strcmp(accounts[i].username, username) == 0 &&
            strcmp(accounts[i].password, password) == 0)
        {
            return i; // Dang nhap thanh cong
        }
    }
    return -1; // Sai thong tin
}

/*
 * Them tai khoan moi vao danh sach
 * Tra ve 0 neu thanh cong, -1 neu that bai
 */
int add_account(const char *username, const char *password, const char *home_dir)
{
    if (account_count >= MAX_ACCOUNTS)
    {
        printf("Account list is full!\n");
        return -1;
    }

    // kiem tra trung ten nguoi dung
    for (int i = 0; i < account_count; i++)
    {
        if (strcmp(accounts[i].username, username) == 0)
        {
            printf("Username already exists!\n");
            return -1;
        }
    }

    strncpy(accounts[account_count].username, username, MAX_USERNAME - 1);
    accounts[account_count].username[MAX_USERNAME - 1] = '\0';

    strncpy(accounts[account_count].password, password, MAX_PASSWORD - 1);
    accounts[account_count].password[MAX_PASSWORD - 1] = '\0';

    strncpy(accounts[account_count].home_dir, home_dir, MAX_PATH_LEN - 1);
    accounts[account_count].home_dir[MAX_PATH_LEN - 1] = '\0';

    account_count++;

    return 0;
}

/*
 * Luu danh sach tai khoan ra file
 */
int save_accounts(const char *filename)
{
    FILE *fp = fopen(filename, "w");
    if (fp == NULL)
    {
        printf("Cannot write to file: %s\n", filename);
        return -1;
    }

    for (int i = 0; i < account_count; i++)
    {
        fprintf(fp, "%s %s %s\n",
                accounts[i].username,
                accounts[i].password,
                accounts[i].home_dir);
    }

    fclose(fp);
    return 0;
}
