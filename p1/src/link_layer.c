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
struct termios oldtio;
int nRetransmissions = 3;
int timeout = 4;

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
    newtio.c_cc[VTIME] = 1; // Inter-character timer
    newtio.c_cc[VMIN] = 0;  // Blocking read until

    tcflush(fd, TCIOFLUSH);

    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    int success = FALSE;
    unsigned char write_buf[5] = {0};
    linkLayerRole = connectionParameters.role;
    nRetransmissions = connectionParameters.nRetransmissions;
    timeout = connectionParameters.timeout;
    switch (linkLayerRole)
    {
    case LlTx:;
        printf("LlTx\n");
        write_buf[0] = FLAG;
        write_buf[1] = A_SENDER;
        write_buf[2] = SET;
        write_buf[3] = A_SENDER ^ SET;
        write_buf[4] = FLAG;

        (void)signal(SIGALRM, alarmHandler);

        while (alarmCount < nRetransmissions && state != 5)
        {
            if (alarmEnabled == FALSE)
            {
                STOP = FALSE;
                int bytes = write(fd, write_buf, 5);
                alarm(timeout);
                alarmEnabled = TRUE;

                unsigned char read_byte;
                while (STOP == FALSE)
                {
                    int bytes = read(fd, &read_byte, 1);
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
                            success = TRUE;
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
    case LlRx:;
        printf("LlRx\n");
        unsigned char read_byte;
        while (STOP == FALSE)
        {
            int bytes = read(fd, &read_byte, 1);
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
                else if (read_byte == A_SENDER)
                    state = 2;
                else
                    state = 0;
                break;
            case 2:
                if (read_byte == FLAG)
                    state = 1;
                else if (read_byte == SET)
                    state = 3;
                else
                    state = 0;
                break;
            case 3:
                if (read_byte == FLAG)
                    state = 1;
                else if (read_byte == A_SENDER ^ SET)
                    state = 4;
                else
                    state = 0;
                break;
            case 4:
                if (read_byte == FLAG)
                {
                    STOP = TRUE;
                    success = TRUE;
                }
                else
                    state = 0;
                break;
            }
        }

        write_buf[0] = FLAG;
        write_buf[1] = A_RECEIVER;
        write_buf[2] = UA;
        write_buf[3] = A_RECEIVER ^ UA;
        write_buf[4] = FLAG;

        int bytes = write(fd, write_buf, 5);

        break;
    default:
        break;
    }
    return success;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    sleep(1);
    printf("llwrite - Nlocal %d\n", N_local);
    
    printf("bufSize = %d\n", bufSize);
    alarmCount = 0;
    state = 0;
    alarmEnabled = FALSE;
    unsigned char write_buf[MAX_PAYLOAD_SIZE] = {0};
    write_buf[0] = FLAG;
    write_buf[1] = A_SENDER;
    if (N_local == I0)
        write_buf[2] = I0;
    else
        write_buf[2] = I1;
    write_buf[3] = A_SENDER ^ write_buf[2]; // BCC1

    // BCC2 = P1^P2^...^Pn
    unsigned char bcc2 = buf[0];
    for (int k = 1; k < bufSize; k++)
    {
        bcc2 = bcc2 ^ buf[k];
    }

    // Stuffing
    int j = 4;
    int i = 0;
    while (i < bufSize)
    {
        if (buf[i] == FLAG)
        {
            write_buf[j] = ESC;
            write_buf[j + 1] = 0x5E;
            j += 2;
        }
        else if (buf[i] == ESC)
        {
            write_buf[j] = ESC;
            write_buf[j + 1] = 0x5D;
            j += 2;
        }
        else
        {
            write_buf[j] = buf[i];
            j++;
        }
        i++;
    }
    if (bcc2 == FLAG)
    {
        write_buf[j] = ESC;
        write_buf[j + 1] = 0x5E;
        j += 2;
    }
    else if (bcc2 == ESC)
    {
        write_buf[j] = ESC;
        write_buf[j + 1] = 0x5D;
        j += 2;
    }
    else
    {
        write_buf[j] = bcc2;
        j++;
    }
    write_buf[j] = FLAG;
    j++;

    for (int i = 0; i < 10; i++)
    {
        printf("%u ", write_buf[i]);
    }
    printf("\n");

    (void)signal(SIGALRM, alarmHandler);

    while (alarmCount < nRetransmissions && state != 5)
    {
        if (alarmEnabled == FALSE)
        {
            STOP = FALSE;
            printf("j = %d\n", j);
            int bytes = write(fd, write_buf, j+1);
            alarm(timeout);
            alarmEnabled = TRUE;

            unsigned char read_byte;
            unsigned char control;
            while (STOP == FALSE)
            {

                int bytes = read(fd, &read_byte, 1);

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
                    else if (read_byte == RR1 || read_byte == RR0 || read_byte == REJ1 || read_byte == REJ0){
                        control = read_byte;
                        state = 3;
                    }
                    else
                        state = 0;
                    break;
                case 3:
                    if (read_byte == FLAG)
                        state = 1;
                    else if (read_byte == A_RECEIVER ^ control)
                        state = 4;
                    else
                        state = 0;
                    break;
                case 4:
                    if (read_byte == FLAG)
                    {
                        if(control == RR0 || control == RR1){
                            STOP = TRUE;
                            alarm(0);
                            state = 5;
                            if ((N_local == I0 && control == RR1) || (N_local == I1 && control == RR0)){
                                if (N_local == I1)
                                    N_local = I0;
                                else if (N_local == I0)
                                    N_local = I1;
                            }
                        }
                        else if(control == REJ0 || control == REJ1){
                            if ((N_local == I0 && control == REJ0) || (N_local == I1 && control == REJ1)){ //Sent wrong info, but reader already read well once
                                STOP = TRUE;
                                alarm(0);
                                state = 5;
                                if (N_local == I1)
                                    N_local = I0;
                                else if (N_local == I0)
                                    N_local = I1;
                            }
                            else {
                                STOP = FALSE;
                                alarm(0);
                                alarm(timeout);
                                state = 0;
                            }
                        }
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

    return alarmCount < nRetransmissions;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    sleep(1);
    printf("llread - Nlocal %d\n", N_local);
    STOP = FALSE;
    state = 0;
    unsigned char response;
    unsigned char writer_nlocal;
    unsigned char data[MAX_PAYLOAD_SIZE] = {0};
    int pos = 0;
    unsigned char read_byte;
    int new_packet = FALSE;
    while (STOP == FALSE)
    {
        int bytes = read(fd, &read_byte, 1);

        switch (state)
        {
        case 0:
            printf("state %d (read_byte = %02x)\n", state, read_byte);
            if (read_byte == FLAG)
                state = 1;
            else
                state = 0;
            break;
        case 1:
            if (read_byte == A_SENDER)
                state = 2;
            else{
                printf("Unexpected byte. Are you not the sender?\n");
                if(N_local == 0x00)
                    response = REJ1;
                else if(N_local == 0x40)
                    response = REJ0;
                STOP = TRUE;
            }
            break;
        case 2:
            
            if(N_local == 0x00 && read_byte == I0){
                writer_nlocal = I0;
                state = 3;
            }
            else if(N_local == 0x40 && read_byte == I1){
                writer_nlocal = I1;
                state = 3;
            }
            else if(N_local == 0x00 && read_byte == I1){
                response = RR1;
                STOP = TRUE; //Check BCC1 first
            }
            else if(N_local == 0x40 && read_byte == I0){
                response = RR0;
                STOP = TRUE; //Check BCC1 first
            }
            else{
                printf("Unexpected byte. I(n) must be 0x00 or 0x40\n");
                if(N_local == 0x00)
                    response = REJ1;
                else if(N_local == 0x40)
                    response = REJ0;
                STOP = TRUE;
            }
            
            break;
        case 3:
            if (read_byte == A_SENDER ^ writer_nlocal)
                state = 4;
            else {
                printf("Error in BCC1\n");
                if(N_local == 0x00)
                    response = REJ1;
                else if(N_local == 0x40)
                    response = REJ0;
                STOP = TRUE;
                }
            break;
        case 4:
            if (read_byte == FLAG)
            {
                unsigned char destuf[MAX_PAYLOAD_SIZE] = {0};
                int a = 0, b = 0;
                while (a < pos)
                {
                    if (data[a] == ESC)
                    {
                        a++;
                        if (data[a] == 0x5E)
                        {
                            destuf[b] = 0x7E;
                        }
                        else if (data[a] == 0x5D)
                        {
                            destuf[b] = ESC;
                        }
                    }
                    else
                    {
                        destuf[b] = data[a];
                    }
                    b++;
                    a++;
                }
                unsigned char bcc2 = destuf[0];
                for (int i = 1; i < b - 1; i++)
                {
                    bcc2 = bcc2 ^ destuf[i];
                }
                if (destuf[b - 1] == bcc2)
                {
                    STOP = TRUE;
                    new_packet = TRUE;
                    for (int i = 0; i < b - 1; i++)
                    {
                        data[i] = destuf[i];
                    }
                    pos = b - 1;
                    if(N_local == 0x00){
                        response = RR1;
                        N_local = 0x40;
                    }
                    else if(N_local == 0x40){
                        response = RR0;
                        N_local = 0x00;
                    }
                }
                else
                {
                    printf("Error in BCC2\n");
                    if(N_local == 0x00)
                        response = REJ1;
                    else if(N_local == 0x40)
                        response = REJ0;
                    pos = 0;
                    STOP = TRUE;
                }
            }
            else
            {
                data[pos] = read_byte;
                pos++;
            }

            break;
        default:
            break;
        }
    }
    if (new_packet == TRUE){
        for (int i = 0; i < pos; i++)
        {
            packet[i] = data[i];
        }
    }


    unsigned char write_buf[5] = {0};
    write_buf[0] = FLAG;
    write_buf[1] = A_RECEIVER;
    write_buf[2] = response;
    write_buf[3] = A_RECEIVER ^ response;
    write_buf[4] = FLAG;

    int bytes = write(fd, write_buf, 5);
    return new_packet;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    printf("llclose\n");
    state = 0;
    alarmCount = 0;
    alarmEnabled = FALSE;
    STOP = FALSE;
    unsigned char write_buf[5] = {0};
    switch (linkLayerRole)
    {
    case LlTx:;
        write_buf[0] = FLAG;
        write_buf[1] = A_SENDER;
        write_buf[2] = DISC;
        write_buf[3] = A_SENDER ^ DISC;
        write_buf[4] = FLAG;

        (void)signal(SIGALRM, alarmHandler);

        while (alarmCount < nRetransmissions && state != 5)
        {
            if (alarmEnabled == FALSE)
            {
                STOP = FALSE;
                int bytes = write(fd, write_buf, 5);
                alarm(timeout);
                alarmEnabled = TRUE;


                unsigned char read_byte;
                while (STOP == FALSE)
                {
                    int bytes = read(fd, &read_byte, 1);
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
                        else if (read_byte == DISC)
                            state = 3;
                        else
                            state = 0;
                        break;
                    case 3:
                        if (read_byte == FLAG)
                            state = 1;
                        else if (read_byte == A_RECEIVER ^ DISC)
                            state = 4;
                        else
                            state = 0;
                        break;
                    case 4:
                        if (read_byte == FLAG)
                        {
                            STOP = TRUE;
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

        write_buf[0] = FLAG;
        write_buf[1] = A_SENDER;
        write_buf[2] = UA;
        write_buf[3] = A_SENDER ^ UA;
        write_buf[4] = FLAG;
        write(fd, write_buf, 5);

        break;
    case LlRx:;
        unsigned char read_byte;
        while (STOP == FALSE)
        {
            int bytes = read(fd, &read_byte, 1);
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
                else if (read_byte == A_SENDER)
                    state = 2;
                else
                    state = 0;
                break;
            case 2:
                if (read_byte == FLAG)
                    state = 1;
                else if (read_byte == DISC)
                    state = 3;
                else
                    state = 0;
                break;
            case 3:
                if (read_byte == FLAG)
                    state = 1;
                else if (read_byte == A_SENDER ^ DISC)
                    state = 4;
                else
                    state = 0;
                break;
            case 4:
                if (read_byte == FLAG)
                {
                    STOP = TRUE;
                }
                else
                    state = 0;
                break;
            }
        }

        write_buf[0] = FLAG;
        write_buf[1] = A_RECEIVER;
        write_buf[2] = DISC;
        write_buf[3] = A_RECEIVER ^ DISC;
        write_buf[4] = FLAG;

        write(fd, write_buf, 5);
        state = 0;
        alarmCount = 0;
        alarmEnabled = FALSE;
        STOP = FALSE;

        while (STOP == FALSE)
        {
            int bytes = read(fd, &read_byte, 1);
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
                else if (read_byte == A_SENDER)
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
                else if (read_byte == A_SENDER ^ UA)
                    state = 4;
                else
                    state = 0;
                break;
            case 4:
                if (read_byte == FLAG)
                {
                    STOP = TRUE;
                }
                else
                    state = 0;
                break;
            }
        }

        break;
    default:
        break;
    }

    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}
