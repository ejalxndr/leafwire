#include "nlctl.h"

const char *nl_strerror(nl_status s)
{
    switch (s) {
    case NL_OK:            return "success";
    case NL_ERR_HID_INIT:  return "HID API initialization failed";
    case NL_ERR_HID_OPEN:  return "could not open HID device";
    case NL_ERR_HID_IO:    return "HID transfer failed";
    case NL_ERR_X_OPEN:    return "could not open X display";
    case NL_ERR_X_SHM:     return "X shared-memory setup failed";
    case NL_ERR_SOCKET:    return "socket operation failed";
    case NL_ERR_PROTO:     return "protocol error";
    case NL_ERR_STATE_IO:  return "state file I/O failed";
    case NL_ERR_ARGS:      return "invalid arguments";
    case NL_ERR_NODEV:     return "device not connected";
    case NL_ERR_NOMEM:     return "out of memory";
    case NL_ERR_TIMEOUT:   return "operation timed out";
    }
    return "unknown error";
}
