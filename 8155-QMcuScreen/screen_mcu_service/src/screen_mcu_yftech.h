#ifndef SCREEN_MCU_YFTECH_H
#define SCREEN_MCU_YFTECH_H
#include "screen_mcu_service.h"
struct did_info {
	uint8_t i2caddr;
	uint8_t sw_ver[24];
	uint8_t hw_ver[24];
	uint8_t supplier_sw_ver[24];
	uint8_t supplier_hw_ver[24];
	uint8_t sn[24];
	uint8_t pn[24];
	uint8_t manufacture_date[24];
	uint8_t bl_ver[24];
	uint8_t i2c_ver[24];
	uint8_t tp_ver[24];
	uint8_t temperature[24];
	uint8_t session[24];
	uint8_t dialg_code[128];
};

unsigned screen_mcu_yftech_init(struct screen_mcu_drv *screen_mcu_p);
#endif
