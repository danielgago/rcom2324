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
    case LlTx:;
        FILE *input_file = fopen(filename, "rb");
        if (input_file == NULL)
        {
            perror("File not found\n");
            exit(-1);
        }

        fseek(input_file, 0, SEEK_END);
        long input_file_size = ftell(input_file);

        // Start packet
        int tx_control_packet_size = 5 + sizeof(long) + strlen(filename);

        unsigned char *tx_control_packet = (unsigned char *)malloc(tx_control_packet_size * sizeof(unsigned char));
        int tx_control_packet_pos = 0;
        int timeout = FALSE;

        tx_control_packet[tx_control_packet_pos++] = 2;
        tx_control_packet[tx_control_packet_pos++] = 0;
        tx_control_packet[tx_control_packet_pos++] = sizeof(long);

        for (int i = 0; i < sizeof(long); i++)
        {
            tx_control_packet[tx_control_packet_pos++] = (unsigned char)((input_file_size >> (i * 8)) & 0xFF);
        }

        tx_control_packet[tx_control_packet_pos++] = 1;
        tx_control_packet[tx_control_packet_pos++] = strlen(filename);

        for (size_t i = 0; i < strlen(filename); i++)
        {
            tx_control_packet[tx_control_packet_pos++] = filename[i];
        }

        for (int i = 0; i < 10; i++)
        {
            printf("%u ", tx_control_packet[i]);
        }
        printf("\n");

        if(llwrite(tx_control_packet, tx_control_packet_size) == FALSE)
        {
            printf("TIMEOUT\n");
            break;
        }

        // Data packets
        fseek(input_file, 0, SEEK_SET);
        unsigned char data[MAX_BUF_SIZE];
        int bytes_read;

        while ((bytes_read = fread(data, 1, sizeof(data), input_file)) > 0)
        {
            unsigned char *tx_data_packet = (char *)malloc(3 + bytes_read);
            int tx_data_packet_pos = 0;

            tx_data_packet[tx_data_packet_pos++] = 1;
            tx_data_packet[tx_data_packet_pos++] = bytes_read >> 8 & 0xFF;
            tx_data_packet[tx_data_packet_pos++] = bytes_read & 0xFF;

            for (int i = 0; i < bytes_read; i++)
            {
                tx_data_packet[tx_data_packet_pos++] = data[i];
            }

            if(llwrite(tx_data_packet, bytes_read + 3) == FALSE)
            {
                printf("TIMEOUT\n");
                timeout = TRUE;
                break;
            }
        }

        if(timeout == TRUE)
            break;
        
        // End packet
        tx_control_packet[0] = 3;
        if(llwrite(tx_control_packet, tx_control_packet_size) == FALSE)
        {
            printf("TIMEOUT\n");
            break;
        }

        llclose(0);

        break;
    case LlRx:;
        // Start packet
        unsigned char rx_control_packet[MAX_PAYLOAD_SIZE];

        llread(rx_control_packet);
        unsigned char L1 = rx_control_packet[2];
        long output_file_size = 0;
        for (unsigned int i = 0; i < L1; i++)
            output_file_size = (output_file_size << 8) | rx_control_packet[i + 3];

        // still need to parse name (dont know why)

        // Data packets
        FILE *output_file = fopen(filename, "wb");
        if (output_file == NULL)
        {
            perror("Could not open file\n");
            exit(-1);
        }

        unsigned char rx_data_packet[MAX_PAYLOAD_SIZE];

        while (TRUE)
        {   
            printf("Hey!\n");
            while(llread(rx_data_packet) != TRUE) printf("Packet is NOT gonna be written");
            printf("Packet is gonna be written");
            for (int i = 0; i < 10; i++)
            {
                printf("%u ", rx_data_packet[i]);
            }
            printf("\n");
            long rx_data_size = 0;
            if (rx_data_packet[0] == 1)
            {
                printf("Hey2!\n");
                rx_data_size = (rx_data_packet[1] << 8) | rx_data_packet[2];
                size_t bytes_written = fwrite(&rx_data_packet[3], sizeof(char), rx_data_size, output_file);
                printf("Bytes written = %d\n", bytes_written);
            }
            else if (rx_data_packet[0] == 3)
                break;
        }
        printf("Hey4!\n");
        fclose(output_file);
        printf("Hey5!\n");
        llclose(0);
        break;
    }
}
