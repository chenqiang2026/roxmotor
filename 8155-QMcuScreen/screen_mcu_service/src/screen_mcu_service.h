#ifndef SCREEN_MCU_SERVICE_H
#define SCREEN_MCU_SERVICE_H
#include <sys/slog.h>
#include <sys/slogcodes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <pthread.h>
#include <amss/i2c_client.h>
#define dbg_out(fmt, args...)	//slogf(_SLOG_SETCODE(_SLOG_SYSLOG, 0), _SLOG_DEBUG1, fmt, ##args)
#define info_out(fmt, args...)	slogf(_SLOG_SETCODE(_SLOG_SYSLOG, 0), _SLOG_INFO, fmt, ##args)
#define warn_out(fmt, args...)	slogf(_SLOG_SETCODE(_SLOG_SYSLOG, 0), _SLOG_WARNING, fmt, ##args)
#define err_out(fmt, args...)	slogf(_SLOG_SETCODE(_SLOG_SYSLOG, 0), _SLOG_ERROR, fmt, ##args)
#define REQUEST_LEN 128
struct screen_mcu_drv;
typedef unsigned attachFuncType(struct screen_mcu_drv *screen_mcu_drv);
typedef int setRequestFuncType(void *screen_mcu_drv, char *request);
typedef int getReplyFuncType(void *screen_mcu_drv, char *reply_msg);
typedef unsigned detachFuncType(struct screen_mcu_drv *screen_mcu_drv);
struct screen_mcu_drv{
	uint8_t busnum;
	uint8_t i2caddr;
	uint8_t status;
	char protocol[16];
	uint8_t attach_chg;
	char name[16];
	uint8_t request_chg;
	char request[REQUEST_LEN];
	char reply[32];
	int i2cdev;
	int power_gpio;
	uint8_t motor_type;
	uint8_t show_tft_erro;
	uint8_t illuminance_sensor;
	uint8_t bl_on;
	int bl;
	uint8_t pwr_chg;
	uint8_t bkl_chg;
	uint8_t is_updating;
	uint8_t update_progress;
	unsigned dtc_irq_id;
    unsigned dtc_irq_num;
    unsigned dtc_irq_gpio;
    unsigned dtc_irq_type;
	pthread_t dtc_irq_thread;
	unsigned tp_irq_id;
    unsigned tp_irq_num;
    unsigned tp_irq_gpio;
    unsigned tp_irq_type;
	pthread_t tp_irq_thread;
	pthread_mutex_t lock;
	pthread_cond_t request_ready_cond;
	pthread_t request_handler_thread;
	attachFuncType* attach_handler;
	setRequestFuncType* set_request_handler;
	getReplyFuncType* get_reply_handler;
	detachFuncType* detach_handler;
 	void * priv_data;
};

int get_i2cdev_with_bus(int busNum);

int i2c_write_bat(struct screen_mcu_drv *screen_mcu_p, uint8_t dev, uint8_t *buf, uint8_t len);
int i2c_read_bytes(struct screen_mcu_drv *screen_mcu_p, uint8_t dev, uint8_t reg, uint8_t *buf, uint8_t len);
int i2c_read_bytes_direct(struct screen_mcu_drv *screen_mcu_p, uint8_t dev, uint8_t *buf, uint8_t len);
#endif

