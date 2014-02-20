#include <Arduino.h>

//#define DEBUG

typedef enum state_t {
    WAITING = 0,
    PART_HEADER,
    GOT_HEADER,
    PART_PAYLOAD,
    GOT_PAYLOAD,
    INVALID
} state_t;

state_t state;
int buff[300];
int bidx;
int dataRemain;

void setup() {
    Serial.begin(115200);
    
    for (int i = 0; i < 300; i++) {
        buff[i] = 0;
    }
    bidx = 0;
    dataRemain = 0;

    state = WAITING;
}

void loop() {
    if (Serial.available()) {
        rxInput();
    } else {
        delay(20);
    }
}

void rxInput() {
#ifdef DEBUG
    Serial.print("In state ");
    Serial.println(state);
#endif // DEBUG
    switch (state) {
    case WAITING:
        bidx = 0;
    case PART_HEADER:
        while (Serial.available() && bidx < 4) {
            buff[bidx++] = Serial.read();
        }
        if (bidx == 4) {
            state = GOT_HEADER;
        } else {
            state = PART_HEADER;
        }
        break;
        
    case GOT_HEADER:
        // do the header checksum
        if ((buff[1] + buff[2]) % 256 != buff[3]) {
            while (Serial.available()) {
                Serial.read();
            }
            state = WAITING;
            break;
        }

        dataRemain = buff[2];
        state = PART_PAYLOAD;
    case PART_PAYLOAD:
        while (Serial.available() && dataRemain > 0) {
            buff[bidx++] = Serial.read();
            dataRemain--;
        }

        if (dataRemain == 0) {
            state = GOT_PAYLOAD;
        }
        break;
    case GOT_PAYLOAD:
        buff[bidx] = Serial.read();
        if (buff[bidx++] != checkPacket(buff + 4, buff[2])) {
            while (Serial.available()) {
                Serial.read();
            }
            state = WAITING;
            break;
        }

        runCommand();
        state = WAITING;
        break;
    default:
        while (Serial.available()) {
            Serial.read();
        }
        state = WAITING;
    }
}

void runCommand() {
    // for now, just confirm it
    byte confbuff[6] = {'>', 'C', 1, 'C' + 1, buff[1], buff[1]};
    Serial.write(confbuff, 6);
}

int checkPacket(int *buffer, int len) {
    int sum = 0;
    for (int i = 0; i < len; i++) {
        sum += buffer[i];
    }
    return sum % 256;
}
