#ifndef GUARD_PINGU_H
#define GUARD_PINGU_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include <stdint.h>
#include "uartlinux.h"
#include "bgcdefs.h"

#pragma pack(1)
// little-endian, 16bit, signed values are used a lot
typedef struct le16_t {
    byte lo;
    byte hi;
} le16_t;
#pragma pack()

typedef enum ctrl_mode_t {
    MODE_NO_CONTROL = 0,
    MODE_SPEED,
    MODE_ANGLE,
    MODE_SPEED_ANGLE
} ctrl_mode_t;

typedef enum profile_id_t {
    PROFILE_0 = 0,
    PROFILE_1,
    PROFILE_2,
    PROFILE_CUR = 255
} profile_id_t;

#pragma pack(1)
typedef struct ctrl_payload_t {
    byte mode;
    le16_t rollSpeed;
    le16_t rollAngle;
    le16_t pitchSpeed;
    le16_t pitchAngle;
    le16_t yawSpeed;
    le16_t yawAngle;
} ctrl_payload_t;

typedef struct axis_ctrl_t {
    byte p;
    byte i;
    byte d;
    byte power;
    byte invert;
    byte poles;
} axis_ctrl_t;

typedef struct axis_rc_t {
    byte ignored[8];
} axis_rc_t;

typedef struct profile_t {
    byte id;
    axis_ctrl_t roll_ctrl;
    axis_ctrl_t pitch_ctrl;
    axis_ctrl_t yaw_ctrl;
    byte ext_fc[2]; // unused right now
    axis_rc_t rc_axes[3]; // unused
    byte gyro_trust;
    byte use_model;
    byte pwm_freq;
    byte uart_speed;
    byte unused_mid[22]; // RC_TRIM_ROLL .. FOLLOW_ROLL_MIX_RANGE
    byte gen_params[30];
    byte cur_id;
} profile_t;
#pragma pack()

typedef struct vers_data_t {
    unsigned int boardMaj;
    unsigned int boardMin;
    unsigned int fwMaj;
    unsigned int fwMin;
    unsigned int fwRev;
    unsigned int debug : 1;
    unsigned int threeAxis : 1;
    unsigned int batMon : 1;
} vers_data_t;

// Creates a proper packet for command, with payload data, and puts it into
// memory pointed to by buffer. buffer must have at least enough space for the
// payload and 5 bytes overhead. Returns the length of the packet in bytes, or
// a negative number on error.
int mkPacket(byte command, byte *data, size_t data_size, byte *buffer);

// Angles are in tenths of a degree
void mkCtrlPayload(ctrl_mode_t mode,
        int yawSpeed, int yawAngle, int pitchSpeed, int pitchAngle,
        ctrl_payload_t *buff);

// Set the target angle in an angle-control mode, speed is dependent on PID
// settings. Units are tenths of a degree.
void setAngle(int yaw, int pitch);

// Set a constant speed for yaw and pitch. Units are deg/s, scaled by 
// (strangely) 0.1220740379, so a value of 5 translates to roughly 0.6104 deg/s.
// Note: this is not a speed limit, but will actually directly control the speed
void setSpeed(int yaw, int pitch);

// read and parse version info
int readVersion(vers_data_t *buff);

int setPID(int p, int i, int d, axis_ctrl_t *axis);

int getProfile(profile_id_t id, profile_t *buff);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // GUARD_PINGU_H
