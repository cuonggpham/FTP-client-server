CC = gcc
CFLAGS = -Wall -g

# Duong dan
SERVER_SRC = server/src
CLIENT_SRC = client/src
SERVER_OUT = server/server
CLIENT_OUT = client/client

# Target mac dinh: compile ca hai
all: server client

# Compile server (them -lpthread cho multithread)
server:
	$(CC) $(CFLAGS) -o $(SERVER_OUT) \
		$(SERVER_SRC)/server.c \
		$(SERVER_SRC)/account.c \
		$(SERVER_SRC)/ftp_server.c \
		-lpthread

# Compile client
client:
	$(CC) $(CFLAGS) -o $(CLIENT_OUT) \
		$(CLIENT_SRC)/client.c \
		$(CLIENT_SRC)/ftp_client.c

# Xoa file binary
clean:
	rm -f $(SERVER_OUT) $(CLIENT_OUT)

# Tao thu muc data cho user admin
setup:
	mkdir -p server/data/admin
	mkdir -p server/data/user1

.PHONY: all server client clean setup
