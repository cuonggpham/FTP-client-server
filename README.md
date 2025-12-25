# FTP Client-Server

A simple FTP client and server implementation in C following RFC 959.

## Features

- **Multi-threaded server** - Handles multiple simultaneous client connections
- **User authentication** - Account-based login with password verification
- **Passive mode** - PASV data transfer for firewall compatibility
- **File operations** - Upload, download, and directory navigation

### Supported FTP Commands

| Command | Description |
|---------|-------------|
| `USER` | Specify username |
| `PASS` | Specify password |
| `PWD` | Print working directory |
| `CWD` | Change working directory |
| `LIST` | List directory contents |
| `RETR` | Download file from server |
| `STOR` | Upload file to server |
| `TYPE` | Set transfer type |
| `SYST` | Get system information |
| `PASV` | Enter passive mode |
| `QUIT` | Disconnect |

## Quick Start

### Build

```bash
make all
```

### Setup Data Directories

```bash
make setup
```

### Run Server

```bash
./bin/ftp_server [port]
# Default port: 2121
```

### Run Client

```bash
./bin/ftp_client [host] [port]
# Default: 127.0.0.1:2121
```

### Test Connection

```bash
# Terminal 1: Start server
./bin/ftp_server

# Terminal 2: Connect client
./bin/ftp_client
# Login with user1/123456
```

## Project Structure

```
FTP-client-server/
├── bin/                    # Compiled binaries
├── build/                  # Build artifacts
├── docs/                   # Documentation
├── scripts/                # Utility scripts
├── server/
│   ├── include/            # Server headers
│   │   ├── account.h
│   │   └── ftp_server.h
│   ├── src/                # Server source files
│   │   ├── server.c        # Main entry point
│   │   ├── ftp_server.c    # FTP protocol handler
│   │   ├── account.c       # Account management
│   │   └── account_add.c   # Account creation tool
│   └── data/               # User home directories
│       └── accounts.txt    # User credentials
├── client/
│   ├── include/            # Client headers
│   │   └── ftp_client.h
│   └── src/                # Client source files
│       ├── client.c        # Main entry point
│       └── ftp_client.c    # FTP client implementation
├── Makefile
├── README.md
└── LICENSE
```

## Make Targets

| Target | Description |
|--------|-------------|
| `make all` | Build server, client, and tools |
| `make server` | Build FTP server only |
| `make client` | Build FTP client only |
| `make account_add` | Build account management tool |
| `make clean` | Remove build artifacts |
| `make setup` | Create data directories |
| `make help` | Show available targets |

## Account Management

### Default Account

- Username: `user1`
- Password: `123456`

### Add New Account

```bash
./bin/account_add
```

### Account File Format

```
username password /path/to/home/directory
```

## FTP Response Codes

The server uses standard FTP response codes according to RFC 959:

### Positive Completion (2xx)

| Code | Message | Description |
|------|---------|-------------|
| `200` | OK | Command executed successfully |
| `200` | Type set to A/I | Transfer type changed |
| `211` | End | Feature list end |
| `215` | UNIX Type: L8 | System type response |
| `220` | FTP Server Ready | Server greeting on connection |
| `221` | Goodbye | Session terminated |
| `226` | Transfer complete | Data transfer finished |
| `227` | Entering Passive Mode | PASV mode with IP and port |
| `230` | User logged in | Authentication successful |
| `250` | Directory changed | CWD command successful |
| `257` | "path" is current directory | PWD response |

### Authentication (3xx)

| Code | Message | Description |
|------|---------|-------------|
| `331` | Username OK, need password | USER accepted, awaiting PASS |

### Connection Errors (4xx)

| Code | Message | Description |
|------|---------|-------------|
| `425` | Can't open data connection | Data connection failed |

### File/Action Errors (5xx)

| Code | Message | Description |
|------|---------|-------------|
| `501` | Syntax error | Invalid command arguments |
| `502` | Command not implemented | Unknown command |
| `503` | Login with USER first | Command requires authentication |
| `504` | Type not supported | Invalid transfer type |
| `530` | Not logged in | Authentication required |
| `530` | Login incorrect | Wrong username/password |
| `550` | Directory not found | CWD target doesn't exist |
| `550` | File not found | RETR target doesn't exist |
| `550` | Cannot create file | STOR failed |
| `550` | Failed to open directory | LIST failed |

## License

MIT License - see [LICENSE](LICENSE) file.