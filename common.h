#ifndef COMMON_H
#define COMMON_H

#include <stdlib.h>
#include <string.h>


enum MessageType {
    TERMINATE = 0,
    REGISTER = 1,
    REGISTER_SUCCESS = 2,
    REGISTER_FAILURE = 3,
    LOC_REQUEST = 4,
    LOC_SUCCESS = 5,
    LOC_FAILURE = 6,
    EXECUTE = 7,
    EXECUTE_SUCCESS = 8,
    EXECUTE_FAILURE = 9
};

enum ErrorMsg
{
    COMPLETE = 0,
    SKELETON_NOT_FOUND = 1,
    SKELETON_HAS_ALREADY_REGISTERED = 2,
    NOT_INIT = -1,
    FAIL_TO_CONNECT = -2,
    UNEXPECTED_MESSAGE = -3,
    HOSTNAME_ERROR = -4,
    INVALID_FD = -5,
    INVALID_ARGTYPE = -6,
    INVALID_NAME = -7,
    INVALID_SKELETON = -8,
    FAIL_TO_BIND = -9,
    FAIL_TO_LISTEN = -10,
    NOT_REGISTER = -11,
    FAIL_TO_ACCEPT = -12,
    MESSAGE_TYPE_ERROR = -13,
    INVALID_PORT = -15,
    FAIL_TO_REGISTER = -16

};


#endif
