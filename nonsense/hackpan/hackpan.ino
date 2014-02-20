#include <Arduino.h>

#define DEBUG

#ifdef DEBUG
int _i;
#define LOG_BYTES(BUFF, LEN)\
    for(_i = 0; _i < LEN; _i++) {\
        Serial.print(BUFF[_i], HEX);\
        Serial.print(' ');\
    }\
    Serial.println();
#else
#define LOG_BYTES(BUFF, LEN) {}
#endif // DEBUG

#define gimbal Serial2
#define CHECK(MSG)\
    if ( i >= rxIdx) {\
        Serial.println(MSG);\
        return;\
    }


const int RX_BUFF_SZ = 300;

byte rxBuff[RX_BUFF_SZ];
int rxIdx;

void setup() {
    Serial.begin(57600);
    rxIdx = 0;
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
    while(Serial.available() && rxIdx < RX_BUFF_SZ) {
        rxBuff[rxIdx] = Serial.read();
        if (rxBuff[rxIdx++] == ';') {
            rxBuff[rxIdx] = '\0';
#ifdef DEBUG
            Serial.println((char *)rxBuff);
#endif // DEBUG
            runCommand();
            rxIdx = 0;
        }
    }

    if (rxIdx >= RX_BUFF_SZ) {
        // overflow, so flush it out
        while(Serial.available()) {
            Serial.read();
        }
        rxIdx = 0;
    }

    delay(20);
}

void runCommand() {
    // command is in form "a [+|-]pppp [+|-]yyyy;"
    int i = 0;
    while (rxBuff[i++] != 'a') {
        CHECK("cannot find a")
    }
    CHECK("overrun after a")
    // rxBuff[i] is two characters after a

    int pitch=0, yaw=0;
    bool negative = true;

    byte b = rxBuff[++i];
    if (!(b == '+' || b == '-')) {
#ifdef DEBUG
        Serial.println("no sign for pitch");
#endif // DEBUG
        return;
    } else {
        negative = (b == '-');
    }

    while (rxBuff[++i] != ' ' && i < rxIdx) {
        b = rxBuff[i];
        if (b < '0' || b > '9') {
#ifdef DEBUG
            Serial.println("pitch outta range");
#endif // DEBUG
            return;
        }
        pitch = (10 * pitch) + (b - '0');
    }

    CHECK("overrun after pitch");

    pitch %= 7200;
    pitch *= (negative ? -1 : 1);

    b = rxBuff[++i];
    if (!(b == '+' || b == '-')) {
#ifdef DEBUG
        Serial.println("no sign for yaw");
#endif // DEBUG
        return;
    } else {
        negative = (b == '-');
    }

    while (rxBuff[++i] != ';' && i < rxIdx) {
        b = rxBuff[i];
        if (b < '0' || b > '9') {
#ifdef DEBUG
            Serial.println("yaw outta range");
#endif // DEBUG
            return;
        }
        yaw = (10 * yaw) + (b - '0');
    }

    yaw %= 7200;
    yaw *= (negative ? -1 : 1);

#ifdef DEBUG
    Serial.print("sent command pitch: ");
    Serial.print(pitch);
    Serial.print(" yaw: ");
    Serial.println(yaw);
#endif // DEBUG

    setAngle(pitch, yaw);
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

void setAngle(int pitch, int yaw) {
    byte buff[5 + 13] = {'>', 'C', 13, 'C' + 13, 2, 0};
    buff[4 + 7]  = (byte)(pitch & 0xff);
    buff[4 + 8]  = (byte)((pitch & 0xff00) >> 8);
    buff[4 + 11] = (byte)(yaw & 0xff);
    buff[4 + 12] = (byte)((yaw & 0xff00) >> 8);

    buff[4 + 13] = (byte)((2 + buff[4 + 7] + buff[4 + 8] +
                           buff[4 + 11] + buff[4 + 12]) & 0xff);
    
    sendCmd(buff, 5 + 13, 0);
}
