#include "pingu.h"
#include <string.h>
#include <stdlib.h>

static le16_t mkLEint(int val) {
    return (le16_t){(byte)(val & 0xff), (byte)((val & 0xff00) >> 8)};
}

static unsigned int read_uint(byte *from) {
    return (unsigned int)(from[0]) | (unsigned int)(from[1] << 8);
}

static byte checksumPacket(byte *buff) {
    int len = buff[2];
    int sum = 0;
    for (int i = 0; i < len; i++) {
        sum += buff[4 + i];
    }
    return (byte)(sum % 256);
}

int mkPacket(byte command, byte *data, size_t data_size, byte *buffer) {
    // check that the data fits inside the BGC's buffer
    if (data_size > MAX_PAYLOAD) {
        return -1;
    }

    buffer[0] = '>';
    buffer[1] = command;
    buffer[2] = (byte)data_size;
    buffer[3] = (byte)((data_size + command) % 256); 
    
    int chksum = 0;
    memcpy(buffer + 4, data, data_size);
    for (unsigned int i = 0; i < data_size; i++) {
        chksum += data[i];
    }

    buffer[4 + data_size] = (byte)(chksum % 256);

    return 4 + (int)data_size + 1;
}

void mkCtrlPayload(ctrl_mode_t mode,
        int yawSpeed, int yawAngle, int pitchSpeed, int pitchAngle,
        ctrl_payload_t *buff) {
    buff->mode = mode;
    buff->rollSpeed = mkLEint(0);
    buff->rollAngle = mkLEint(0);
    buff->yawSpeed = mkLEint(yawSpeed);
    buff->yawAngle = mkLEint(yawAngle % 7200);
    buff->pitchSpeed = mkLEint(pitchSpeed);
    buff->pitchAngle = mkLEint(pitchAngle % 7200);
}


void setAngle(int yaw, int pitch) {
    ctrl_payload_t payload;
    mkCtrlPayload(MODE_ANGLE, 0, yaw, 0, pitch, &payload);
    sendPacket('C', sizeof(payload), (byte*)(&payload));
    return;
}

void setSpeed(int yaw, int pitch) {
    ctrl_payload_t payload;
    mkCtrlPayload(MODE_SPEED, yaw, 0, pitch, 0, &payload);
    sendPacket('C', sizeof(payload), (byte*)(&payload));
    return;
}

int readVersion(vers_data_t *buffer) {
    byte *data = (byte *)malloc(PKT_OVERHEAD + MAX_PAYLOAD);
    serialPort.read(data);
    if (data[0] != '>') {
        free(data);
        return -EBADPRELUDE;
    }
    if (data[1] != 'V' || data[2] != 18 || data[3] != 'V' + 18) {
        free(data);
        return -EHEADERERROR;
    }
    if (data[data[2] + PKT_OVERHEAD - 1] != checksumPacket(data)) {
        free(data);
        return -EBADCHECKSUM;
    }

    buffer->boardMaj = data[4] / 10;
    buffer->boardMin = data[4] % 10;

    unsigned int fw = read_uint(data + 5);
    buffer->fwMaj = fw / 1000;
    buffer->fwMin = (fw % 1000) / 10;
    buffer->fwRev = fw % 10;

    buffer->debug = data[7] ? 1 : 0;
    buffer->threeAxis = data[8] & 1 ? 1 : 0;
    buffer->batMon = data[8] & 2 ? 1 : 0;

    free(data);
    return 0;
}

int setPID(int p, int i, int d, axis_ctrl_t *axis) {
    if (p < 0 || 50 < p || i < 0 || 50 < i || d < 0 || 50 < d) {
        return -EBADRANGE;
    }

    axis->p = (byte)p;
    axis->i = (byte)i;
    axis->d = (byte)d;

    return 0;
}

int getProfile(profile_id_t id, profile_t *buff) {
    byte *tmp = malloc(PKT_OVERHEAD + MAX_PAYLOAD);
    serialPort.read(tmp);

    if (tmp[0] != '>') {
        free(tmp);
        return -EBADPRELUDE;
    }
    if (tmp[1] != 'R' || tmp[2] != sizeof(profile_t)
            || tmp[3] != (byte)(('R' + sizeof(profile_t)) % 256)) {
        free(tmp);
        return -EHEADERERROR;
    }
    if (tmp[tmp[2] + PKT_OVERHEAD - 1] != checksumPacket(tmp)) {
        free(tmp);
        return -EBADCHECKSUM;
    }

    memcpy(buff, tmp + 4, sizeof(profile_t));
    free(tmp);
    return 0;
}
