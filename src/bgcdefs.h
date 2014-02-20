#ifndef GUARD_BGCDEFS_H
#define GUARD_BGCDEFS_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

typedef unsigned char byte;

typedef enum pingu_err_t {
    EBADPRELUDE = 1,
    EHEADERERROR,
    EBADCHECKSUM,
    EBADRANGE,
    ETTYERROR,
    EUNKNOWN = 100
} pingu_err_t;

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // GUARD_BGCDEFS_H
