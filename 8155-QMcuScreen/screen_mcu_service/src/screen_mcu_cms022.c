#include <ctype.h>
#include <gpio_client.h>
#include <sys/mman.h>
#include <hw/inout.h>
#include <sys/iofunc.h>
#include <amss/core/pdbg.h>
#include "screen_mcu_cms022.h"

static int set_request_cms022(void *screen_mcu_drv, char *request){
	struct screen_mcu_drv *screen_mcu_p = (struct screen_mcu_drv *)screen_mcu_drv;
	unsigned nbytes = strlen(request);
	int le = nbytes - 1;
	while (le > 0 && isspace(request[le]))
		request[le--] = 0;
	pthread_mutex_lock(&screen_mcu_p->lock);
	info_out("%s, busnum:%d, i2caddr:0x%02x,request:%s\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, request);
	if(!strcmp(request, "lcd open backlight")){
	    screen_mcu_p->pwr_chg = 1;
		screen_mcu_p->bl_on = 1;
	}else if(!strcmp(request, "lcd close backlight")){
	    screen_mcu_p->pwr_chg = 1;
		screen_mcu_p->bl_on = 0;
	}else if(!strncmp(request, "lcd backlight ", 14)){
	    screen_mcu_p->bkl_chg = 1;
	    screen_mcu_p->bl = strtol(&(request[14]), NULL, 0);
	}else{
	    if(strlen(request)<REQUEST_LEN){
	        strcpy(screen_mcu_p->request, request);
	        screen_mcu_p->request_chg = -1;
	    }else{
	        err_out("%s, busnum:%d, i2caddr:0x%02x,request too long ignore, request:%s\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, request);
	    }
	}
	pthread_cond_signal(&screen_mcu_p->request_ready_cond);
	pthread_mutex_unlock(&screen_mcu_p->lock);

	return nbytes;
}
static int get_reply_cms022(void *screen_mcu_drv, char *reply_msg){
	struct screen_mcu_drv *screen_mcu_p = (struct screen_mcu_drv *)screen_mcu_drv;
	unsigned nbytes=0;
	
	pthread_mutex_lock(&screen_mcu_p->lock);
	info_out("%s, busnum:%d, i2caddr:0x%02x, screen_mcu_p->request:%s\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, screen_mcu_p->request);
	pthread_mutex_unlock(&screen_mcu_p->lock);
	info_out("%s, busnum:%d, i2caddr:0x%02x, nbytes%d, reply_msg:%s\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, nbytes, reply_msg);
	return nbytes;
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

static int setbacklightonoff(struct screen_mcu_drv *screen_mcu_p, uint8_t onoff){
	uint8_t vals[] = {0x04, 0x03, 0x00, !!(onoff), 0};
	vals[4] = mcu_calc_crc8(vals, 4);

	return i2c_write_bat(screen_mcu_p, screen_mcu_p->i2caddr, vals, 5);
}

static int getbacklightonoff(struct screen_mcu_drv *screen_mcu_p, uint8_t* onoff){
    int ret;
    uint8_t buf[2] = {0};
    ret = i2c_read_bytes(screen_mcu_p, screen_mcu_p->i2caddr, 0x04, buf, 2);
    if(!ret){
        *onoff = buf[1];
    }
    info_out("%s, busnum:%d, i2caddr:0x%02x,*onoff:%d ret:%d\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, *onoff, ret);
    return ret;
}


static int setbacklight(struct screen_mcu_drv *screen_mcu_p, uint8_t bklight){
	uint8_t vals[] = {0x03, 0x03, 0x00, bklight, 0};
	vals[4] = mcu_calc_crc8(vals, 4);

	return i2c_write_bat(screen_mcu_p, screen_mcu_p->i2caddr, vals, 5);
}

static int getbacklight(struct screen_mcu_drv *screen_mcu_p, uint8_t* bklight){
    int ret;
    uint8_t buf[2] = {0};
    ret = i2c_read_bytes(screen_mcu_p, screen_mcu_p->i2caddr, 0x03, buf, 2);
    if(!ret){
        *bklight = buf[1];
    }
    info_out("%s, busnum:%d, i2caddr:0x%02x,*bklight:%d ret:%d\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, *bklight, ret);
    return ret;
}


static int heartbeat(struct screen_mcu_drv *screen_mcu_p){
	uint8_t vals[] = {0x60, 0x00};

	return i2c_write_bat(screen_mcu_p, screen_mcu_p->i2caddr, vals, 2);
}


static int handle_request(struct screen_mcu_drv *screen_mcu_p){
	return 0;
}

static void *request_handler_thread(void *thread_param){
	int wret;
	struct screen_mcu_drv *screen_mcu_p = thread_param;
	pthread_mutex_lock(&screen_mcu_p->lock);
	do{
	    if(screen_mcu_p->status){
		    if(screen_mcu_p->attach_chg){
			    screen_mcu_p->attach_chg = 0;
		    }
		    
		    if(screen_mcu_p->request_chg){
			    if(!handle_request(screen_mcu_p)){
				    screen_mcu_p->request_chg = 0;	
			    }
		    }
		    
		    heartbeat(screen_mcu_p);
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

static unsigned screen_mcu_cms022_attach(struct screen_mcu_drv *screen_mcu_p){
	info_out("%s, busnum:%d, i2caddr:0x%02x\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
	
	pthread_mutex_lock(&screen_mcu_p->lock);
	screen_mcu_p->attach_chg = -1;
	
	pthread_cond_signal(&screen_mcu_p->request_ready_cond);
	pthread_mutex_unlock(&screen_mcu_p->lock);
	
	return 0;
}

static unsigned screen_mcu_cms022_detach(struct screen_mcu_drv *screen_mcu_p){
	info_out("%s, busnum:%d, i2caddr:0x%02x\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);

	return 0;
}

unsigned screen_mcu_cms022_init(struct screen_mcu_drv *screen_mcu_p){
    screen_mcu_p->attach_handler = &screen_mcu_cms022_attach;
    screen_mcu_p->set_request_handler = &set_request_cms022;
	screen_mcu_p->get_reply_handler = &get_reply_cms022;
	screen_mcu_p->detach_handler = &screen_mcu_cms022_detach;
	
	if(screen_mcu_p->i2cdev <= 0){
	    screen_mcu_p->i2cdev = get_i2cdev_with_bus(screen_mcu_p->busnum);
	    if(screen_mcu_p->i2cdev <= 0){
		    err_out("%s, busnum:%d, i2caddr:0x%02x, can't open i2cdev:%d\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, screen_mcu_p->i2cdev);
		    return -1;	
	    }
	}
	
	if(!screen_mcu_p->request_handler_thread){
		pthread_create(&screen_mcu_p->request_handler_thread, NULL, request_handler_thread, screen_mcu_p);
	}
	
	return 0;
}



