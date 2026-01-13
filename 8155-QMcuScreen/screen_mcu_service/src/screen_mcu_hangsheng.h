#ifndef SCREEN_MCU_HANGSHENG_H
#define SCREEN_MCU_HANGSHENG_H
#include "screen_mcu_service.h"
#include "screen_mcu_tp.h"
struct huangsheng_did_info {
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
	screen_mcu_tp_t* tp_info;
};

unsigned screen_mcu_hangsheng_init(struct screen_mcu_drv *screen_mcu_p);
#endif
