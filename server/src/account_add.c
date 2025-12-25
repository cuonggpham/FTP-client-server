/*
 * Program to add new FTP account
 * Run: ./account_add
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "../include/account.h"

#define ACCOUNT_FILE "./server/data/accounts.txt"

int main() {
    char username[MAX_USERNAME];
    char password[MAX_PASSWORD];
    char home_dir[MAX_PATH_LEN];
    
    // Read current account list
    load_accounts(ACCOUNT_FILE);
    printf("\nCurrently %d accounts in the system.\n\n", account_count);
    
    // Enter new account information
    printf("=== ADD NEW ACCOUNT ===\n");
    
    printf("Username: ");
    fgets(username, sizeof(username), stdin);
    username[strcspn(username, "\r\n")] = 0;
    
    printf("Password: ");
    fgets(password, sizeof(password), stdin);
    password[strcspn(password, "\r\n")] = 0;
    
    printf("Home directory (leave empty for default): ");
    fgets(home_dir, sizeof(home_dir), stdin);
    home_dir[strcspn(home_dir, "\r\n")] = 0;
    
    // If empty, create default path
    if (strlen(home_dir) == 0) {
        snprintf(home_dir, sizeof(home_dir), 
                 "/home/dell/Documents/Code/FTP-client-server/server/data/%s", 
                 username);
    }
    
    // Add account
    if (add_account(username, password, home_dir) == 0) {
        printf("\nAccount added successfully!\n");
        
        // Check home directory
        struct stat st;
        if (stat(home_dir, &st) == 0) {
            // Directory already exists
            if (S_ISDIR(st.st_mode)) {
                printf("Using existing directory: %s\n", home_dir);
            } else {
                printf("Warning: %s exists but is not a directory!\n", home_dir);
            }
        } else {
            // Directory doesn't exist, create it
            if (mkdir(home_dir, 0755) == 0) {
                printf("Created new directory: %s\n", home_dir);
            } else {
                printf("Cannot create directory: %s (check permissions)\n", home_dir);
            }
        }
        
        // Save to file
        if (save_accounts(ACCOUNT_FILE) == 0) {
            printf("Saved to file: %s\n", ACCOUNT_FILE);
        }
    } else {
        printf("\nFailed to add account!\n");
    }
    
    return 0;
}
