// Application layer protocol implementation

#include "application_layer.h"

#include "link_layer.h"
#include <stdio.h>
#include <stdlib.h>

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    LinkLayer connectionParameters;

    connectionParameters.baudRate = baudRate;
    connectionParameters.nRetransmissions = nTries;
    if(role[0] == 't' && role[1] == 'x')
        connectionParameters.role = LlTx;
    else if(role[0] == 'r' && role[1] == 'x')
        connectionParameters.role = LlRx;
    else
        perror("Invalid role.\n");
    for(int i=0; i<12; i++)
        connectionParameters.serialPort[i] = serialPort[i];
    connectionParameters.timeout = timeout;

    if (llopen(connectionParameters) == FALSE)
    {
        perror("Connection failed.\n");
        exit(-1);
    }
    printf("Connection established.\n");
    unsigned char f[4] = "ola";
    switch (connectionParameters.role)
    {
    case LlTx: ;
        FILE *file = fopen(filename, "rb");
        if (file == NULL)
        {
            perror("File not found\n");
            exit(-1);
        }

        
        llwrite(f, 0);
        break;
    case LlRx:
        llread(f);
        break;
    }
}
