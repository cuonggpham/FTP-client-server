# ============================================================================
# FTP Client-Server Project
# A simple FTP implementation in C following RFC 959
# ============================================================================

CC := gcc
CFLAGS := -Wall -Wextra -g
LDFLAGS := -lpthread

# Directories
BUILD_DIR := build
BIN_DIR := bin
SERVER_SRC := server/src
CLIENT_SRC := client/src
SERVER_INC := server/include
CLIENT_INC := client/include

# Output binaries
SERVER_BIN := $(BIN_DIR)/ftp_server
CLIENT_BIN := $(BIN_DIR)/ftp_client
ACCOUNT_ADD_BIN := $(BIN_DIR)/account_add

# ============================================================================
# Main Targets
# ============================================================================

.PHONY: all clean server client account_add dirs setup help

all: dirs server client account_add
	@echo "Build complete. Binaries are in $(BIN_DIR)/"

dirs:
	@mkdir -p $(BUILD_DIR) $(BIN_DIR)

# ============================================================================
# Server Build
# ============================================================================

server: dirs
	$(CC) $(CFLAGS) -I $(SERVER_INC) -o $(SERVER_BIN) \
		$(SERVER_SRC)/server.c \
		$(SERVER_SRC)/account.c \
		$(SERVER_SRC)/ftp_server.c \
		$(LDFLAGS)
	@echo "Built: $(SERVER_BIN)"

# ============================================================================
# Client Build
# ============================================================================

client: dirs
	$(CC) $(CFLAGS) -I $(CLIENT_INC) -o $(CLIENT_BIN) \
		$(CLIENT_SRC)/client.c \
		$(CLIENT_SRC)/ftp_client.c
	@echo "Built: $(CLIENT_BIN)"

# ============================================================================
# Tools Build
# ============================================================================

account_add: dirs
	$(CC) $(CFLAGS) -I $(SERVER_INC) -o $(ACCOUNT_ADD_BIN) \
		$(SERVER_SRC)/account_add.c \
		$(SERVER_SRC)/account.c
	@echo "Built: $(ACCOUNT_ADD_BIN)"

# ============================================================================
# Utility Targets
# ============================================================================

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)
	@echo "Clean complete"

setup:
	mkdir -p server/data/admin server/data/user1
	@echo "Created data directories"

help:
	@echo "FTP Client-Server Build System"
	@echo ""
	@echo "Usage: make [target]"
	@echo ""
	@echo "Targets:"
	@echo "  all         - Build server, client, and account_add (default)"
	@echo "  server      - Build FTP server only"
	@echo "  client      - Build FTP client only"
	@echo "  account_add - Build account management tool"
	@echo "  clean       - Remove all build artifacts"
	@echo "  setup       - Create data directories"
	@echo "  help        - Show this help message"
