#ifndef GUARD_UARTLINUX_H
#define GUARD_UARTLINUX_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include <termios.h>
#include "bgcdefs.h"

// 4 bytes header, 1 byte checksum
#define PKT_OVERHEAD (4 + 1)
#define MAX_PAYLOAD 255
#define UART_SPEED B115200

#ifndef UART_PORT
#define UART_PORT "/dev/tty.usbserial-FTF7LHGW"
//#define UART_PORT "/dev/tty.usbmodem141131"
#endif // def UART_PORT

extern struct serial {
    // Sends the buffer contents across the serial interface for the gimbal
    // controller board. Returns the number of bytes written. Assumes a
    // well-formed packet begins at buffer[0].
    int (*write)(byte *buffer, size_t len);

    // flushes out any data in the receive buffer. Returns the number of bytes
    // cleared, or negative value on error.
    int (*flush)(void);

    // reads a packet into buffer. buffer MUST be at least PKT_OVERHEAD +
    // MAX_PAYLOAD bytes long. returns the number of bytes read, or negative
    // value on error.
    int (*read)(byte *buffer);

    int (*foo)(unsigned int nchar, unsigned int timeout, byte *buffer);

    struct termios term;

    int fd;
} serialPort;

int sendPacket(byte cmd, size_t datalen, byte *buff);
int readPacket(byte cmd, byte *buff);
int readAnyPacket(byte *buff);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // GUARD_UARTLINUX_H
