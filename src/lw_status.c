#include "leafwire.h"

const char *lw_strerror(lw_status s)
{
    switch (s) {
    case LW_OK:            return "success";
    case LW_ERR_HID_INIT:  return "HID API initialization failed";
    case LW_ERR_HID_OPEN:  return "could not open HID device";
    case LW_ERR_HID_IO:    return "HID transfer failed";
    case LW_ERR_X_OPEN:    return "could not open X display";
    case LW_ERR_X_SHM:     return "X shared-memory setup failed";
    case LW_ERR_SOCKET:    return "socket operation failed";
    case LW_ERR_PROTO:     return "protocol error";
    case LW_ERR_STATE_IO:  return "state file I/O failed";
    case LW_ERR_ARGS:      return "invalid arguments";
    case LW_ERR_NODEV:     return "device not connected";
    case LW_ERR_NOMEM:     return "out of memory";
    case LW_ERR_TIMEOUT:   return "operation timed out";
    }
    return "unknown error";
}
