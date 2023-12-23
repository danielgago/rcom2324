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
#include <sys/time.h>

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

LinkLayerRole linkLayerRole;
struct termios oldtio;
int maxNReTransmissions = 3;
int timeout = 4;

int state = 0;
unsigned char N_local = I0;
int fd;

volatile int STOP = FALSE;

int alarmEnabled = FALSE;
int nReTransmissions = 0;

//Statistics
int totalAlarms = 0;
int totalRejects = 0;
int totalReTransmissions = 0;
int totalBytesSent = 0;
int totalDataBytesReceived = 0;
struct timeval begin, end;

void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    nReTransmissions++;
    totalAlarms++;
    totalReTransmissions++;
    state = 0;
    STOP = TRUE;

    printf("Alarm activated (Retransmission #%d)\n", nReTransmissions);
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{
    gettimeofday(&begin, 0);
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
    maxNReTransmissions = connectionParameters.nRetransmissions;
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

        while (nReTransmissions < maxNReTransmissions && state != 5)
        {
            if (alarmEnabled == FALSE)
            {
                STOP = FALSE;
                write(fd, write_buf, 5);
                alarm(timeout);
                alarmEnabled = TRUE;

                unsigned char read_byte;
                while (STOP == FALSE)
                {
                    read(fd, &read_byte, 1);
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
                        else if (read_byte == (A_RECEIVER ^ UA))
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
            read(fd, &read_byte, 1);
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
                else if (read_byte == (A_SENDER ^ SET))
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

        write(fd, write_buf, 5);

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
    // sleep(1);
    nReTransmissions = 0;
    state = 0;
    alarmEnabled = FALSE;
    unsigned char *write_buf = (unsigned char *)malloc(bufSize * 2);
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

    (void)signal(SIGALRM, alarmHandler);

    while (nReTransmissions < maxNReTransmissions && state != 5)
    {
        if (alarmEnabled == FALSE)
        {
            STOP = FALSE;
            write(fd, write_buf, j + 1);
            alarm(timeout);
            alarmEnabled = TRUE;

            unsigned char read_byte;
            unsigned char control;
            while (STOP == FALSE)
            {

                read(fd, &read_byte, 1);

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
                    else if (read_byte == RR1 || read_byte == RR0 || read_byte == REJ1 || read_byte == REJ0)
                    {
                        control = read_byte;
                        state = 3;
                    }
                    else
                        state = 0;
                    break;
                case 3:
                    if (read_byte == FLAG)
                        state = 1;
                    else if (read_byte == (A_RECEIVER ^ control))
                        state = 4;
                    else
                        state = 0;
                    break;
                case 4:
                    if (read_byte == FLAG)
                    {
                        if (control == RR0 || control == RR1)
                        {
                            STOP = TRUE;
                            alarm(0);
                            alarmEnabled = FALSE;
                            totalBytesSent += j;
                            state = 5;
                            if ((N_local == I0 && control == RR1) || (N_local == I1 && control == RR0))
                            {
                                if (N_local == I1)
                                    N_local = I0;
                                else if (N_local == I0)
                                    N_local = I1;
                            }
                        }
                        else if (control == REJ0 || control == REJ1)
                        {
                            STOP = TRUE;
                            alarm(0);
                            alarmEnabled = FALSE;
                            nReTransmissions++;
                            totalReTransmissions++;
                            state = 0;
                            printf("Sent frame got rejected (Retransmission #%d)\n", nReTransmissions);
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
    free(write_buf);

    return nReTransmissions < maxNReTransmissions;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    // sleep(1);
    STOP = FALSE;
    state = 0;
    unsigned char response;
    unsigned char writer_nlocal;
    int pos = 0;
    unsigned char read_byte;
    int new_packet = FALSE;
    int escapeRead = FALSE;
    unsigned char bcc2 = 0x00;
    while (STOP == FALSE)
    {
        read(fd, &read_byte, 1);

        switch (state)
        {
        case 0:
            if (read_byte == FLAG)
                state = 1;
            break;
        case 1:
            if (read_byte == A_SENDER)
                state = 2;
            else if (read_byte == FLAG)
                state = 1;
            else
                state = 0;
            break;
        case 2:

            if (read_byte == I0 || read_byte == I1)
            {
                writer_nlocal = read_byte;
                state = 3;
            }
            else if (read_byte == FLAG)
                state = 1;
            else
                state = 0;

            break;
        case 3:
            if (read_byte == (A_SENDER ^ writer_nlocal))
                state = 4;
            else if (read_byte == FLAG)
                state = 1;
            else
                state = 0;
            break;
        case 4:
            if (read_byte == FLAG)
            {
                if (bcc2 == 0x00)
                {
                    packet[pos - 1] = '\0';
                    STOP = TRUE;

                    if (N_local == I0 && writer_nlocal == I0)
                    {
                        new_packet = TRUE;
                        response = RR1;
                        N_local = I1;
                    }
                    else if (N_local == I1 && writer_nlocal == I1)
                    {
                        new_packet = TRUE;
                        response = RR0;
                        N_local = I0;
                    }
                    else if (N_local == I0 && writer_nlocal == I1)
                    {
                        response = RR0;
                    }
                    else if (N_local == I1 && writer_nlocal == I0)
                    {
                        response = RR1;
                    }
                }
                else
                {
                    printf("Error in BCC2\n");
                    if (N_local == I0)
                        response = REJ0;
                    else if (N_local == I1)
                        response = REJ1;
                    pos = 0;
                    STOP = TRUE;
                }
            }
            else if (escapeRead)
            {
                if (read_byte == 0x5D)
                {
                    packet[pos] = ESC;
                    bcc2 = bcc2 ^ ESC;
                }
                else if (read_byte == 0x5E)
                {
                    packet[pos] = FLAG;
                    bcc2 = bcc2 ^ FLAG;
                }
                escapeRead = FALSE;
                pos++;
            }
            else if (read_byte == ESC)
            {
                escapeRead = TRUE;
            }
            else
            {
                packet[pos] = read_byte;
                bcc2 = bcc2 ^ read_byte;
                pos++;
            }

            break;
        default:
            break;
        }
    }

    unsigned char write_buf[5] = {0};
    write_buf[0] = FLAG;
    write_buf[1] = A_RECEIVER;
    write_buf[2] = response;
    write_buf[3] = A_RECEIVER ^ response;
    write_buf[4] = FLAG;

    write(fd, write_buf, 5);
    return new_packet;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    printf("Starting closing procedures...\n");
    state = 0;
    nReTransmissions = 0;
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

        while (nReTransmissions < maxNReTransmissions && state != 5)
        {
            if (alarmEnabled == FALSE)
            {
                STOP = FALSE;
                write(fd, write_buf, 5);
                alarm(timeout);
                alarmEnabled = TRUE;

                unsigned char read_byte;
                while (STOP == FALSE)
                {
                    read(fd, &read_byte, 1);
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
                        else if (read_byte == (A_RECEIVER ^ DISC))
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
            read(fd, &read_byte, 1);
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
                else if (read_byte == (A_SENDER ^ DISC))
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
        nReTransmissions = 0;
        alarmEnabled = FALSE;
        STOP = FALSE;

        while (STOP == FALSE)
        {
            read(fd, &read_byte, 1);
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
                else if (read_byte == (A_SENDER ^ UA))
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

    gettimeofday(&end, 0);
    long seconds = end.tv_sec - begin.tv_sec;
    long microseconds = end.tv_usec - begin.tv_usec;
    double timeElapsed = seconds + microseconds * 1e-6;

    if (showStatistics == TRUE)
    {
        printf("Time elapsed: %f seconds\n", timeElapsed);
        if (linkLayerRole == LlTx)
        {
            printf("Total alarms activated: %d\n", totalAlarms);
            printf("Total frame rejections: %d\n", totalReTransmissions - totalAlarms);
            printf("Total re-transmissions: %d\n", totalReTransmissions);
            printf("Total bytes sent: %d bytes\n", totalBytesSent);
        }
        else if (linkLayerRole == LlRx)
            printf("Total data bytes received: %d bytes\n", totalDataBytesReceived);
    }

    close(fd);

    return nReTransmissions < maxNReTransmissions;
}
