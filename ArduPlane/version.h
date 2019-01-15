#pragma once

#include "ap_version.h"

#define THISFIRMWARE "QuadPlane V3.8.5_SimpTrans"
#define FIRMWARE_VERSION 3,8,5,FIRMWARE_VERSION_TYPE_BETA

#ifndef GIT_VERSION
#define FIRMWARE_STRING THISFIRMWARE
#else
#define FIRMWARE_STRING THISFIRMWARE " (" GIT_VERSION ")"
#endif
