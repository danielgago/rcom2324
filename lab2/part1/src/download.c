#include "download.h"

struct FTPURL ftpURL;

int containsAtSymbol(const char *str) {
    while (*str != '\0') {
        if (*str == '@') {
            return 1;
        }
        str++;
    }
    return 0;
}

void getFileName(const char *urlPath) {
    int lastSlashIndex = -1;
    for (int i = 0; urlPath[i] != '\0'; ++i) {
        if (urlPath[i] == '/') {
            lastSlashIndex = i;
        }
    }

    if (lastSlashIndex != -1) {
        strncpy(ftpURL.pathToFile, urlPath, lastSlashIndex);
        ftpURL.pathToFile[lastSlashIndex] = '\0';
        strcpy(ftpURL.file, urlPath + lastSlashIndex + 1);
    } else {
        strcpy(ftpURL.pathToFile, urlPath);
        ftpURL.file[0] = '\0'; // No file in the path
    }
}

// Return 1 if the string contains an @ symbol, 0 otherwise
int parseFTPURL(const char *url, struct FTPURL *ftpURL) {
    const char *str = url;
    if (containsAtSymbol(str)) {
        char urlPath[500];
        sscanf(url, "ftp://%[^:]:%[^@]@%[^/]/%s", ftpURL->user, ftpURL->password, ftpURL->host, urlPath);
        getFileName(urlPath);
        return 1;
    }
    else {
        char urlPath[500];
        sscanf(url, "ftp://%[^/]/%s", ftpURL->host, urlPath);
        getFileName(urlPath);
        ftpURL->password[0] = '\0';
        ftpURL->user[0] = '\0';
        return 0;
    }
}

int newSocket(char* ip, int port) {
    int sockfd;
    struct sockaddr_in server_addr;

    /*server address handling*/
    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);    /*32 bit Internet address network byte ordered*/
    server_addr.sin_port = htons(port);        /*server TCP port must be network byte ordered */

    /*open a TCP socket*/
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        exit(-1);
    }
    /*connect to the server*/
    if (connect(sockfd,
                (struct sockaddr *) &server_addr,
                sizeof(server_addr)) < 0) {
        perror("connect()");
        exit(-1);
    }
    return sockfd;
}

int serverResponse(int sockfd, char* response) {
    memset(response, 0, 500);
    int responseCode; char responseCodeStr[4]; int pos = 4;

    read(sockfd, &responseCodeStr, 3);
    printf("%s", responseCodeStr);
    for(int i = 0; i < 4; i++) {
        if (i < 3)
            response[i] = responseCodeStr[i];
        else
            response[i] = ' ';
    }
    responseCodeStr[3] = '\0';
    responseCode = atoi(responseCodeStr);

    char byte;
    ReadServerState state = BEGIN;

    while (state != END) {
        read(sockfd, &byte, 1);
        printf("%c", byte);
        switch (state) {
            case BEGIN:
                if (byte == '\n') {
                    state = END;
                }
                else if (byte == '-') {
                    state = MULTI_LINE;
                }
                else if (byte == ' ') {
                    state = SINGLE_LINE;
                }
                else response[pos++] = byte;
                break;
            case SINGLE_LINE:
                if (byte == '\n') {
                    state = END;
                }
                else response[pos++] = byte;
                break;
            case MULTI_LINE:
                if (byte == '\n') {
                    memset(response, 0, 500); //We just want the last line
                    pos = 0;
                    state = BEGIN;
                }
                else response[pos++] = byte;
                break;
            case END:
                break;
        }
    }
    return responseCode;
}

int authentication(int sockfd, char* user, char* password) {
    char response[500];
    char userHandler[5+strlen(user)+1]; sprintf(userHandler, "user %s\n", user);
    char passwordHandler[5+strlen(password)+1]; sprintf(passwordHandler, "pass %s\n", user);
    
    write(sockfd, userHandler, strlen(userHandler));
    if (serverResponse(sockfd, response) != 331) {
        printf("Unknown user '%s'. Abort.\n", user);
        exit(-1);
    }

    write(sockfd, passwordHandler, strlen(passwordHandler));
    return serverResponse(sockfd, response);
}

int passiveMode(int sockfd, char* ip) {
    char response[500];
    write(sockfd, "pasv\n", 5);
    int responseCode = serverResponse(sockfd, response);
    if (responseCode != 227) {
        printf("Error entering passive mode. Abort.\n");
        exit(-1);
    }

    int ip1, ip2, ip3, ip4, port1, port2;
    sscanf(response, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)\n", &ip1, &ip2, &ip3, &ip4, &port1, &port2);
    sprintf(ip, "%d.%d.%d.%d", ip1, ip2, ip3, ip4);
    return port1*256+port2;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: ./download <URL>\n");
        return 1;
    }
    char *url = argv[1];
    int at_symbol = parseFTPURL(url, &ftpURL);
    printf("User: %s\nPassword: %s\nHost: %s\nPath to File: %s\nFilename: %s\n", ftpURL.user, ftpURL.password, ftpURL.host, ftpURL.pathToFile, ftpURL.file);
    struct hostent *h;
    if ((h = gethostbyname(ftpURL.host)) == NULL) {
        herror("gethostbyname()");
        exit(-1);
    }

    char *ip = inet_ntoa(*((struct in_addr *) h->h_addr));
    printf("IP Address : %s\n", ip);
    char response[500];
    int sockfd = newSocket(ip, FTP_SERVER_PORT);
    if (serverResponse(sockfd, response) != 220) {
        printf("Error connecting to server\n");
        exit(-1);
    }

    if(at_symbol) {
        if (authentication(sockfd, ftpURL.user, ftpURL.password) != 230) {
            printf("Wrong password. Abort.\n");
            exit(-1);
        }
    }
    else {
        if (authentication(sockfd, "anonymous", "anonymous") != 230) {
            printf("Wrong password. Abort.\n");
            exit(-1);
        }
    }

    char cwdHandler[5+strlen(ftpURL.pathToFile)+1]; sprintf(cwdHandler, "cwd %s\n", ftpURL.pathToFile);
    write(sockfd, cwdHandler, strlen(cwdHandler));
    if (serverResponse(sockfd, response) != 250) {
        printf("Directory not found. Abort.\n");
        exit(-1);
    }

    int filePort;
    char fileIP[16];

    filePort = passiveMode(sockfd, fileIP);
    printf("File IP: %s\n", fileIP);
    printf("File port: %d\n", filePort);

    int fileSockfd = newSocket(fileIP, filePort);

    char fileHandler[5+strlen(ftpURL.file)+1]; sprintf(fileHandler, "retr %s\n", ftpURL.file);
    write(fileSockfd, fileHandler, strlen(fileHandler));
    if (serverResponse(fileSockfd, response) != 150) {
        printf("File not found. Abort.\n");
        exit(-1);
    }

    FILE *file = fopen(ftpURL.file, "w");
    char buffer[500];
    int bytes;
    do {
        bytes = read(fileSockfd, buffer, 500);
        if (fwrite(buffer, bytes, 1, file) < 0) return -1;
    } while (bytes);
    fclose(file);


    return 0;
}
