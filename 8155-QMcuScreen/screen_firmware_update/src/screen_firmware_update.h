#ifndef SCREEN_FIRMWARE_UPDATE_H
#define SCREEN_FIRMWARE_UPDATE_H
typedef int updateCallbackFuncType(void *screen_firmware,  int progress);
enum 
{
	YFTECH_PROTOCOL,
	HAIWEI_PROTOCOL,
	HUAYANG_RPOTOCOL
};
struct screen_firmware{
	int i2c_bus;
	int i2c_dev;
	uint8_t slave_addr;
	char cur_version[256];
	char new_version[256];
	char cur_pn[256];
	char new_pn[256];
	char firmware_path[1024];
	updateCallbackFuncType* callback;
	int protocol;
};
int start_update_firmware(struct screen_firmware *screen_firmware_p);
#endif
