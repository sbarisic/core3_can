#pragma once

#ifndef OPSYS_SIM
#include "esp_attr.h"
#endif

#include <inttypes.h>
#include <stdint.h>
#include <stddef.h>

#if defined(_WIN32) || defined(_WIN64)
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define CORE2_WINDOWS
#endif

#if defined(__cplusplus)
extern "C"
{
#endif

    void core3_opsys_init();

#if defined(__cplusplus)
}
#endif
