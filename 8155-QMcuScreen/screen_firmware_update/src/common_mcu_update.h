#ifndef COMMON_MCU_UPDATE_H
#define COMMON_MCU_UPDATE_H
#include "screen_firmware_update.h"
#define MCU_UPDATE_SUCCESSFUL	1
#define MCU_UPDATE_FAIL			-1
int common_mts_mcu_firmware_update(struct screen_firmware * screen_firmware_p, int is_for_tp);
#endif
