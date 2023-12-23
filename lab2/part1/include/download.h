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
    char user[500];
    char password[500];
    char host[500];
    char pathToFile[500];
    char file[500];
};

typedef enum {
    BEGIN,
    SINGLE_LINE,
    MULTI_LINE,
    END
} ReadServerState;