#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "uartlinux.h"

#ifdef DEBUG_LOGGING
#include <stdio.h>
#define ERRLOG(X) perror(X)
#define LOG(...) fprintf(stderr, __VA_ARGS__)
#else
#define ERRLOG(X) do {} while (0)
#define LOG(...) do {} while (0)
#endif // DEBUG_LOGGING

// UART startup time in us
#define STARTUPTIME 2000000

static byte txBuff[PKT_OVERHEAD + MAX_PAYLOAD];
int serialFlush(void);

static int serialInit(void) {
    struct termios term = serialPort.term;
    memset(&term, 0, sizeof(term));
    term.c_cflag = (CS8 | CREAD | CLOCAL); // 8n1
    term.c_cc[VMIN] = 1;
    term.c_cc[VTIME] = 5;

    serialPort.fd = 0;
    serialPort.fd = open(UART_PORT, O_RDWR | O_NONBLOCK);
    if (serialPort.fd < 0) {
        return -ETTYERROR;
    }
    LOG("port opened on %s\n", UART_PORT);

    cfsetospeed(&term, UART_SPEED);
    cfsetispeed(&term, UART_SPEED);

    tcsetattr(serialPort.fd, TCSANOW, &term);

    usleep(STARTUPTIME);
    serialFlush();
    return 0;
}

static int trySerialInit(void) {
    if (serialPort.fd >= 0) {
        return 0;
    }
    return serialInit();
}

int serialWrite(byte *buffer, size_t len) {
    int err = trySerialInit();
    if (err != 0) {
        ERRLOG("write");
        return err;
    }

    return (int)write(serialPort.fd, buffer, len);
}

int serialFlush(void) {
    int err = trySerialInit();
    if (err != 0) {
        ERRLOG("flush");
        return err;
    }

    int count = 0;
    byte c;
    while (read(serialPort.fd, &c, 1) > 0) {
        count++;
    }

    return count;
}

int serialRead(unsigned int nchar, unsigned int timeout, byte *buffer) {
    int err = trySerialInit();
    if (err != 0) {
        return err;
    }

    serialPort.term.c_cc[VMIN] = (cc_t)nchar;
    serialPort.term.c_cc[VTIME] = (cc_t)timeout;
    tcsetattr(serialPort.fd, TCSANOW, &(serialPort.term));

    return (int)read(serialPort.fd, buffer, nchar);
}

struct serial serialPort = {&serialWrite, &serialFlush, NULL, &serialRead,
                            {0}, -1};

static byte checksumPayload(byte *buff, size_t datalen) {
    int sum = 0;
    for (unsigned int i = 0; i < datalen; i++) {
        sum += buff[i];
    }
    return (byte)(sum % 256);
}

int sendPacket(byte cmd, size_t datalen, byte *buff) {
    if (datalen > 255) {
        return -EBADRANGE;
    }

    txBuff[0] = '>';
    txBuff[1] = cmd;
    txBuff[2] = (byte)datalen;
    txBuff[3] = (byte)((cmd + datalen) % 256);

    memcpy(txBuff + 4, buff, datalen);

    txBuff[datalen + 4] = checksumPayload(buff, datalen);

    int err = serialPort.write(txBuff, datalen + PKT_OVERHEAD);
    if (err < 0) {
        ERRLOG("serial write");
        return err;
    }

    LOG("Sent Packet: ");
    for (int i = 0; i < err; i++) {
        LOG("%02x ", txBuff[i]);
    }
    LOG("\n");

    return (int)(datalen + PKT_OVERHEAD);
}

int readHeader(byte *buff, byte *datalen) {
    byte buffer[5];
    int err = serialPort.foo(4, 0, buffer);
    if (err < 0) {
        ERRLOG("header");
        return err;
    }
    if (buffer[0] != '>') {
        // once the data gets ugly, just throw it all out
        serialPort.flush();
        return -EHEADERERROR;
    }
    if (buffer[3] != (byte)((buffer[1] + buffer[2]) % 256)) {
        serialPort.flush();
        return -EBADCHECKSUM;
    }
    
    *datalen = buffer[2];
    memcpy(buff, buffer, 4);

    return 0;
}

int readAnyPacket(byte *buff) {
    byte buffer[MAX_PAYLOAD + PKT_OVERHEAD];
    byte datalen;

    int err = readHeader(buffer, &datalen);
    if (err < 0) {
        return err;
    }

    err = serialPort.foo((unsigned int)datalen, 0, buffer + 4);
    if (err < 0) {
        ERRLOG("data");
        return err;
    }

    err = serialPort.foo(1, 0, buffer + 4 + datalen);
    if (err < 0) {
        ERRLOG("checksum");
        return err;
    }

    if (checksumPayload(buffer + 4, (size_t)datalen) != buffer[4 + datalen]) {
        serialPort.flush();
        return -EBADCHECKSUM;
    }

    memcpy(buff, buffer, (size_t)datalen + PKT_OVERHEAD);
    return 0;
}

int readPacket(byte cmd, byte *buff) {
    byte buffer[MAX_PAYLOAD + PKT_OVERHEAD];
    byte readCmd = 0;

    while(readCmd != cmd) {
        int err = readAnyPacket(buffer);
        if (err < 0) {
            return err;
        }
        readCmd = buffer[1];
    }

    memcpy(buff, buffer + 4, (size_t)(buffer[2]));
    return 0;
}
