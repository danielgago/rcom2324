#include "download.h"

void parseFTPURL(const char *url, struct FTPURL *ftpURL) {
    // Assuming a well-formed FTP URL format
    sscanf(url, "ftp://%[^:]:%[^@]@%[^/]/%s", ftpURL->user, ftpURL->password, ftpURL->host, ftpURL->urlPath);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: ./download <URL>\n");
        return 1;
    }
    char *url = argv[1];
    struct FTPURL ftpURL;
    parseFTPURL(url, &ftpURL);
    printf("User: %s\n", ftpURL.user);
    printf("Password: %s\n", ftpURL.password);
    printf("Host: %s\n", ftpURL.host);
    printf("URL Path: %s\n", ftpURL.urlPath);

    return 0;
}
