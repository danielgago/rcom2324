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
// Return 1 if the string contains an @ symbol, 0 otherwise
int parseFTPURL(const char *url, struct FTPURL *ftpURL) {
    const char *str = url;
    if (containsAtSymbol(str)) {
        sscanf(url, "ftp://%[^:]:%[^@]@%[^/]/%s", ftpURL->user, ftpURL->password, ftpURL->host, ftpURL->urlPath);
        return 1;
    }
    else {
        sscanf(url, "ftp://%[^/]/%s", ftpURL->host, ftpURL->urlPath);
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
                    memset(response, 0, 500);
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

int passiveMode(int sockfd) {
    char response[500];
    write(sockfd, "pasv\n", 5);
    int responseCode = serverResponse(sockfd, response);
    if (responseCode != 227) {
        printf("Error entering passive mode. Abort.\n");
        exit(-1);
    }
    printf("Response:\n%s\n", response);
    int port1, port2;
    sscanf(response, "227 Entering Passive Mode (%*d,%*d,%*d,%*d,%d,%d)\n", &port1, &port2);
    return port1*256+port2;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: ./download <URL>\n");
        return 1;
    }
    char *url = argv[1];
    int at_symbol = parseFTPURL(url, &ftpURL);
    printf("User: %s\nPassword: %s\nHost: %s\nURL Path: %s\n", ftpURL.user, ftpURL.password, ftpURL.host, ftpURL.urlPath);
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

    int filePort;
    char fileIP[16];

    filePort = passiveMode(sockfd);


    char buf[] = "Mensagem de teste na travessia da pilha TCP/IP\n";
    size_t bytes;
    
    /*send a string to the server*/
    bytes = write(sockfd, buf, strlen(buf));
    if (bytes > 0)
        printf("Bytes escritos %ld\n", bytes);
    else {
        perror("write()");
        exit(-1);
    }

    if (close(sockfd)<0) {
        perror("close()");
        exit(-1);
    }


    return 0;
}
