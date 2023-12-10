#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>

#define FTP_SERVER_PORT 21

struct FTPURL {
    char user[256];
    char password[256];
    char host[256];
    char urlPath[256];
};

typedef enum {
    BEGIN,
    SINGLE_LINE,
    MULTI_LINE,
    END
} ReadServerState;