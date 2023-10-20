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
    if (role[0] == 't' && role[1] == 'x')
        connectionParameters.role = LlTx;
    else if (role[0] == 'r' && role[1] == 'x')
        connectionParameters.role = LlRx;
    else
        perror("Invalid role.\n");
    for (int i = 0; i < 12; i++)
        connectionParameters.serialPort[i] = serialPort[i];
    connectionParameters.timeout = timeout;

    if (llopen(connectionParameters) == FALSE)
    {
        perror("Connection failed.\n");
        exit(-1);
    }
    printf("Connection established.\n");
    switch (connectionParameters.role)
    {
    case LlTx: ;
        FILE *file = fopen(filename, "rb");
        if (file == NULL)
        {
            perror("File not found\n");
            exit(-1);
        }

        fseek(file, 0, SEEK_END);
        long file_size = ftell(file);

        // Start packet
        int control_packet_size = 5 + sizeof(long) + strlen(filename);

        unsigned char *control_packet = (char *)malloc(control_packet_size * sizeof(char));
        int control_packet_pos = 0;

        control_packet[control_packet_pos++] = 2;
        control_packet[control_packet_pos++] = 0;
        control_packet[control_packet_pos++] = sizeof(long);

        for (int i = 0; i < sizeof(long); i++)
        {
            control_packet[control_packet_pos++] = (char)((file_size >> (i * 8)) & 0xFF);
        }

        control_packet[control_packet_pos++] = 1;
        control_packet[control_packet_pos++] = strlen(filename);

        for (size_t i = 0; i < strlen(filename); i++)
        {
            control_packet[control_packet_pos++] = filename[i];
        }

        llwrite(control_packet, control_packet_size);
/*
        // Data packets
        fseek(file, 0, SEEK_SET);
        unsigned char data[MAX_PAYLOAD_SIZE];
        int bytes_read;

        while ((bytes_read = fread(data, 1, sizeof(data), file)) > 0)
        {
            unsigned char *data_packet = (char *)malloc(3 + bytes_read);
            int data_packet_pos = 0;

            data_packet[data_packet_pos++] = 1;
            data_packet[data_packet_pos++] = bytes_read >> 8 & 0xFF;
            data_packet[data_packet_pos++] = bytes_read & 0xFF;

            for (int i = 0; i < sizeof(long); i++)
            {
                control_packet[control_packet_pos++] = data[i];
            }

            llwrite(data_packet, bytes_read);
        }

        // End packet
        control_packet[0] = 3;
        llwrite(control_packet, control_packet_size);
*/
        llclose(0);

        break;
    case LlRx: ;
        unsigned char *amogus = "sus";
        llread(amogus);
        llclose(0);
        break;
    }
}
