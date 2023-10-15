// Link layer protocol implementation
#include "link_layer.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

LinkLayerRole linkLayerRole;

int state = 0;
unsigned char N_local = 0x00;
int fd;

volatile int STOP = FALSE;

int alarmEnabled = FALSE;
int alarmCount = 0;

void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;
    STOP = TRUE;

    printf("Alarm #%d\n", alarmCount);
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{
    fd = open(connectionParameters.serialPort, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        perror(connectionParameters.serialPort);
        exit(-1);
    }

    struct termios oldtio;
    struct termios newtio;
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }

    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = connectionParameters.baudRate | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0.1; // Inter-character timer
    newtio.c_cc[VMIN] = 0;    // Blocking read until

    tcflush(fd, TCIOFLUSH);

    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }
    
    linkLayerRole = connectionParameters.role;
    switch (linkLayerRole)
    {
    case LlTx:
        unsigned char write_buf[5] = {0};
        write_buf[0] = FLAG;
        write_buf[1] = A_SENDER;
        write_buf[2] = SET;
        write_buf[3] = A_SENDER ^ SET;
        write_buf[4] = FLAG;

        (void)signal(SIGALRM, alarmHandler);

        while (alarmCount < 4 && state != 5)
        {
            if (alarmEnabled == FALSE)
            {
                STOP = FALSE;
                int bytes = write(fd, write_buf, MAX_PAYLOAD_SIZE);
                alarm(3);
                alarmEnabled = TRUE;

                sleep(1);

                unsigned char read_byte;
                while (STOP == FALSE)
                {
                    int bytes = read(fd, read_byte, 1);
                    switch (state)
                    {
                    case 0:
                        if (read_byte == FLAG)
                            state = 1;
                        else
                            state = 0;
                        break;
                    case 1:
                        if (read_byte == FLAG)
                            state = 1;
                        else if (read_byte == A_RECEIVER)
                            state = 2;
                        else
                            state = 0;
                        break;
                    case 2:
                        if (read_byte == FLAG)
                            state = 1;
                        else if (read_byte == UA)
                            state = 3;
                        else
                            state = 0;
                        break;
                    case 3:
                        if (read_byte == FLAG)
                            state = 1;
                        else if (read_byte == A_RECEIVER ^ UA)
                            state = 4;
                        else
                            state = 0;
                        break;
                    case 4:
                        if (read_byte == FLAG)
                        {
                            STOP = TRUE;
                            printf("Success!");
                            alarm(0);
                            state = 5;
                        }
                        else
                            state = 0;
                        break;
                    default:
                        break;
                    }
                }
            }
        }
        break;
    case LlRx:
        unsigned char read_buf[MAX_PAYLOAD_SIZE + 1] = {0};
        while (STOP == FALSE)
        {
            int bytes = read(fd, read_buf, 1);
            state_machine_control(read_buf[0], A_SENDER, SET, A_SENDER ^ SET);
            switch (state)
            {
            case 0:
                if (read_buf[0] == FLAG)
                    state = 1;
                else
                    state = 0;
                break;
            case 1:
                if (read_buf[0] == FLAG)
                    state = 1;
                else if (read_buf[0] == A_SENDER)
                    state = 2;
                else
                    state = 0;
                break;
            case 2:
                if (read_buf[0] == FLAG)
                    state = 1;
                else if (read_buf[0] == SET)
                    state = 3;
                else
                    state = 0;
                break;
            case 3:
                if (read_buf[0] == FLAG)
                    state = 1;
                else if (read_buf[0] == A_SENDER ^ SET)
                    state = 4;
                else
                    state = 0;
                break;
            case 4:
                if (read_buf[0] == FLAG)
                    STOP = TRUE;
                else
                    state = 0;
                break;
            default:
            }
        }

        sleep(1);

        unsigned char write_buf[MAX_PAYLOAD_SIZE] = {0};

        write_buf[0] = FLAG;
        write_buf[1] = A_RECEIVER;
        write_buf[2] = UA;
        write_buf[3] = A_RECEIVER ^ UA;
        write_buf[4] = FLAG;

        int bytes = write(fd, write_buf, 5);

        sleep(1);
        break;
    default:
        break;
    }
    return TRUE;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    alarmCount = 0;
    alarmEnabled = FALSE;
    unsigned char fake_data[10] = {0x00, 0x7E, 0x05, 0x7D, 0x11, 0xFF, 0x7E, 0x7D, 0x7E, 0xFF};
    unsigned char write_buf[MAX_PAYLOAD_SIZE] = {0};
    write_buf[0] = FLAG;
    write_buf[1] = A_SENDER;
    if (N_local == I0)
        write_buf[2] = I0;
    else
        write_buf[2] = I1;
    write_buf[3] = A_SENDER ^ write_buf[2]; // BCC1

    // BCC2 = P1^P2^...^Pn
    unsigned char bcc2 = fake_data[0];
    for (int k = 1; k < 10; k++)
    {
        bcc2 = bcc2 ^ fake_data[k];
    }

    // Stuffing
    int j = 4;
    int i = 0;
    while (i < 10)
    {
        if (fake_data[i] == FLAG)
        {
            write_buf[j] = 0x7D;
            write_buf[j + 1] = 0x5E;
            j += 2;
        }
        else if (fake_data[i] == 0x7D)
        {
            write_buf[j] = 0x7D;
            write_buf[j + 1] = 0x5D;
            j += 2;
        }
        else
        {
            write_buf[j] = fake_data[i];
            j++;
        }
        i++;
    }
    if (bcc2 == FLAG)
    {
        write_buf[j] = 0x7D;
        write_buf[j + 1] = 0x5E;
        j += 2;
    }
    else if (bcc2 == 0x7D)
    {
        write_buf[j] = 0x7D;
        write_buf[j + 1] = 0x5D;
        j += 2;
    }
    else
    {
        write_buf[j] = bcc2;
        j++;
    }
    write_buf[j] = FLAG;

    (void)signal(SIGALRM, alarmHandler);

    while (alarmCount < 4)
    {
        if (alarmEnabled == FALSE)
        {
            STOP = FALSE;
            int bytes = write(fd, write_buf, bufSize);
            
            alarm(3);
            alarmEnabled = TRUE;

            sleep(1);

            unsigned char read_buf[MAX_PAYLOAD_SIZE + 1] = {0};
            while (STOP == FALSE)
            {

                int bytes = read(fd, read_buf, 1);
                if (N_local == 0x00)
                    write_state_machine(read_buf[0], A_RECEIVER, RR1, A_RECEIVER ^ RR1);
                else if (N_local == 0x40)
                    write_state_machine(read_buf[0], A_RECEIVER, RR0, A_RECEIVER ^ RR0);
            }
        }
    }

    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    STOP = FALSE;
    state = 0;
    unsigned char response;
    unsigned char data[BUF_SIZE] = {0};
    int pos = 0;
    unsigned char read_buf[BUF_SIZE + 1] = {0};
    while (STOP == FALSE)
    {
        int bytes = read(fd, read_buf, 1);
        state_machine_info(read_buf[0], &pos, data, A_SENDER, I0, A_SENDER^I0);
    }

    for(int i = 0; i < pos; i++){
        printf("0x%02X\n", data[i]);
    }

    /*Lazy Approach, need to change this!!!*/

    if(N_local == 0x00){
        response = RR1;
        N_local = 0x40;
    }
    else{
        response = RR0;
        N_local = 0x00;
    }

    sleep(1);

    unsigned char write_buf[5] = {0};

    write_buf[0] = FLAG;
    write_buf[1] = A_RECEIVER;
    write_buf[2] = response;    
    write_buf[3] = A_RECEIVER^response;
    write_buf[4] = FLAG;

    int bytes = write(fd, write_buf, 5);
    
    sleep(1);
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    // TODO

    return 1;
}