#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include "CppUTest/TestHarness.h"
#include "CppUTest/TestPlugin.h"
#include "CppUTest/SimpleString.h"
#include "uartlinux.h"

static SimpleString _E, _R;

#define CHECK_BUFF(EXP, RET, I)\
    _E = stringFrom(EXP, I);\
    _R = stringFrom(RET, I);\
    CHECK_EQUAL(_E, _R);

#define CHECK_NULL_BUFF(B, I) \
    for (size_t i = 0; i < I; i++) {\
        CHECK_EQUAL(0, B[i]);\
    }

static byte *buff;
static byte *cmdBuff;
static bool cmdReceived;
static byte *rxBuff;
static int rxStart, rxEnd;

static SimpleString stringFrom(byte *buffer, size_t len) {
    char *hexBuff = (char *)malloc(len * 3 + 1);
    for (unsigned int i = 0; i < len; i++) {
        sprintf(hexBuff + 3 * i, "%02x ", buffer[i]);
    }
    SimpleString retval = SimpleString(hexBuff);
    free(hexBuff);
    return retval;
}

static int cmdLogger(byte *buffer, size_t len) {
    memcpy(cmdBuff, buffer, len);
    cmdReceived = true;
    return (int)len;
}

static byte *getLastCmd(void) {
    // TODO log and return how many bytes were sent
    if (cmdReceived) {
        return cmdBuff;
    } else {
        return NULL;
    }
}

static int rxFaker(unsigned int nchar, unsigned int timeout, byte *buffer) {
    if (timeout) {
        // fuck I dunno, just to make the compiler happy for now
        return -EUNKNOWN;
    }

    if (nchar > (unsigned int)(rxEnd - rxStart)) {
        return -EUNKNOWN;
    }

    memcpy(buffer, rxBuff + rxStart, nchar);
    rxStart += (int)nchar;
    
    return (int)nchar;
}

static int rxFlush(void) {
    int len = rxEnd - rxStart;
    rxStart = rxEnd;
    return len;
}

TEST_GROUP(uart) {
    void setup() {
        buff = (byte *)calloc(2 * MAX_PAYLOAD, sizeof(byte));
        cmdBuff = (byte *)calloc(2 * (PKT_OVERHEAD + MAX_PAYLOAD),
                sizeof(byte));
        cmdReceived = false;
        UT_PTR_SET(serialPort.write, &cmdLogger);

        rxBuff = (byte *)calloc(2 * (PKT_OVERHEAD + MAX_PAYLOAD), sizeof(byte));
        rxStart = rxEnd = 0;
        UT_PTR_SET(serialPort.foo, &rxFaker);
        UT_PTR_SET(serialPort.flush, &rxFlush);
    }

    void teardown() {
        free(buff);
        free(cmdBuff);
        cmdReceived = false;

        free(rxBuff);
    }
};

static void mkStar(void) {
    if (rxEnd + 5 > 2 * (PKT_OVERHEAD + MAX_PAYLOAD)) {
        FAIL("Overflowing rxBuff");
    }
    rxBuff[rxEnd++] = '>';
    rxBuff[rxEnd++] = '*';
    rxBuff[rxEnd++] = 0;
    rxBuff[rxEnd++] = '*';
    rxBuff[rxEnd++] = 0;
}


TEST(uart, canCreateEmptyPacket) {
    int numBytes = sendPacket('V', 0, NULL);
    CHECK_EQUAL(5, numBytes);
    byte *result = getLastCmd();
    byte expected[5] = {'>', 'V', 0, 'V', 0};
    CHECK_BUFF(expected, result, 5);
}

TEST(uart, canCreateMaxSizePacket) {
    for (int i = 0; i < MAX_PAYLOAD; i++) {
        buff[i] = (byte)(i % 0x100);
    }
    int numBytes = sendPacket('*', MAX_PAYLOAD, buff);
    CHECK_EQUAL(MAX_PAYLOAD + PKT_OVERHEAD, numBytes);

    byte *result = getLastCmd();
    byte header[4] = {'>', '*', MAX_PAYLOAD, (byte)(('*' + MAX_PAYLOAD) % 256)};
    CHECK_BUFF(header, result, 4);
    CHECK_BUFF(buff, result + 4, MAX_PAYLOAD);
    CHECK_EQUAL(129, result[MAX_PAYLOAD + PKT_OVERHEAD - 1]);
}

TEST(uart, cannotCreateOversizePacket) {
    int numBytes = sendPacket('V', MAX_PAYLOAD + 1, buff);
    CHECK_EQUAL(-EBADRANGE, numBytes);
}

TEST(uart, canRxEmptyPacket) {
    mkStar();
    int err = readPacket('*', buff);
    CHECK_EQUAL(0, err);
    // make sure the whole packet's been moved out of the buffer
    CHECK_EQUAL(rxStart, rxEnd);
}

TEST(uart, cantRxBadPrelude) {
    mkStar();
    rxBuff[0] = '<';
    rxEnd += 5;
    int err = readPacket('*', buff);
    CHECK_EQUAL(-EHEADERERROR, err);
    // make sure the buffer gets flushed out if there's bad data in it
    CHECK_EQUAL(rxStart, rxEnd);
}

TEST(uart, willSkipWrongCommand) {
    mkStar();
    mkStar();
    rxBuff[1] = '$';
    rxBuff[3] = '$';
    int err = readPacket('*', buff);
    CHECK_EQUAL(0, err);

    CHECK_EQUAL(rxStart, rxEnd);
}

TEST(uart, willErrorBadHeaderChecksum) {
    mkStar();
    rxBuff[3]++;
    int err = readPacket('*', buff);
    CHECK_EQUAL(-EBADCHECKSUM, err);
    CHECK_EQUAL(rxStart, rxEnd);
}

TEST(uart, willErrorBadDataChecksum) {
    mkStar();
    rxBuff[2] = 9;
    rxBuff[3] = (byte)(rxBuff[3] + 9);
    for (int i = 1; i < 10; i++) {
        rxBuff[3 + i] = (byte)i;
    }
    rxEnd += 9;

    int err = readPacket('*', buff);
    CHECK_EQUAL(-EBADCHECKSUM, err);
    CHECK_EQUAL(rxStart, rxEnd);
}

TEST(uart, willPassGoodChecksum) {
    mkStar();
    rxBuff[2] = 9;
    rxBuff[3] = (byte)(rxBuff[3] + 9);
    for (int i = 1; i < 10; i++) {
        rxBuff[3 + i] = (byte)i;
    }
    rxBuff[9 + PKT_OVERHEAD - 1] = 45;
    rxEnd += 9;

    int err = readPacket('*', buff);
    CHECK_EQUAL(0, err);
    CHECK_EQUAL(rxStart, rxEnd);
}

#ifdef MOCKBIRD
TEST_GROUP(harduart) {
    void setup() {
    }

    void teardown() {
    }
};

TEST(harduart, cantalk) {
    int err = sendPacket('*', 0, NULL);
    CHECK_EQUAL(5, err);
    usleep(100000);
    byte buffer[10] = {0};
    err = readPacket('C', buffer);
    CHECK_EQUAL(0, err);
    CHECK_EQUAL('*', buffer[0]);
}

#endif // MOCKBIRD

