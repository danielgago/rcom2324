// Write to serial port in non-canonical mode
//
// Modified by: Eduardo Nuno Almeida [enalmeida@fe.up.pt]

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>

// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

#define FALSE 0
#define TRUE 1
#define FLAG 0x7E
#define A_SENDER 0x03
#define A_RECEIVER 0x01
#define UA 0x07
#define SET 0x03
#define RR0 0x05
#define RR1 0x85
#define REJ0 0x01
#define REJ1 0x81
#define DISC 0x0B

int state = 0;

#define BUF_SIZE 512

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

void stablish_connection(int fd){
    unsigned char write_buf[BUF_SIZE] = {0};
    write_buf[0] = FLAG;
    write_buf[1] = A_SENDER;
    write_buf[2] = SET;
    write_buf[3] = A_SENDER^SET;
    write_buf[4] = FLAG;
    
    (void)signal(SIGALRM, alarmHandler);

    while (alarmCount < 4){
        if (alarmEnabled == FALSE)
        {
            STOP = FALSE;
            int bytes = write(fd, write_buf, BUF_SIZE);
            printf("%d bytes written\n", bytes);
            alarm(3); // Set alarm to be triggered in 3s
            alarmEnabled = TRUE;

            // Wait until all bytes have been written to the serial port
            sleep(1);

            unsigned char read_buf[BUF_SIZE + 1] = {0};
            while (STOP == FALSE){
                // Returns after 5 chars have been input
                int bytes = read(fd, read_buf, 1);
                printf("var = 0x%02X\n", read_buf[0]);
                switch (state)
                {
                case 0:
                    if(read_buf[0] == FLAG) state = 1;
                    else state = 0;
                    break;
                case 1:
                    if(read_buf[0] == FLAG) state = 1;
                    else if(read_buf[0] == A_RECEIVER) state = 2;
                    else state = 0;
                    break;
                case 2:
                    if(read_buf[0] == FLAG) state = 1;
                    else if(read_buf[0] == UA) state = 3;
                    else state = 0;
                    break;
                case 3:
                    if(read_buf[0] == FLAG) state = 1;
                    else if(read_buf[0] == A_RECEIVER^UA) state = 4;
                    else state = 0;
                    break;
                case 4:
                    if(read_buf[0] == FLAG) {
                        STOP = TRUE;
                        printf("Success!");
                        alarm(0);
                        alarmCount = 4;
                    }
                    else state = 0;
                    break;
                default:
                    break;
                }
            }
        }
    }
}

void write_data(int fd){
    
}


int main(int argc, char *argv[])
{
    // Program usage: Uses either COM1 or COM2
    const char *serialPortName = argv[1];

    if (argc < 2)
    {
        printf("Incorrect program usage\n"
               "Usage: %s <SerialPort>\n"
               "Example: %s /dev/ttyS1\n",
               argv[0],
               argv[0]);
        exit(1);
    }

    // Open serial port device for reading and writing, and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    int fd = open(serialPortName, O_RDWR | O_NOCTTY);

    if (fd < 0)
    {
        perror(serialPortName);
        exit(-1);
    }

    struct termios oldtio;
    struct termios newtio;

    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }

    // Clear struct for new port settings
    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0.1; // Inter-character timer unused
    newtio.c_cc[VMIN] = 0;  // Blocking read until 5 chars received

    // VTIME e VMIN should be changed in order to protect with a
    // timeout the reception of the following character(s)

    // Now clean the line and activate the settings for the port
    // tcflush() discards data written to the object referred to
    // by fd but not transmitted, or data received but not read,
    // depending on the value of queue_selector:
    //   TCIFLUSH - flushes data received but not read.
    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");

    stablish_connection(fd);

    write_data(fd);

    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}
