#ifndef SCREEN_MCU_yfmotor_H
#define SCREEN_MCU_yfmotor_H
#include "screen_mcu_service.h"
#include "screen_mcu_tp.h"
struct yfmotor_info {
    uint8_t sw_ver[3];
	uint8_t hw_ver[2];
	uint8_t bl_ver[2];
	uint8_t tp_ver[8];
	uint8_t tilt_degree;
	uint8_t tilt_stat;
	uint8_t power_stat;
	uint8_t open_stat;
	uint8_t anti_pinch_stat;
	uint8_t key_lock_stat;
	uint8_t key_stat;
	screen_mcu_tp_t *tp_info;
};
unsigned screen_mcu_yfmotor_init(struct screen_mcu_drv *screen_mcu_p);
#endif
