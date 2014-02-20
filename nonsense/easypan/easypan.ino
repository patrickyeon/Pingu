#include <Arduino.h>

#define DEBUG_CMD_RESPONSES

#ifdef DEBUG_CMD_RESPONSES
int _i;
#define LOG_BYTES(BUFF, LEN)\
    for(_i = 0; _i < LEN; _i++) {\
        Serial.print(BUFF[_i], HEX);\
        Serial.print(' ');\
    }\
    Serial.println();
#else
#define LOG_BYTES(BUFF, LEN) {}
#endif // DEBUG_CMD_RESPONSES

#define gimbal Serial2

const int RX_BUFF_SZ = 300;

byte rxBuff[RX_BUFF_SZ];

void setup() {
    Serial.begin(57600);
    long speed = getLink();
    if (speed > 0) {
        Serial.print("connected to gimbal with UART speed ");
        Serial.println(speed);
    } else {
        Serial.println("failed to connect to gimbal");
        while (1) {}
    }
}

void loop() {
    for (int i = -450; i <= 450; i += 30) {
        setPitch(i);
        Serial.print("angle: ");
        Serial.println(i);
        delay(33);
    }
    for (int i = 450; i >= -450; i -= 15) {
        setPitch(i);
        Serial.print("angle: ");
        Serial.println(i);
        delay(33);
    }
}

long getLink() {
    long speeds[5] = {115200, 57600, 38400, 28800, 19200};
    int i;
    bool found = false;
    for (i = 0; i < 5; i++) {
        gimbal.begin(speeds[i]);
        int byteCnt = getVersion();
        if (byteCnt == 5 + 6 + 12 && rxBuff[0] == '>' && rxBuff[1] == 'V') {
            found = true;
            break;
        }
    }

    if (found) {
        return speeds[i];
    } else {
        return -1;
    }
}

int getVersion() {
    byte command[5] = {'>', 'V', 0, 'V', 0};
    int retLen = sendCmd(command, 5, 2000);
    if (retLen > 0) {
        LOG_BYTES(rxBuff, retLen)
    }
    return retLen;
}

int sendCmd(byte *cmd, int cmdLen, int timeout) {
    // flush out any unhandled incoming data from the gimbal
    while (gimbal.available()) {
        gimbal.read();
    }

    gimbal.write(cmd, cmdLen);
    long start = millis();
    while (millis() - start < timeout && !(gimbal.available())) {
        delay(20);
    }

    if (!gimbal.available()) {
        // timeout
        return -1;
    }

    int i;
    for (i = 0; gimbal.available() && i < RX_BUFF_SZ; i++) {
        rxBuff[i] = gimbal.read();
    }
    
    return i;
}

void setPitch(int angle) {
    byte buff[5 + 13] = {'>', 'C', 13, 'C' + 13, 2, 0};
    buff[4 + 11] = (byte)(angle & 0xff);
    buff[4 + 12] = (byte)((angle & 0xff00) >> 8);
    buff[4 + 13] = (byte)((2 + buff[4 + 11] + buff[4 + 12]) & 0xff);
    
    sendCmd(buff, 5 + 13, 0);
}
