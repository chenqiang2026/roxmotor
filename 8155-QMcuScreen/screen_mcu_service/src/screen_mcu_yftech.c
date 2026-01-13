#include <ctype.h>
#include <gpio_client.h>
#include <sys/mman.h>
#include <hw/inout.h>
#include <sys/iofunc.h>
#include <amss/core/pdbg.h>
#include "screen_mcu_yftech.h"
#define MCU_REG_DIAG_WRITE_ADDRESS			0x40	//diagnosis wirte
#define MCU_REG_DIAG_READ_ADDRESS			0x41	//diagnosis	read
#define MCU_REG_ILLUMINANCE_ADDRESS			0x09	//illuminance read
#define MCU_REG_DIAG_READ_DTC_ADDRESS       0x50   
#define MCU_SERVICE_ID_MCU_READ			0x22	//mcu read
#define MCU_REG_SET_MFD_ADDRESS 0xA2
#define MCU_REG_GET_MFD_ADDRESS 0xA3
#define MCU_REG_SET_SN_ADDRESS 0xA4
#define MCU_REG_GET_SN_ADDRESS 0xA5

#define DID_NUMBER_SOFTWARE_VERSION		0xFEF0 // software version
#define DID_NUMBER_HARDWARE_VERSION		0xFEF1 // hardware version
#define DID_NUMBER_SUPPLIER_SOFTWARE_VERSION		0xFEFD // supplier software version
#define DID_NUMBER_SUPPLIER_HARDWARE_VERSION		0xFEFE // supplier hardware version
#define DID_NUMBER_DISPLAY_PN	0xFEF5	//display pn
#define DID_NUMBER_DISPLAY_SN   0xFEF6  //display sn
#define DID_NUMBER_MANUFACTURE_DATE   0xFEFA  //manufacture date
#define DID_NUMBER_BOOTLOADER_VERSION   0xFEF2 //bootloader version
#define DID_NUMBER_I2C_VERSION   0xFEF3 //I2C version
#define DID_NUMBER_TP_VERSION   0xFEFB //tp version
#define DID_NUMBER_TEMPERATURE   0xFEF8 //temperature
#define DID_NUMBER_SESSION		0xFEF7	//session

#define MAX_FILE_NAME_LEN	24

static int set_request_yftech(void *screen_mcu_drv, char *request){
	struct screen_mcu_drv *screen_mcu_p = (struct screen_mcu_drv *)screen_mcu_drv;
	unsigned nbytes = strlen(request);
	int le = nbytes - 1;
	while (le > 0 && isspace(request[le]))
		request[le--] = 0;
	pthread_mutex_lock(&screen_mcu_p->lock);
	info_out("%s, busnum:%d, i2caddr:0x%02x,request:%s\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, request);
	strcpy(screen_mcu_p->request, request);
	screen_mcu_p->request_chg = -1;
	pthread_cond_signal(&screen_mcu_p->request_ready_cond);
	pthread_mutex_unlock(&screen_mcu_p->lock);

	return nbytes;
}
static int get_reply_yftech(void *screen_mcu_drv, char *reply_msg){
	struct screen_mcu_drv *screen_mcu_p = (struct screen_mcu_drv *)screen_mcu_drv;
	unsigned nbytes=0;
	struct did_info *did_info_p = screen_mcu_p->priv_data;
	
	pthread_mutex_lock(&screen_mcu_p->lock);
	info_out("%s, busnum:%d, i2caddr:0x%02x, screen_mcu_p->request:%s\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, screen_mcu_p->request);
	if(!strcmp(screen_mcu_p->request, "lcd get show sver")){
		nbytes = snprintf(reply_msg, 24, "%s\n", did_info_p->sw_ver);
	}else if(!strcmp(screen_mcu_p->request, "lcd get show hver")){
		nbytes = snprintf(reply_msg, 24, "%s\n", did_info_p->hw_ver);
	}else if(!strcmp(screen_mcu_p->request, "lcd get bl ver")){
		nbytes = snprintf(reply_msg, 24, "%s\n", did_info_p->bl_ver);
	}else if(!strcmp(screen_mcu_p->request, "lcd get supplier sver")){
		nbytes = snprintf(reply_msg, 24, "%s\n", did_info_p->supplier_sw_ver);
	}else if(!strcmp(screen_mcu_p->request, "lcd get supplier hver")){
		nbytes = snprintf(reply_msg, 24, "%s\n", did_info_p->supplier_hw_ver);
	}else if(!strcmp(screen_mcu_p->request, "lcd get PN")){
		nbytes = snprintf(reply_msg, 24, "%s\n", did_info_p->pn);
	}else if(!strcmp(screen_mcu_p->request, "lcd get SN")){
		nbytes = snprintf(reply_msg, 24, "%s\n", did_info_p->sn);
	}else if(!strcmp(screen_mcu_p->request, "lcd get MFD")){
		nbytes = snprintf(reply_msg, 24, "%s\n", did_info_p->manufacture_date);
	}else if(!strcmp(screen_mcu_p->request, "lcd get tp ver")){
		nbytes = snprintf(reply_msg, 24, "%s\n", did_info_p->tp_ver);
	}else if(!strcmp(screen_mcu_p->request, "lcd get temperature")){
	    short temperature = (did_info_p->temperature[0] << 8) | did_info_p->temperature[1];
		nbytes = snprintf(reply_msg, 24, "%d\n", temperature);
	}else if(!strcmp(screen_mcu_p->request, "lcd get session")){
	    nbytes = snprintf(reply_msg, 24, "%d\n", did_info_p->session[0]);
	}else if(!strcmp(screen_mcu_p->request, "lcd get dialg code")){
		nbytes = snprintf(reply_msg, 128, "%s\n", did_info_p->dialg_code);
	}else{
	    nbytes = snprintf(reply_msg, 128, "%s\n", screen_mcu_p->reply);
	}
	pthread_mutex_unlock(&screen_mcu_p->lock);
	info_out("%s, busnum:%d, i2caddr:0x%02x, nbytes%d, reply_msg:%s\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, nbytes, reply_msg);
	return nbytes;
}
  
static int yftech_get_did_info(struct screen_mcu_drv *screen_mcu_p, const uint16_t did_number, uint8_t * rxdata){
	uint8_t write_byte[6] = {0};
	int ret = -1;
	int retry = 3;

	write_byte[0] = MCU_REG_DIAG_WRITE_ADDRESS;
	write_byte[1] = 3;
	write_byte[2] = MCU_SERVICE_ID_MCU_READ;
	write_byte[3] = did_number >> 8;
	write_byte[4] = did_number & 0x00ff;
	if(!i2c_write_bat(screen_mcu_p, screen_mcu_p->i2caddr, write_byte, 5)){
		while(ret && retry-- > 0){
			usleep(100000);
			uint8_t buf[25] = {0};
			ret = i2c_read_bytes(screen_mcu_p, screen_mcu_p->i2caddr, MCU_REG_DIAG_READ_ADDRESS, buf, 24);
			if(!ret && buf[2] == (did_number >> 8) && buf[3] == (did_number & 0x00ff) && buf[0] > 3){
				info_out("yftech_get_diag_info success len:%d\n", buf[0]);
				memcpy(rxdata, &buf[4], buf[0]-3);
				ret = 0;
			}else{
				ret = -1;			
			}
		 }
		return ret;
	}else{
		return -1;	
	}
}
static char mcu_calc_crc8(const char *data, uint8_t len)
{
  uint32_t idx;
  char idx2;
  char crc_val = 0xFF;
  char crc_poly = 0x1D;

  for(idx = 0; idx < len; idx++) {
    crc_val ^= data[idx];
    for(idx2 = 0; idx2 < 8; idx2++) {
      if((crc_val & (char)0x80) > 0) {
        crc_val = ((char)(crc_val << 1)) ^ crc_poly;
      } else {
        crc_val = (char)(crc_val << 1);
      }
    }
  }
  return crc_val;
}

static int set_mfd_info(struct screen_mcu_drv *screen_mcu_p, char* data)
{
    char write_byte[11] = { 0 };
    uint32_t len = strlen(data);
    if (len != 8) {
        info_out("%s, busnum:%d, i2caddr:0x%02x, data length error. \n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
        return -1;
    }

    write_byte[0] = MCU_REG_SET_MFD_ADDRESS;
    write_byte[1] = 9;
    memcpy(&write_byte[2], data, 8);
    write_byte[10] = mcu_calc_crc8(write_byte, 10);

    return i2c_write_bat(screen_mcu_p, screen_mcu_p->i2caddr, (uint8_t*)(&write_byte[0]), 11);
}

static int set_sn_info(struct screen_mcu_drv *screen_mcu_p, char* data)
{
    char write_byte[13] = { 0 };
    uint32_t len = strlen((char*)data);
    if (len != 10) {
        info_out("%s, busnum:%d, i2caddr:0x%02x, data length error. \n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
        return -1;
    }

    write_byte[0] = MCU_REG_SET_SN_ADDRESS;
    write_byte[1] = 11;
    memcpy(&write_byte[2], data, 10);
    write_byte[12] = mcu_calc_crc8(write_byte, 12);

    return i2c_write_bat(screen_mcu_p, screen_mcu_p->i2caddr, (uint8_t*)(&write_byte[0]), 13);
}

static int set_acc_state(struct screen_mcu_drv *screen_mcu_p, int onoff){
    char write_byte[5] = { 0 };
    write_byte[0] = 0x0a; 
    write_byte[1] = 3;
    write_byte[2] = 0xfe;
    write_byte[3] = 0xfc;
    write_byte[4] = mcu_calc_crc8(write_byte, 4);
    if(!onoff){
        i2c_write_bat(screen_mcu_p, screen_mcu_p->i2caddr, (uint8_t*)(&write_byte[0]), 5);
        uint8_t buf[3] = {0};
        int retry = 6;
        uint16_t screen_state = 0;
        do{
            usleep(50000);
			if(!i2c_read_bytes(screen_mcu_p, screen_mcu_p->i2caddr, 0x0b, buf, 3)){
			    screen_state = (buf[1] << 8) | buf[0];
			}
			info_out("%s, busnum:%d, i2caddr:0x%02x, screen_state: 0x%04x\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, screen_state);
		}while(retry-- && screen_state != 0x5e5f);
    }else{
        
    }
    if (screen_mcu_p->power_gpio >= 0) {
		lseek(screen_mcu_p->power_gpio, 0, SEEK_SET);
		write(screen_mcu_p->power_gpio, onoff ? "1" : "0", 1);
		usleep(10000);
	}
    return 0;
}

static int set_lcd_tilt_degree(struct screen_mcu_drv *screen_mcu_p, int angle) {
	int rc = -1;

	if (1 == screen_mcu_p->motor_type) {
		uint8_t buf[] = { 0xB1, 3, 0x00, 0x00, 0x00, 0x00 };

		if (angle) {
			buf[2] = 0x01;
			buf[3] = angle - 1;
		}
		buf[sizeof(buf)-1] = mcu_calc_crc8((char*)buf, sizeof(buf)-1);

		rc = i2c_write_bat(screen_mcu_p, screen_mcu_p->i2caddr, buf, sizeof(buf));
	}
	info_out("%s, angle:%d, rc:%d\n", __func__, angle, rc);

	return 0;
}

static int get_motor_state(struct screen_mcu_drv *screen_mcu_p){
    uint8_t buf[3] = {0};
    if(!i2c_read_bytes(screen_mcu_p, screen_mcu_p->i2caddr, 0xb1, buf, 4)){
        snprintf(screen_mcu_p->reply, 32, "state:%d,degree:%d", buf[1], buf[2]);
	}
	
	return 0;
}

static int handle_request(struct screen_mcu_drv *screen_mcu_p){
	struct did_info *did_info_p = screen_mcu_p->priv_data;

	if(!strcmp(screen_mcu_p->request, "lcd get show sver")){
		yftech_get_did_info(screen_mcu_p, DID_NUMBER_SOFTWARE_VERSION, did_info_p->sw_ver);
	}else if(!strcmp(screen_mcu_p->request, "lcd get show hver")){
		yftech_get_did_info(screen_mcu_p, DID_NUMBER_HARDWARE_VERSION, did_info_p->hw_ver);
	}else if(!strcmp(screen_mcu_p->request, "lcd get bl ver")){
		yftech_get_did_info(screen_mcu_p, DID_NUMBER_BOOTLOADER_VERSION, did_info_p->bl_ver);
	}else if(!strcmp(screen_mcu_p->request, "lcd get supplier sver")){
		yftech_get_did_info(screen_mcu_p, DID_NUMBER_SUPPLIER_SOFTWARE_VERSION, did_info_p->supplier_sw_ver);
	}else if(!strcmp(screen_mcu_p->request, "lcd get supplier hver")){
		yftech_get_did_info(screen_mcu_p, DID_NUMBER_SUPPLIER_HARDWARE_VERSION, did_info_p->supplier_hw_ver);
	}else if(!strcmp(screen_mcu_p->request, "lcd get PN")){
		yftech_get_did_info(screen_mcu_p, DID_NUMBER_DISPLAY_PN, did_info_p->pn);
	}else if(!strcmp(screen_mcu_p->request, "lcd get SN")){
		yftech_get_did_info(screen_mcu_p, DID_NUMBER_DISPLAY_SN, did_info_p->sn);
	}else if(!strcmp(screen_mcu_p->request, "lcd get MFD")){
		yftech_get_did_info(screen_mcu_p, DID_NUMBER_MANUFACTURE_DATE, did_info_p->manufacture_date);
	}else if(!strcmp(screen_mcu_p->request, "lcd get tp ver")){
		yftech_get_did_info(screen_mcu_p, DID_NUMBER_TP_VERSION, did_info_p->tp_ver);
	}else if(!strcmp(screen_mcu_p->request, "lcd get temperature")){
		yftech_get_did_info(screen_mcu_p, DID_NUMBER_TEMPERATURE, did_info_p->temperature);
	}else if(!strcmp(screen_mcu_p->request, "lcd get session")){
		yftech_get_did_info(screen_mcu_p, DID_NUMBER_SESSION, did_info_p->session);
	}else if(!strcmp(screen_mcu_p->request, "lcd get motor state")){
		get_motor_state(screen_mcu_p);
	}else if(!strncmp(screen_mcu_p->request, "lcd set MFD", strlen("lcd set MFD"))){
		set_mfd_info(screen_mcu_p, &(screen_mcu_p->request[12]));
	}else if(!strncmp(screen_mcu_p->request, "lcd set SN", strlen("lcd set SN"))){
		set_sn_info(screen_mcu_p, &(screen_mcu_p->request[11]));
	}else if(!strncmp(screen_mcu_p->request, "lcd acc on", strlen("lcd acc on"))){
		set_acc_state(screen_mcu_p, 1);
	}else if(!strncmp(screen_mcu_p->request, "lcd acc off", strlen("lcd acc off"))){
	    usleep(500000);
		set_acc_state(screen_mcu_p, 0);
	}else if(!strncmp(screen_mcu_p->request, "lcd tilt degree", 15)) {
		set_lcd_tilt_degree(screen_mcu_p, atoi(screen_mcu_p->request + 15));
	}else{
	
	}
	return 0;
}

static void *request_handler_thread(void *thread_param){
	int wret;
	struct screen_mcu_drv *screen_mcu_p = thread_param;
	struct did_info *did_info_p = screen_mcu_p->priv_data;
	int temperature_interval = 3;
	pthread_mutex_lock(&screen_mcu_p->lock);
	do{
		if(screen_mcu_p->attach_chg){
			screen_mcu_p->attach_chg = 0;
		}
		if(screen_mcu_p->request_chg){
			if(!handle_request(screen_mcu_p)){
				screen_mcu_p->request_chg = 0;	
			}
		}
		
		if(temperature_interval == 3){
		    yftech_get_did_info(screen_mcu_p, DID_NUMBER_TEMPERATURE, did_info_p->temperature);
		    temperature_interval = 0;
		}
		temperature_interval++;
		
		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);
		now.tv_sec=now.tv_sec+1;
		wret = pthread_cond_timedwait(&screen_mcu_p->request_ready_cond, &screen_mcu_p->lock, &now);
		//info_out("%s, busnum:%d, i2caddr:0x%02x, new request\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
		//wret = pthread_cond_wait(&screen_mcu_p->request_ready_cond, &screen_mcu_p->lock);
	}while(wret == 0 || wret == ETIMEDOUT);	

	info_out("%s, busnum:%d, i2caddr:0x%02x, exit request_handler_thread\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
	pthread_mutex_unlock(&screen_mcu_p->lock);
	
	return NULL;
}

#define DTC_POLL_MODE    0x1000

static int dtc_irq_init(struct screen_mcu_drv *screen_mcu_p)
{
    int fd;
    char *name = "/dev/gpio";
	if(DTC_POLL_MODE == screen_mcu_p->dtc_irq_gpio){
		return 0;
	}
	if(screen_mcu_p->dtc_irq_gpio<=0){
		return -1;	
	}
	screen_mcu_p->dtc_irq_type = GPIO_INTERRUPT_TRIGGER_FALLING;
    fd = gpio_open(name);
    if(fd >= 0) {
        int err = gpio_set_interrupt_cfg(fd, screen_mcu_p->dtc_irq_gpio, screen_mcu_p->dtc_irq_type, NULL);
        if(err == EOK) {
            err = gpio_get_interrupt_cfg(fd, screen_mcu_p->dtc_irq_gpio, &screen_mcu_p->dtc_irq_num);
            if(err == EOK && screen_mcu_p->dtc_irq_num > 0) {
				info_out("%s, busnum:%d, i2caddr:0x%02x,gpio %d num %d\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, screen_mcu_p->dtc_irq_gpio, screen_mcu_p->dtc_irq_num);
                return 0;
            }
            else {
				err_out("%s, busnum:%d, i2caddr:0x%02x, get gpio %d num %d fail\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, screen_mcu_p->dtc_irq_gpio, screen_mcu_p->dtc_irq_num);
            }
        }
        else {
			err_out("%s, busnum:%d, i2caddr:0x%02x, set gpio %d type %d fail\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, screen_mcu_p->dtc_irq_gpio, screen_mcu_p->dtc_irq_type);
        }
        gpio_close(name);
    }
    else {
		err_out("%s, busnum:%d, i2caddr:0x%02x, gpio_open fail %d\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, fd);
    }
    return -1;
}

static void dtc_cleanup(void *arg){
    struct screen_mcu_drv *screen_mcu_p = arg;
    info_out("%s, busnum:%d, i2caddr:0x%02x\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
    pthread_mutex_unlock(&screen_mcu_p->lock);
}
int yferr_log(const char *os, const char *type, const char *subsys, const char *name, int errcode);
static int show_tft_erro(struct screen_mcu_drv *screen_mcu_p, int status){
    char temp[64];
    int fd = -1;
    char buf[1] = {0};
    
    
    if(status){
       buf[0] = 1; 
    }else{
        buf[0] = 0;
    }
    
	snprintf(temp, sizeof(temp), "/tmp/%s_tft_erro", screen_mcu_p->name);
	fd = open(temp, O_RDWR|O_CREAT|O_CLOEXEC);
	if(fd < 0){
	    err_out("%s, busnum:%d, i2caddr:0x%02x failed open %s \n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, temp);
	}else{
	    if (lseek(fd, 0, SEEK_SET) == 0 &&
			write(fd, buf, sizeof(buf)) > 0) {
			close(fd);
			return 0;
		}else{
		    err_out("%s, busnum:%d, i2caddr:0x%02x can't write file %s\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, temp);
		}
		close(fd);
	}
	
	return -1;
}
static void *dtc_irq_thread(void *thread_param){
	int err;
	struct screen_mcu_drv *screen_mcu_p = thread_param;
	struct did_info *did_info_p = screen_mcu_p->priv_data;
	struct sigevent event;
	uint8_t buf[16] = {0};
	pthread_cleanup_push(dtc_cleanup, screen_mcu_p);

	if(DTC_POLL_MODE != screen_mcu_p->dtc_irq_gpio){
		SIGEV_INTR_INIT(&event);
		screen_mcu_p->dtc_irq_id = InterruptAttachEvent(screen_mcu_p->dtc_irq_num, &event, _NTO_INTR_FLAGS_TRK_MSK);
		InterruptMask(screen_mcu_p->dtc_irq_num, screen_mcu_p->dtc_irq_id);
	}
	info_out("%s, busnum:%d, i2caddr:0x%02x,gpio %d id %d\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, screen_mcu_p->dtc_irq_gpio, screen_mcu_p->dtc_irq_id);
	while(1){
		if(DTC_POLL_MODE == screen_mcu_p->dtc_irq_gpio){
			sleep(2);
		} else {
			do{
				err = InterruptUnmask(screen_mcu_p->dtc_irq_num, screen_mcu_p->dtc_irq_id);
				if(err != 0) {	
					err_out("%s, busnum:%d, i2caddr:0x%02x,InterruptUnmask %d\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, err);
				}
			}while(err > 0);
			info_out("%s, busnum:%d, i2caddr:0x%02x,InterruptWait_r\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
			err = InterruptWait_r(0, NULL );
			info_out("%s, busnum:%d, i2caddr:0x%02x,process irq %d\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, err);
		}

        pthread_mutex_lock(&screen_mcu_p->lock);
		if(!i2c_read_bytes(screen_mcu_p, screen_mcu_p->i2caddr, MCU_REG_DIAG_READ_DTC_ADDRESS, buf, 16)){
			info_out("%s, busnum:%d, i2caddr:0x%02x, DTC reg buf:0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x \n", __func__,
			screen_mcu_p->busnum, screen_mcu_p->i2caddr, buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8], buf[9], buf[10], buf[11], buf[12], buf[13], buf[14], buf[15]);
			snprintf(did_info_p->dialg_code, 128, "0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x",buf[0], buf[1], buf[2], 
			buf[3], buf[4], buf[5], buf[6], buf[7], buf[8], buf[9], buf[10], buf[11], buf[12], buf[13], buf[14], buf[15]);
			if(!(buf[1]&0x03)){
			    yferr_log("QNX", "DRIVER", "VOLTAGE", screen_mcu_p->name, 0);
			}else{
			    if(buf[1]&0x01){
			        yferr_log("QNX", "DRIVER", "VOLTAGE", screen_mcu_p->name, -1);
			    }
			    if(buf[1]&0x02){
			        yferr_log("QNX", "DRIVER", "VOLTAGE", screen_mcu_p->name, -2);
			    }
			}
			if(buf[1]&0x10){
			    yferr_log("QNX", "DRIVER", "TFT", screen_mcu_p->name, -1);
			    if(screen_mcu_p->show_tft_erro){
			        show_tft_erro(screen_mcu_p, -1);
			    }
			}else{
			    yferr_log("QNX", "DRIVER", "TFT", screen_mcu_p->name, 0);
			    if(screen_mcu_p->show_tft_erro){
			        show_tft_erro(screen_mcu_p, 0);
			    }
			}
			if(buf[1]&0x40){
			    yferr_log("QNX", "DRIVER", "TP", screen_mcu_p->name, -1);
			}else{
			    yferr_log("QNX", "DRIVER", "TP", screen_mcu_p->name, 0);
			}
			if(buf[2]&0x01){
			    yferr_log("QNX", "DRIVER", "TEMP", screen_mcu_p->name, -1);
			}else{
			    yferr_log("QNX", "DRIVER", "TEMP", screen_mcu_p->name, 0);
			}
			if(buf[2]&0x02){
			    yferr_log("QNX", "DRIVER", "BACKLIGHT", screen_mcu_p->name, -1);
			}else{
			    yferr_log("QNX", "DRIVER", "BACKLIGHT", screen_mcu_p->name, 0);
			}
		}else{
			err_out("%s, busnum:%d, i2caddr:0x%02x, read MCU_REG_DIAG_READ_DTC_ADDRESS faided", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
		}
		pthread_mutex_unlock(&screen_mcu_p->lock);
	}
	pthread_cleanup_pop(0);
	return NULL;
}

static unsigned screen_mcu_yftech_attach(struct screen_mcu_drv *screen_mcu_p){
	info_out("%s, busnum:%d, i2caddr:0x%02x\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
	
	pthread_mutex_lock(&screen_mcu_p->lock);
	screen_mcu_p->attach_chg = -1;
	if(!dtc_irq_init(screen_mcu_p)){
		pthread_create(&screen_mcu_p->dtc_irq_thread, NULL, dtc_irq_thread, screen_mcu_p);	
	}
	pthread_cond_signal(&screen_mcu_p->request_ready_cond);
	pthread_mutex_unlock(&screen_mcu_p->lock);
	
	return 0;
}

static unsigned screen_mcu_yftech_detach(struct screen_mcu_drv *screen_mcu_p){
	info_out("%s, busnum:%d, i2caddr:0x%02x\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
	if(screen_mcu_p->dtc_irq_thread){
		pthread_cancel(screen_mcu_p->dtc_irq_thread);
		info_out("%s, busnum:%d, i2caddr:0x%02x 222222222222222\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
		pthread_join(screen_mcu_p->dtc_irq_thread, NULL);
	    info_out("%s, busnum:%d, i2caddr:0x%02x 333333333333333\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
	    pthread_mutex_lock(&screen_mcu_p->lock);
		screen_mcu_p->dtc_irq_thread=0;
		pthread_mutex_unlock(&screen_mcu_p->lock);
	}

	return 0;
}


static unsigned screen_mcu_temperature_set(char *recv_msg, size_t size, off64_t *offset, void *file_ctx){
    struct screen_mcu_drv *screen_mcu_p = file_ctx;
	unsigned nbytes = strlen(recv_msg);
	int le = nbytes - 1;
	while (le > 0 && isspace(recv_msg[le]))
		recv_msg[le--] = 0;
	return nbytes;
}

static unsigned screen_mcu_temperature_get(char *reply_msg, size_t size, off64_t *offset, void *file_ctx){
    unsigned nbytes=0;

	if (*offset != 0)
		return 0;
	struct screen_mcu_drv *screen_mcu_p = file_ctx;
	struct did_info *did_info_p = screen_mcu_p->priv_data;
	pthread_mutex_lock(&screen_mcu_p->lock);
	short temperature = (did_info_p->temperature[0] << 8) | did_info_p->temperature[1];
	nbytes = snprintf(reply_msg, 24, "%d\n", temperature);
	*offset += nbytes;
	pthread_mutex_unlock(&screen_mcu_p->lock);	
	
	return min(nbytes, size);
}

static unsigned screen_mcu_illuminance_set(char *recv_msg, size_t size, off64_t *offset, void *file_ctx){
    struct screen_mcu_drv *screen_mcu_p = file_ctx;
	unsigned nbytes = strlen(recv_msg);
	int le = nbytes - 1;
	while (le > 0 && isspace(recv_msg[le]))
		recv_msg[le--] = 0;
	return nbytes;
}

static unsigned screen_mcu_illuminance_get(char *reply_msg, size_t size, off64_t *offset, void *file_ctx){
    unsigned nbytes=0;

	if (*offset != 0)
		return 0;
	struct screen_mcu_drv *screen_mcu_p = file_ctx;
	uint8_t buf[5] = {0};
	int illuminance = 0;
	pthread_mutex_lock(&screen_mcu_p->lock);
	if(screen_mcu_p->status){
	    if(!i2c_read_bytes(screen_mcu_p, screen_mcu_p->i2caddr, MCU_REG_ILLUMINANCE_ADDRESS, buf, 5)){
	        illuminance = (buf[1]<<24)|(buf[2]<<16)|(buf[3]<<8)|(buf[4]);
	        nbytes = snprintf(reply_msg, size, "%d\n", illuminance);
		    *offset += nbytes;
	    }
	}
	pthread_mutex_unlock(&screen_mcu_p->lock);	
	
	return min(nbytes, size);
}

unsigned screen_mcu_yftech_init(struct screen_mcu_drv *screen_mcu_p){
    struct stat st;
    char file_name[MAX_FILE_NAME_LEN];
    screen_mcu_p->attach_handler = &screen_mcu_yftech_attach;
    screen_mcu_p->set_request_handler = &set_request_yftech;
	screen_mcu_p->get_reply_handler = &get_reply_yftech;
	screen_mcu_p->detach_handler = &screen_mcu_yftech_detach;
	
	if(screen_mcu_p->i2cdev <= 0){
	    screen_mcu_p->i2cdev = get_i2cdev_with_bus(screen_mcu_p->busnum);
	    if(screen_mcu_p->i2cdev <= 0){
		    err_out("%s, busnum:%d, i2caddr:0x%02x, can't open i2cdev:%d\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, screen_mcu_p->i2cdev);
		    return -1;	
	    }
	}
	
	if(!screen_mcu_p->priv_data){
	    struct did_info *did_info_p = (struct did_info *)malloc(sizeof(struct did_info));
	    memset(did_info_p, 0x00, sizeof(struct did_info));
	    screen_mcu_p->priv_data = did_info_p;
	}
	
	if(!screen_mcu_p->request_handler_thread){
		pthread_create(&screen_mcu_p->request_handler_thread, NULL, request_handler_thread, screen_mcu_p);
	}
	
	if (stat("/proc/mount/dev/screen_mcu", &st)) {
		    pdbg_init("/dev/screen_mcu"); //create a node for user function
		    usleep(20000);
	}
	
	 struct pdbg_ops_s ops_temperature;
	 memset(&ops_temperature, 0x0, sizeof(struct pdbg_ops_s));
	 ops_temperature.read = screen_mcu_temperature_get;
	 ops_temperature.write  = screen_mcu_temperature_set;
	 snprintf(file_name, MAX_FILE_NAME_LEN, "/%s/temperature", screen_mcu_p->name);
	 pdbg_create_file(file_name, 0666, ops_temperature, screen_mcu_p);		
	
	if(screen_mcu_p->illuminance_sensor){    
	    struct pdbg_ops_s ops_illuminance;
	    memset(&ops_illuminance, 0x0, sizeof(struct pdbg_ops_s));
	    ops_illuminance.read = screen_mcu_illuminance_get;
	    ops_illuminance.write  = screen_mcu_illuminance_set;
	    snprintf(file_name, MAX_FILE_NAME_LEN, "/%s/illuminance", screen_mcu_p->name);
	    pdbg_create_file(file_name, 0666, ops_illuminance, screen_mcu_p);
	}	
	
	return 0;
}



