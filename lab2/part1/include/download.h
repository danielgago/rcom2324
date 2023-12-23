#ifndef _DOWNLOAD_H_
#define _DOWNLOAD_H_

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

// Function that checks if a string contains an @ symbol.
// Returns 1 if the string contains an @ symbol, 0 otherwise.
int containsAtSymbol(const char *str);

// Function that gets the file name from the URL path.
void getFileName(const char *urlPath);

// Function that parses the FTP URL with the following format: ftp://[<user>:<password>@]<host>/<url-path>
// Returns 1 if the URL contains a user and password, 0 otherwise.
int parseFTPURL(const char *url, struct FTPURL *ftpURL);

// Function that creates a new socket.
// Returns the socket file descriptor on success, -1 otherwise.
int newSocket(char* ip, int port);

// Function that reads the server response.
// Returns 1 on success, -1 otherwise.
int serverResponse(int sockfd, char* response);

// Function that authenticates the user.
// Returns 1 on success, -1 otherwise.
int authentication(int sockfd, char* user, char* password);

// Function that enters passive mode, and gets the IP address and port to download the file.
// Returns the port on success, -1 otherwise.
int passiveMode(int sockfd, char* ip);

#endif // _DOWNLOAD_H_