#include <string.h>
#include <stdio.h>
#include "CppUTest/TestHarness.h"
#include "CppUTest/TestPlugin.h"
#include "CppUTest/SimpleString.h"
#include "pingu.h"

static SimpleString _E, _R;

#define CHECK_BUFF(EXP, RET, I)\
    _E = stringFrom(EXP, I);\
    _R = stringFrom(RET, I);\
    CHECK_EQUAL(_E, _R);

#define CHECK_NULL_BUFF(B, I) \
    for (size_t i = 0; i < I; i++) {\
        CHECK_EQUAL(0, B[i]);\
    }

static byte *cmdBuff;
static bool cmdReceived;
static byte *buff;
static byte *rxBuff;

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

static int rxFaker(byte *buffer) {
    int len = rxBuff[2] + PKT_OVERHEAD;
    memcpy(buffer, rxBuff, (size_t)len);
    return len;
}

TEST_GROUP(pingu) {
    void setup() {
        buff = (byte *)calloc(PKT_OVERHEAD + MAX_PAYLOAD, sizeof(byte));
        cmdBuff = (byte *)calloc(PKT_OVERHEAD + MAX_PAYLOAD, sizeof(byte));
        rxBuff = (byte *)calloc(PKT_OVERHEAD + MAX_PAYLOAD, sizeof(byte));
        cmdReceived = false;
        UT_PTR_SET(serialPort.write, &cmdLogger);
        UT_PTR_SET(serialPort.read, &rxFaker);
    }

    void teardown() {
        free(buff);
        free(cmdBuff);
        free(rxBuff);
        cmdReceived = false;
    }
};

TEST(pingu, testsrun) {
    CHECK_TEXT(1, "OK for testing.");
}

TEST(pingu, canCreateEmptyPacket) {
    int numBytes = mkPacket('V', NULL, 0, buff);
    CHECK_EQUAL(5, numBytes);
    byte expected[5] = {'>', 'V', 0, 'V', 0};
    CHECK_BUFF(expected, buff, 5);
}

TEST(pingu, cannotCreateOversizePacket) {
    int numBytes = mkPacket('V', NULL, MAX_PAYLOAD + 1, buff);
    CHECK_EQUAL(-1, numBytes);
}

TEST(pingu, controlYawAngle) {
    setAngle(200, 0);
    byte *cmd = getLastCmd();
    CHECK_TRUE(cmd != NULL);
    byte len = (byte)sizeof(ctrl_payload_t);
    byte expected[18] = {'>', 'C', len, (byte)('C' + len),
        2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 200, 0, 202};
    CHECK_BUFF(expected, cmd, 18);
}

TEST(pingu, controlPitchAngle) {
    setAngle(0, 1995);
    byte *cmd = getLastCmd();
    CHECK_TRUE(cmd != NULL);
    byte len = (byte)sizeof(ctrl_payload_t);
    byte expected[18] = {'>', 'C', len, (byte)('C' + len),
        2, 0, 0, 0, 0, 0, 0, (1995 % 0x100), (1995 / 0x100), 0, 0, 0, 0,
        (2 + (1995 % 0x100) + (1995 / 0x100)) % 256};
    CHECK_BUFF(expected, cmd, 18);
}

TEST(pingu, controlYawSpeed) {
    setSpeed(20, 0);
    byte *cmd = getLastCmd();
    CHECK_TRUE(cmd != NULL);
    byte len = (byte)sizeof(ctrl_payload_t);
    byte expected[18] = {'>', 'C', len, (byte)('C' + len),
        1, 0, 0, 0, 0, 0, 0, 0, 0, 20, 0, 0, 0, (1 + 20) % 256};
    CHECK_BUFF(expected, cmd, 18);
}

TEST(pingu, controlPitchSpeed) {
    int setVal = 0xff - 20;
    byte setHi = (byte)(setVal / 0x100);
    byte setLo = (byte)(setVal % 0x100);
    setSpeed(0, setVal);
    byte *cmd = getLastCmd();
    CHECK_TRUE(cmd != NULL);
    byte len = (byte)sizeof(ctrl_payload_t);
    byte expected[18] = {'>', 'C', len, (byte)('C' + len),
        1, 0, 0, 0, 0, setLo, setHi, 0, 0, 0, 0, 0, 0,
        (byte)((1 + setLo + setHi) % 256)};
    CHECK_BUFF(expected, cmd, 18);
}

static vers_data_t versData;
const static byte *pVersData = (byte *)(&versData);

TEST_GROUP(versionDetails) {
    void setup() {
        rxBuff = (byte *)calloc(PKT_OVERHEAD + MAX_PAYLOAD, sizeof(byte));
        UT_PTR_SET(serialPort.read, &rxFaker);

        byte foo[10] = {'>', 'V', 18, 'V' + 18,
            98, (7654 % 0x100), (7654 / 0x100), 0, 1 | 2, 0};
        for (int i = 0; i < 10; i++) {
            rxBuff[i] = foo[i];
        }
        // create the checksum
        for (int i = 4; i < 22; i++) {
            rxBuff[4 + 18] = (byte)(rxBuff[4 + 18] + rxBuff[i]);
        }

        versData = vers_data_t();
    }

    void teardown() {
        free(rxBuff);
    }
};

TEST(versionDetails, readVersion) {
    int err = readVersion(&versData);
    CHECK_TRUE(err >= 0);
    
    CHECK_EQUAL(9, versData.boardMaj);
    CHECK_EQUAL(8, versData.boardMin);

    CHECK_EQUAL(7, versData.fwMaj);
    CHECK_EQUAL(65, versData.fwMin);
    CHECK_EQUAL(4, versData.fwRev);

    CHECK_EQUAL(0, versData.debug);

    CHECK_EQUAL(1, versData.threeAxis);
    CHECK_EQUAL(1, versData.batMon);
}

TEST(versionDetails, versionBadPrelude) {
    rxBuff[0] = '<';
    int err = readVersion(&versData);
    CHECK_EQUAL(-EBADPRELUDE, err);
    
    CHECK_NULL_BUFF(pVersData, 23);
}

TEST(versionDetails, versionBadCommand) {
    rxBuff[1] = 'v';
    int err = readVersion(&versData);
    CHECK_EQUAL(-EHEADERERROR, err);

    CHECK_NULL_BUFF(pVersData, 23);
}

TEST(versionDetails, versionBadDataLen) {
    rxBuff[2] = 20;
    int err = readVersion(&versData);
    CHECK_EQUAL(-EHEADERERROR, err);
    CHECK_NULL_BUFF(pVersData, 23);
}

TEST(versionDetails, versionBadChksum) {
    rxBuff[3] = 1;
    int err = readVersion(&versData);
    CHECK_EQUAL(-EHEADERERROR, err);
    CHECK_NULL_BUFF(pVersData, 23);
}

TEST(versionDetails, versionBadDataChksum) {
    rxBuff[22] = 1;
    int err = readVersion(&versData);
    CHECK_EQUAL(-EBADCHECKSUM, err);
    CHECK_NULL_BUFF(pVersData, 23);
}

static ctrl_payload_t payloadBuff;
static ctrl_payload_t expectedCmd;
static byte *pPayload = (byte *)(&payloadBuff);
static byte *pExpected = (byte *)(&expectedCmd); 
const int CTRL_PAYLOAD_LEN = 13;

TEST_GROUP(control) {
    void setup() {
        // C++ initialization will zero out all fields in these payloads
        payloadBuff = ctrl_payload_t();
        expectedCmd = ctrl_payload_t();
    }

    void teardown() {
        CHECK_BUFF(pExpected, pPayload, CTRL_PAYLOAD_LEN);
    }
};

TEST(control, makeZeroControlPayload) {
    mkCtrlPayload(MODE_NO_CONTROL, 0, 0, 0, 0, &payloadBuff);
    CHECK_TRUE(CTRL_PAYLOAD_LEN == sizeof(payloadBuff));
}

TEST(control, makeYawAngleControlPayload) {
    mkCtrlPayload(MODE_ANGLE, 0, 200, 0, 0, &payloadBuff);
    expectedCmd.mode = MODE_ANGLE;
    expectedCmd.yawAngle.lo = 200;
}

TEST(control, makeLargeYawAngleControlPayload) {
    mkCtrlPayload(MODE_ANGLE, 0, 1200, 0, 0, &payloadBuff);
    expectedCmd.mode = MODE_ANGLE;
    expectedCmd.yawAngle.lo = 1200 % 256;
    expectedCmd.yawAngle.hi = 1200 / 256;
}


TEST(control, makePitchAngleControlPayload) {
    mkCtrlPayload(MODE_ANGLE, 0, 0, 0, 89, &payloadBuff);
    expectedCmd.mode = MODE_ANGLE;
    expectedCmd.pitchAngle.lo = 89;
}

TEST(control, makeLargePitchAngleControlPayload) {
    mkCtrlPayload(MODE_ANGLE, 0, 0, 0, 1437, &payloadBuff);
    expectedCmd.mode = MODE_ANGLE;
    expectedCmd.pitchAngle.lo = 1437 % 256;
    expectedCmd.pitchAngle.hi = 1437 / 256;
}

TEST(control, anglesWrapProperly) {
    expectedCmd.mode = MODE_ANGLE;
    for(int i = -7205; i <= -7195; i++) {
        mkCtrlPayload(MODE_ANGLE, 0, i, 0, 0, &payloadBuff);
        int angle = (i <= -7200 ? (i + 7200) : i);
        expectedCmd.yawAngle.lo = (byte)(angle & 0xff);
        expectedCmd.yawAngle.hi = (byte)((angle & 0xff00) >> 8);
        CHECK_BUFF(pExpected, pPayload, sizeof(ctrl_payload_t));
    }

    expectedCmd = ctrl_payload_t();
    expectedCmd.mode = MODE_ANGLE;
    for(int i = 7195; i <= 7205; i++) {
        mkCtrlPayload(MODE_ANGLE, 0, i, 0, 0, &payloadBuff);
        int angle = (i >= 7200 ? (i - 7200) : i);
        expectedCmd.yawAngle.lo = (byte)(angle & 0xff);
        expectedCmd.yawAngle.hi = (byte)((angle & 0xff00) >> 8);
        CHECK_BUFF(pExpected, pPayload, sizeof(ctrl_payload_t));
    }

    expectedCmd = ctrl_payload_t();
    expectedCmd.mode = MODE_ANGLE;
    for(int i = -7205; i <= -7195; i++) {
        mkCtrlPayload(MODE_ANGLE, 0, 0, 0, i, &payloadBuff);
        int angle = (i <= -7200 ? (i + 7200) : i);
        expectedCmd.pitchAngle.lo = (byte)(angle & 0xff);
        expectedCmd.pitchAngle.hi = (byte)((angle & 0xff00) >> 8);
        CHECK_BUFF(pExpected, pPayload, sizeof(ctrl_payload_t));
    }

    expectedCmd = ctrl_payload_t();
    expectedCmd.mode = MODE_ANGLE;
    for(int i = 7195; i <= 7205; i++) {
        mkCtrlPayload(MODE_ANGLE, 0, 0, 0, i, &payloadBuff);
        int angle = (i >= 7200 ? (i - 7200) : i);
        expectedCmd.pitchAngle.lo = (byte)(angle & 0xff);
        expectedCmd.pitchAngle.hi = (byte)((angle & 0xff00) >> 8);
        CHECK_BUFF(pExpected, pPayload, sizeof(ctrl_payload_t));
    }
}

static profile_t profileBuff;
static profile_t expectedProfile;
static byte *pProfile = (byte *)(&profileBuff);
static byte *pExpProf = (byte *)(&expectedProfile);
const int PROFILE_LEN = 102;

TEST_GROUP(profile) {
    void setup() {
        rxBuff = (byte *)calloc(PKT_OVERHEAD + MAX_PAYLOAD, sizeof(byte));
        rxBuff[0] = '>';
        rxBuff[1] = 'R';
        // patch in payload size for rxFaker to work
        rxBuff[2] = (byte)sizeof(profile_t);
        rxBuff[3] = (byte)((rxBuff[1] + rxBuff[2]) % 256);
        for (byte i = 0; i < sizeof(profile_t); i++) {
            rxBuff[4 + i] = i;
        }
        // patch in the checksum
        rxBuff[PKT_OVERHEAD + sizeof(profile_t) - 1] = 31;

        UT_PTR_SET(serialPort.read, &rxFaker);
        profileBuff = profile_t();
        expectedProfile = profile_t();
    }
    
    void teardown() {
        free(rxBuff);
        CHECK_BUFF(pExpProf, pProfile, sizeof(profile_t));
    }
};

TEST(profile, checkProfileInit) {
    CHECK_EQUAL(PROFILE_LEN, sizeof(profile_t));
    CHECK_NULL_BUFF(pProfile, sizeof(profile_t));
    CHECK_NULL_BUFF(pExpProf, sizeof(profile_t));
}

TEST(profile, setValidPIDRoll) {
    setPID(8, 12, 50, &(profileBuff.roll_ctrl));
    expectedProfile.roll_ctrl.p = 8;
    expectedProfile.roll_ctrl.i = 12;
    expectedProfile.roll_ctrl.d = 50;
}

TEST(profile, setValidPIDPitch) {
    setPID(49, 4, 1, &(profileBuff.pitch_ctrl));
    expectedProfile.pitch_ctrl.p = 49;
    expectedProfile.pitch_ctrl.i = 4;
    expectedProfile.pitch_ctrl.d = 1;
}

TEST(profile, setValidPIDYaw) {
    setPID(23, 32, 17, &(profileBuff.yaw_ctrl));
    expectedProfile.yaw_ctrl.p = 23;
    expectedProfile.yaw_ctrl.i = 32;
    expectedProfile.yaw_ctrl.d = 17;
}

TEST(profile, cannotSetNegativePID) {
    // range for PID values is 0..50. Make sure we return out and don't touch
    // the profile.
    int err = setPID(-1, 5, 12, &(profileBuff.pitch_ctrl));
    CHECK_EQUAL(-EBADRANGE, err);
    CHECK_NULL_BUFF(pProfile, sizeof(profile_t));

    err = setPID(5, -50, 12, &(profileBuff.pitch_ctrl));
    CHECK_EQUAL(-EBADRANGE, err);
    CHECK_NULL_BUFF(pProfile, sizeof(profile_t));
    
    err = setPID(5, 12, -49, &(profileBuff.pitch_ctrl));
    CHECK_EQUAL(-EBADRANGE, err);
    CHECK_NULL_BUFF(pProfile, sizeof(profile_t));
}

TEST(profile, cannotSetHighPID) {
    int err = setPID(51, 12, 3, &(profileBuff.pitch_ctrl));
    CHECK_EQUAL(-EBADRANGE, err);
    CHECK_NULL_BUFF(pProfile, sizeof(profile_t));

    err = setPID(2, 180, 34, &(profileBuff.pitch_ctrl));
    CHECK_EQUAL(-EBADRANGE, err);
    CHECK_NULL_BUFF(pProfile, sizeof(profile_t));

    err = setPID(2, 8, 340, &(profileBuff.pitch_ctrl));
    CHECK_EQUAL(-EBADRANGE, err);
    CHECK_NULL_BUFF(pProfile, sizeof(profile_t));
}

TEST(profile, fetchProfile) {
    int err = getProfile(PROFILE_CUR, &profileBuff);
    CHECK_EQUAL(0, err);

    for (byte i = 0; i < sizeof(profile_t); i++) {
        pExpProf[i] = i;
    }
}

