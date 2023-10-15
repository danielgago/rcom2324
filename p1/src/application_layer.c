// Application layer protocol implementation

#include "application_layer.h"

#include "link_layer.h"

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    LinkLayer connectionParameters;

    connectionParameters.baudRate = baudRate;
    connectionParameters.nRetransmissions = nTries;
    connectionParameters.role = role == "tx" ? LlTx : LlRx;
    *connectionParameters.serialPort = serialPort;
    connectionParameters.timeout = timeout;

    if (llopen(connectionParameters) == FALSE)
    {
        perror("Connection failed.\n");
    }
    unsigned char f[4] = "ola";
    switch (connectionParameters.role)
    {
    case LlTx: ;
        /*FILE *file = fopen(filename, "rb");
        if (file == NULL)
        {
            perror("File not found\n");
            exit(-1);
        }

        int prev = ftell(file);
        fseek(file, 0L, SEEK_END);
        long int fileSize = ftell(file) - prev;
        fseek(file, prev, SEEK_SET);*/
        llwrite(f, 0);
        break;
    case LlRx: ;
        llread(f);
        break;
    }
}
