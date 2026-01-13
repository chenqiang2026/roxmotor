#include <ctype.h>
#include <gpio_client.h>
#include <sys/mman.h>
#include <hw/inout.h>
#include <sys/iofunc.h>
#include "screen_mcu_hangsheng.h"
#define MCU_REG_DIAG_WRITE_ADDRESS			0x40	//diagnosis wirte
#define MCU_REG_DIAG_READ_ADDRESS			0x41	//diagnosis	read
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

static int set_request_hangsheng(void *screen_mcu_drv, char *request){
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
static int get_reply_hangsheng(void *screen_mcu_drv, char *reply_msg){
	struct screen_mcu_drv *screen_mcu_p = (struct screen_mcu_drv *)screen_mcu_drv;
	unsigned nbytes=0;
	struct huangsheng_did_info *did_info_p = screen_mcu_p->priv_data;
	
	pthread_mutex_lock(&screen_mcu_p->lock);
	info_out("%s, busnum:%d, i2caddr:0x%02x, screen_mcu_p->request:%s\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, screen_mcu_p->request);
	if(!strcmp(screen_mcu_p->request, "lcd get show sver")){
		nbytes = snprintf(reply_msg, 24, "%s\n", did_info_p->sw_ver);
	}else if(!strcmp(screen_mcu_p->request, "lcd get show hver")){
		nbytes = snprintf(reply_msg, 24, "%s\n", did_info_p->hw_ver);
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
	}
	pthread_mutex_unlock(&screen_mcu_p->lock);
	info_out("%s, busnum:%d, i2caddr:0x%02x, nbytes%d, reply_msg:%s\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, nbytes, reply_msg);
	return nbytes;
}
  
static int hangsheng_get_did_info(struct screen_mcu_drv *screen_mcu_p, const uint16_t did_number, uint8_t * rxdata){
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
				info_out("hangsheng_get_diag_info success len:%d\n", buf[0]);
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

static int handle_request(struct screen_mcu_drv *screen_mcu_p){
	struct huangsheng_did_info *did_info_p = screen_mcu_p->priv_data;

	if(!strcmp(screen_mcu_p->request, "lcd get show sver")){
		hangsheng_get_did_info(screen_mcu_p, DID_NUMBER_SOFTWARE_VERSION, did_info_p->sw_ver);
	}else if(!strcmp(screen_mcu_p->request, "lcd get show hver")){
		hangsheng_get_did_info(screen_mcu_p, DID_NUMBER_HARDWARE_VERSION, did_info_p->hw_ver);
	}else if(!strcmp(screen_mcu_p->request, "lcd get supplier sver")){
		hangsheng_get_did_info(screen_mcu_p, DID_NUMBER_SUPPLIER_SOFTWARE_VERSION, did_info_p->supplier_sw_ver);
	}else if(!strcmp(screen_mcu_p->request, "lcd get supplier hver")){
		hangsheng_get_did_info(screen_mcu_p, DID_NUMBER_SUPPLIER_HARDWARE_VERSION, did_info_p->supplier_hw_ver);
	}else if(!strcmp(screen_mcu_p->request, "lcd get PN")){
		hangsheng_get_did_info(screen_mcu_p, DID_NUMBER_DISPLAY_PN, did_info_p->pn);
	}else if(!strcmp(screen_mcu_p->request, "lcd get SN")){
		hangsheng_get_did_info(screen_mcu_p, DID_NUMBER_DISPLAY_SN, did_info_p->sn);
	}else if(!strcmp(screen_mcu_p->request, "lcd get MFD")){
		hangsheng_get_did_info(screen_mcu_p, DID_NUMBER_MANUFACTURE_DATE, did_info_p->manufacture_date);
	}else if(!strcmp(screen_mcu_p->request, "lcd get tp ver")){
		hangsheng_get_did_info(screen_mcu_p, DID_NUMBER_TP_VERSION, did_info_p->tp_ver);
	}else if(!strcmp(screen_mcu_p->request, "lcd get temperature")){
		hangsheng_get_did_info(screen_mcu_p, DID_NUMBER_TEMPERATURE, did_info_p->temperature);
	}else if(!strcmp(screen_mcu_p->request, "lcd get session")){
		hangsheng_get_did_info(screen_mcu_p, DID_NUMBER_SESSION, did_info_p->session);
	}else if(!strncmp(screen_mcu_p->request, "lcd set MFD", strlen("lcd set MFD"))){
		set_mfd_info(screen_mcu_p, &(screen_mcu_p->request[12]));
	}else if(!strncmp(screen_mcu_p->request, "lcd set SN", strlen("lcd set SN"))){
		set_sn_info(screen_mcu_p, &(screen_mcu_p->request[11]));
	}else if(!strncmp(screen_mcu_p->request, "lcd acc on", strlen("lcd acc on"))){
		set_acc_state(screen_mcu_p, 1);
	}else if(!strncmp(screen_mcu_p->request, "lcd acc off", strlen("lcd acc off"))){
		set_acc_state(screen_mcu_p, 0);
	}else{
	
	}
	return 0;
}

static void *request_handler_thread(void *thread_param){
	int wret;
	struct screen_mcu_drv *screen_mcu_p = thread_param;
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

static int dtc_irq_init(struct screen_mcu_drv *screen_mcu_p)
{
    int fd;
    char *name = "/dev/gpio";
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

static void *dtc_irq_thread(void *thread_param){
	int err;
	struct screen_mcu_drv *screen_mcu_p = thread_param;
	//struct huangsheng_did_info *did_info_p = screen_mcu_p->priv_data;
	struct sigevent event;
	//uint8_t buf[16] = {0};
	pthread_cleanup_push(dtc_cleanup, screen_mcu_p);
	SIGEV_INTR_INIT(&event);
    screen_mcu_p->dtc_irq_id = InterruptAttachEvent(screen_mcu_p->dtc_irq_num, &event, _NTO_INTR_FLAGS_TRK_MSK);
    InterruptMask(screen_mcu_p->dtc_irq_num, screen_mcu_p->dtc_irq_id);
	info_out("%s, busnum:%d, i2caddr:0x%02x,gpio %d id %d\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, screen_mcu_p->dtc_irq_gpio, screen_mcu_p->dtc_irq_id);
	while(1){
		do{
            err = InterruptUnmask(screen_mcu_p->dtc_irq_num, screen_mcu_p->dtc_irq_id);
			if(err != 0) {	
				err_out("%s, busnum:%d, i2caddr:0x%02x,InterruptUnmask %d\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, err);
			}
		}while(err > 0);
        err = InterruptWait_r(0, NULL );
        pthread_mutex_lock(&screen_mcu_p->lock);
		info_out("%s, busnum:%d, i2caddr:0x%02x,process irq %d\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, err);
		/*/
		if(!i2c_read_bytes(screen_mcu_p, screen_mcu_p->i2caddr, MCU_REG_DIAG_READ_DTC_ADDRESS, buf, 16)){
			info_out("%s, busnum:%d, i2caddr:0x%02x, DTC reg buf:0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x \n", __func__,
			screen_mcu_p->busnum, screen_mcu_p->i2caddr, buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8], buf[9], buf[10], buf[11], buf[12], buf[13], buf[14], buf[15]);
			snprintf(did_info_p->dialg_code, 128, "0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x",buf[0], buf[1], buf[2], 
			buf[3], buf[4], buf[5], buf[6], buf[7], buf[8], buf[9], buf[10], buf[11], buf[12], buf[13], buf[14], buf[15]);
		}else{
			err_out("%s, busnum:%d, i2caddr:0x%02x, read MCU_REG_DIAG_READ_DTC_ADDRESS faided", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
		}
		/*/
		pthread_mutex_unlock(&screen_mcu_p->lock);
	}
	pthread_cleanup_pop(0);
	return NULL;
}

static int start_mtouch(struct screen_mcu_drv *screen_mcu_p){
	char mtouch_cfg_path[128];
	snprintf(mtouch_cfg_path, sizeof(mtouch_cfg_path), "/dev/mtouch-proxy/%s", screen_mcu_p->name);
	int fd = open(mtouch_cfg_path, O_RDWR);
	if(fd>=0){
		lseek(fd, 0, SEEK_SET);
		write(fd, "/etc/system/config/mtouch_mcu_proxy_panel1p0.conf", strlen("/etc/system/config/mtouch_mcu_proxy_panel1p0.conf")+1);
		close(fd);
		return 0;			
	}
	return 0;
}

static int tp_irq_init(struct screen_mcu_drv *screen_mcu_p)
{
    int fd;
    int share_fd;
    char shm_name_buf[32]="tp_";
    char *name = "/dev/gpio";
    struct huangsheng_did_info *did_info_p = screen_mcu_p->priv_data;
	if(screen_mcu_p->tp_irq_gpio<=0){
		return -1;	
	}
	screen_mcu_p->tp_irq_type = GPIO_INTERRUPT_TRIGGER_HIGH;
    fd = gpio_open(name);
    if(fd >= 0) {
        int err = gpio_set_interrupt_cfg(fd, screen_mcu_p->tp_irq_gpio, screen_mcu_p->tp_irq_type, NULL);
        if(err == EOK) {
            err = gpio_get_interrupt_cfg(fd, screen_mcu_p->tp_irq_gpio, &screen_mcu_p->tp_irq_num);
            if(err == EOK && screen_mcu_p->tp_irq_num > 0) {
				info_out("%s, busnum:%d, i2caddr:0x%02x,gpio %d num %d\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, screen_mcu_p->tp_irq_gpio, screen_mcu_p->tp_irq_num);
				strcat(shm_name_buf,screen_mcu_p->name);
				shm_unlink(shm_name_buf);
				share_fd = shm_open(shm_name_buf, O_RDWR | O_CREAT, 0666);
				ftruncate(share_fd, sizeof(screen_mcu_tp_t));
				did_info_p->tp_info = mmap(NULL, sizeof(screen_mcu_tp_t), PROT_READ | PROT_WRITE, MAP_SHARED, share_fd, 0);
				close(share_fd);
				sem_init(&did_info_p->tp_info->notify_sem, 1, 0);
				sem_init(&did_info_p->tp_info->mutex, 1, 1);
				//start_mtouch(screen_mcu_p);
                return 0;
            }
            else {
				err_out("%s, busnum:%d, i2caddr:0x%02x, get gpio %d num %d fail\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, screen_mcu_p->tp_irq_gpio, screen_mcu_p->tp_irq_num);
            }
        }
        else {
			err_out("%s, busnum:%d, i2caddr:0x%02x, set gpio %d type %d fail\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, screen_mcu_p->tp_irq_gpio, screen_mcu_p->tp_irq_type);
        }
        gpio_close(name);
    }
    else {
		err_out("%s, busnum:%d, i2caddr:0x%02x, gpio_open fail %d\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, fd);
    }
    return -1;
}

static void tp_cleanup(void *arg){
    struct screen_mcu_drv *screen_mcu_p = arg;
    info_out("%s, busnum:%d, i2caddr:0x%02x\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
    pthread_mutex_unlock(&screen_mcu_p->lock);
}

static int mcu_ts_read_data(struct screen_mcu_drv *screen_mcu_p){
    int ret = -1;
    struct huangsheng_did_info *did_info_p = screen_mcu_p->priv_data;
    screen_mcu_tp_t *tp_info = did_info_p->tp_info;
    uint8_t buf[64];
    int notify_sem_value;
    pthread_mutex_lock(&screen_mcu_p->lock);
    ret = i2c_read_bytes(screen_mcu_p, screen_mcu_p->i2caddr, 0x20, buf, 63);
    pthread_mutex_unlock(&screen_mcu_p->lock);
    if(!ret){
        sem_wait(&tp_info->mutex);
        uint8_t pc;
		uint8_t i;
		uint8_t *tp_data;
		pc = buf[1] & 0xf;
		tp_info->pointer_num = pc;
		tp_data = buf + 2;
		for (i=0; i<pc; i++, tp_data+=6) {
			tp_info->events[i].id = tp_data[0];
			tp_info->events[i].status = tp_data[1];
			tp_info->events[i].x = (tp_data[2]<<8) | tp_data[3];
			tp_info->events[i].y = (tp_data[4]<<8) | tp_data[5];
			info_out("%s %s %s p %d i %d s %d x %d y %d\n", __func__, screen_mcu_p->name, __func__, i, tp_info->events[i].id, tp_info->events[i].status, tp_info->events[i].x, tp_info->events[i].y);
		}
		sem_getvalue(&tp_info->notify_sem, &notify_sem_value);
		if(notify_sem_value != 0){
		    warn_out("%s %s notify_sem_value:%d, tp data isn't consumed\n", screen_mcu_p->name, __func__, notify_sem_value);
		}else{
		    sem_post(&tp_info->notify_sem);
		}
		sem_post(&tp_info->mutex);
		return pc;
    }
    
    return ret;
}

static void *tp_irq_thread(void *thread_param){
	int err;
	struct screen_mcu_drv *screen_mcu_p = thread_param;
	struct sigevent event;
	pthread_cleanup_push(tp_cleanup, screen_mcu_p);
	SIGEV_INTR_INIT(&event);
    screen_mcu_p->tp_irq_id = InterruptAttachEvent(screen_mcu_p->tp_irq_num, &event, _NTO_INTR_FLAGS_TRK_MSK);
    InterruptMask(screen_mcu_p->tp_irq_num, screen_mcu_p->tp_irq_id);
	info_out("%s, busnum:%d, i2caddr:0x%02x,gpio %d id %d\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, screen_mcu_p->tp_irq_gpio, screen_mcu_p->tp_irq_id);
	while(1){
		do{
            err = InterruptUnmask(screen_mcu_p->tp_irq_num, screen_mcu_p->tp_irq_id);
			if(err != 0) {	
				err_out("%s, busnum:%d, i2caddr:0x%02x,InterruptUnmask %d\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, err);
			}
		}while(err > 0);
        err = InterruptWait_r(0, NULL );
        info_out("%s, busnum:%d, i2caddr:0x%02x,process irq %d\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, err);
        mcu_ts_read_data(screen_mcu_p);
	}
	pthread_cleanup_pop(0);
	return NULL;
}

static unsigned screen_mcu_hangsheng_attach(struct screen_mcu_drv *screen_mcu_p){
	info_out("%s, busnum:%d, i2caddr:0x%02x\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
	
	pthread_mutex_lock(&screen_mcu_p->lock);
	screen_mcu_p->attach_chg = -1;
	if(!dtc_irq_init(screen_mcu_p)){
		pthread_create(&screen_mcu_p->dtc_irq_thread, NULL, dtc_irq_thread, screen_mcu_p);	
	}
	if(!tp_irq_init(screen_mcu_p)){
		pthread_create(&screen_mcu_p->tp_irq_thread, NULL, tp_irq_thread, screen_mcu_p);	
	}
	pthread_cond_signal(&screen_mcu_p->request_ready_cond);
	pthread_mutex_unlock(&screen_mcu_p->lock);
	
	return 0;
}

static unsigned screen_mcu_hangsheng_detach(struct screen_mcu_drv *screen_mcu_p){
	info_out("%s, busnum:%d, i2caddr:0x%02x\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
    //pthread_mutex_lock(&screen_mcu_p->lock);
    struct huangsheng_did_info *did_info_p = screen_mcu_p->priv_data;
	if(screen_mcu_p->dtc_irq_thread){
		pthread_cancel(screen_mcu_p->dtc_irq_thread);
		pthread_join(screen_mcu_p->dtc_irq_thread, NULL);
		screen_mcu_p->dtc_irq_thread=0;
	}
	
	if(screen_mcu_p->tp_irq_thread){
		pthread_cancel(screen_mcu_p->tp_irq_thread);
		pthread_join(screen_mcu_p->tp_irq_thread, NULL);
		screen_mcu_p->tp_irq_thread=0;
	}
	if(did_info_p->tp_info){
	    munmap(did_info_p->tp_info, sizeof(screen_mcu_tp_t));
	    did_info_p->tp_info = NULL;
	}
	//pthread_mutex_unlock(&screen_mcu_p->lock);
	return 0;
}
unsigned screen_mcu_hangsheng_init(struct screen_mcu_drv *screen_mcu_p){
    screen_mcu_p->attach_handler = &screen_mcu_hangsheng_attach;
    screen_mcu_p->set_request_handler = &set_request_hangsheng;
	screen_mcu_p->get_reply_handler = &get_reply_hangsheng;
	screen_mcu_p->detach_handler = &screen_mcu_hangsheng_detach;
	
	if(screen_mcu_p->i2cdev <= 0){
	    screen_mcu_p->i2cdev = get_i2cdev_with_bus(screen_mcu_p->busnum);
	    if(screen_mcu_p->i2cdev <= 0){
		    err_out("%s, busnum:%d, i2caddr:0x%02x, can't open i2cdev:%d\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, screen_mcu_p->i2cdev);
		    return -1;	
	    }
	}
	
	if(!screen_mcu_p->priv_data){
	    struct huangsheng_did_info *did_info_p = (struct huangsheng_did_info *)malloc(sizeof(struct huangsheng_did_info));
	    memset(did_info_p, 0x00, sizeof(struct huangsheng_did_info));
	    screen_mcu_p->priv_data = did_info_p;
	}
	
	if(!screen_mcu_p->request_handler_thread){
		pthread_create(&screen_mcu_p->request_handler_thread, NULL, request_handler_thread, screen_mcu_p);
	}
	return 0;
}



