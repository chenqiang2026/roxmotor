#ifndef SCREEN_MCU_chery_H
#define SCREEN_MCU_chery_H
#include "screen_mcu_service.h"
#include "screen_mcu_tp.h"
struct chery_info {
    uint8_t sw_ver[2];
	uint8_t hw_ver[2];
	uint8_t bridge_ver[3];
	uint8_t tp_ver[3];
	uint8_t screen_type;
	uint8_t update_step;
	int update_request_index;
	screen_mcu_tp_t *tp_info;
};
unsigned screen_mcu_chery_init(struct screen_mcu_drv *screen_mcu_p);
#endif
