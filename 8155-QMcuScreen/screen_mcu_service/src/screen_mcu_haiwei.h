#ifndef SCREEN_MCU_HAIWEI_H
#define SCREEN_MCU_HAIWEI_H
#include "screen_mcu_service.h"
struct haiwei_info {
    uint8_t sw_ver[24];
	uint8_t hw_ver[24];
	uint8_t bl_ver[24];
	uint8_t tp_ver[24];
};
unsigned screen_mcu_haiwei_init(struct screen_mcu_drv *screen_mcu_p);
#endif
