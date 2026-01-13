#include <ctype.h>
#include <gpio_client.h>
#include <sys/mman.h>
#include <hw/inout.h>
#include <libgen.h>
#include <sys/iofunc.h>
#include <amss/core/pdbg.h>
#include "screen_mcu_chery.h"

#define MCU_BLOCK_LEN 128

#define MAX_FILE_NAME_LEN	24

#define CHERRY_UPDATE_WAIT_TIMEOUT_US 26000000

static uint8_t calCRC(uint8_t* buf, int len){
    uint8_t crc = 0;
    int i = 0;
    for(;i<len;i++){
        crc = crc + buf[i];
    }
    crc++;
    
    return crc;
}

static int wait_time(struct screen_mcu_drv *screen_mcu_p, long us){
    int wret;
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    long dnsec = now.tv_nsec + us*1000;
    
    long sec = dnsec/(1000000000l);
    long nsec = dnsec%(1000000000l);
    now.tv_sec = now.tv_sec + sec;
    now.tv_nsec = nsec;
    wret = pthread_cond_timedwait(&screen_mcu_p->request_ready_cond, &screen_mcu_p->lock, &now);
    
    return wret;
}

int yferr_log(const char *os, const char *type, const char *subsys, const char *name, int errcode);
static uint8_t heartbeat_ok = 0;
static uint8_t heartbeat_check_start = 0;
static time_t heartbeat_check_reset_time = 0;
#define HEARTBEAT_TIMEOUT 10 

static void reset_heartbeat(){
    heartbeat_ok = 0;
    struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
    heartbeat_check_reset_time = now.tv_sec;
}

static void start_heartbeat_check(){
    reset_heartbeat();
    heartbeat_check_start = 1;
}

static void stop_heartbeat_check(){
    heartbeat_ok = 0;
    heartbeat_check_start = 0;
}

static void get_heartbeat(){
    heartbeat_ok = 1;
}

static void check_heartbeat(struct screen_mcu_drv *screen_mcu_p){
   struct chery_info *chery_info_p =  screen_mcu_p->priv_data;
   if(heartbeat_check_start) {
        char dtc_key[128];
    	snprintf(dtc_key, sizeof(dtc_key), "%s-comm", screen_mcu_p->name);
        struct timespec now;
	    clock_gettime(CLOCK_MONOTONIC, &now);
	    if(now.tv_sec - heartbeat_check_reset_time > HEARTBEAT_TIMEOUT){
	        if(!heartbeat_ok){
	            err_out("%s %s heartbeat timeout!!! chery_info_p->hw_ver:%d.%d\n", __func__, screen_mcu_p->name, chery_info_p->hw_ver[0], chery_info_p->hw_ver[1]);
	            if(chery_info_p->hw_ver[0] != 0 || chery_info_p->hw_ver[1] != 0){
	                yferr_log("QNX", "DRIVER", "DISPLAY", dtc_key, 0);
	            }
	        }else{
	            yferr_log("QNX", "DRIVER", "DISPLAY", dtc_key, 1);
	        }
	        reset_heartbeat();
	    }
   }
}


static int set_request_chery(void *screen_mcu_drv, char *request){
	struct screen_mcu_drv *screen_mcu_p = (struct screen_mcu_drv *)screen_mcu_drv;
	unsigned nbytes = strlen(request);
	if(screen_mcu_p->is_updating){
	    warn_out("%s, busnum:%d, i2caddr:0x%02x,updating ignore request:%s\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, request);
	    return nbytes;
	}
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
	        if(!strncmp(screen_mcu_p->request, "lcd update -p ",  strlen("lcd update -p ")) || !strncmp(screen_mcu_p->request, "lcd update tp -p ",  strlen("lcd update tp -p "))){
	            screen_mcu_p->is_updating = 1;
	        }
	    }else{
	        err_out("%s, busnum:%d, i2caddr:0x%02x,request too long ignore, request:%s\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, request);
	    }
	}
	pthread_cond_signal(&screen_mcu_p->request_ready_cond);
	pthread_mutex_unlock(&screen_mcu_p->lock);

	return nbytes;
}
static int get_reply_chery(void *screen_mcu_drv, char *reply_msg){
	struct screen_mcu_drv *screen_mcu_p = (struct screen_mcu_drv *)screen_mcu_drv;
	unsigned nbytes=0;
	pthread_mutex_lock(&screen_mcu_p->lock);
	nbytes = snprintf(reply_msg, 32, "%s\n", screen_mcu_p->reply);
	info_out("%s, busnum:%d, i2caddr:0x%02x, nbytes%d, reply_msg:%s\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, nbytes, reply_msg);
	pthread_mutex_unlock(&screen_mcu_p->lock);
	return nbytes;
}

static int do_control_action(struct screen_mcu_drv *screen_mcu_p, uint8_t power, uint8_t backlight_power, int backlight){
    uint8_t write_buf[7] = {0};
    write_buf[0] = 0x83;
    write_buf[1] = power;
    write_buf[2] = backlight & 0xff;
    write_buf[3] = (backlight>>8) & 0xff;
    write_buf[4] = backlight_power;
    write_buf[6] = calCRC(write_buf, 6);
    return i2c_write_bat(screen_mcu_p, screen_mcu_p->i2caddr, write_buf, 7);
}

static char * load_file(char * filename, int *len){
    struct stat sStat;
    int fileDes;
	char *buf = NULL;	
	
    memset((char *) &sStat, 0x00, sizeof(struct stat));
	
	 if (0 != stat(filename, &sStat)){
       if (ENOENT == errno) // The path doesn't exist, or path is an empty string
       {
            dbg_out("The config path doesn't exist, or path is an empty string\n");
       }else{
			err_out("The config path stat failed\n");
       }
       return NULL;
     }
	
	info_out("file filesize:%uB\n", (unsigned int)sStat.st_size);
    buf = (char *)malloc(sStat.st_size);
    if(buf == NULL){
    	err_out("malloc file_buf failed\n");
		return NULL;
    }
	
	memset(buf, 0x00, sStat.st_size);
	fileDes = open(filename, O_RDONLY, 0);
	
	if(fileDes==-1){
		err_out("The config path open failed\n");
		free(buf);
    	return NULL;
    }else{
		int num_read = read(fileDes, buf, sStat.st_size);
		close(fileDes);
		if(num_read != sStat.st_size){
			info_out("num_read != sStat.st_size\n");
		}else{
			*len = num_read;		
		}
	}
	
	return buf;
}

static int request_update(struct screen_mcu_drv *screen_mcu_p){
    uint8_t write_buf[7] = {0};
    write_buf[0] = 0xc0;
    write_buf[1] = 0x5a;
    write_buf[2] = 0x05;
    write_buf[3] = 0x00;
    write_buf[4] = 0x01;
    write_buf[5] = 0x01;
    write_buf[6] = calCRC(write_buf, 6);
    return i2c_write_bat(screen_mcu_p, screen_mcu_p->i2caddr, write_buf, 7);
}

static int check_bootloader_mode(struct screen_mcu_drv *screen_mcu_p){\
    uint8_t write_buf[6] = {0};
    write_buf[0] = 0xfb;
    write_buf[1] = 0x5a;
    write_buf[2] = 0x05;
    write_buf[3] = 0x05;
    write_buf[4] = 0x00;
    write_buf[5] = calCRC(write_buf, 5);
    return i2c_write_bat(screen_mcu_p, screen_mcu_p->i2caddr, write_buf, 6);
}

#define BREAK_ON_ERR(x)	\
	ret = x;	\
	if (ret)	\
		break

static int start_send_data(struct screen_mcu_drv *screen_mcu_p,char* flashDriver_buf, int data_len, int blocks_num){
     struct chery_info *chery_info_p = screen_mcu_p->priv_data;
     int ret = -1;
     uint8_t write_byte[134] = {0};
     do{
         write_byte[0] = 0xf2;
         write_byte[1] = 0x5a;
         write_byte[2] = 0x05;
         write_byte[3] = 0x05;
         write_byte[4] = 0x80;
         memcpy(&write_byte[5], flashDriver_buf, 128);
         write_byte[133] = calCRC(write_byte, 133);
         ret = i2c_write_bat(screen_mcu_p, screen_mcu_p->i2caddr, write_byte, 134);
         ret = wait_time(screen_mcu_p, CHERRY_UPDATE_WAIT_TIMEOUT_US);
         if(ret || chery_info_p->update_step != 2){
            ret = -1;
            break;
         }
         
         write_byte[0] = 0xf1;
         write_byte[1] = 0x5a;
         write_byte[2] = 0x05;
         write_byte[3] = 0x05;
         write_byte[4] = 0x03;
         write_byte[5] = 0x01;
         write_byte[6] = ((blocks_num-1)>>8)&0xff;
         write_byte[7] = (blocks_num-1)&0xff;
         write_byte[8] = calCRC(write_byte, 8);
         ret = i2c_write_bat(screen_mcu_p, screen_mcu_p->i2caddr, write_byte, 9);
         ret = wait_time(screen_mcu_p, CHERRY_UPDATE_WAIT_TIMEOUT_US);
         if(ret || chery_info_p->update_step != 3){
            ret = -1;
            break;
         }
     }while(0);
     
     return ret;
}

static int send_data(struct screen_mcu_drv *screen_mcu_p,uint8_t type, uint8_t* data, int data_len, int blocks_index){
    uint8_t write_byte[137] = {0};
    write_byte[0] = 0xf8;
    write_byte[1] = 0x5a;
    write_byte[2] = 0x06;
    write_byte[3] = 0x87;
    write_byte[4] = 0x83;
    write_byte[5] = (blocks_index>>8)&0xff;
    write_byte[6] = blocks_index&0xff;
    write_byte[7] = data_len&0xff;
    memcpy(&write_byte[8], data+blocks_index*MCU_BLOCK_LEN, data_len);
    write_byte[136] = calCRC(write_byte, 136);
    return i2c_write_bat(screen_mcu_p, screen_mcu_p->i2caddr, write_byte, 137);
}

static int send_data_file(struct screen_mcu_drv *screen_mcu_p, int type, char* flashDriver_buf, int flashDriver_buf_len){
     int ret =-1;
     int blocks_num = 0;
     struct chery_info *chery_info_p = screen_mcu_p->priv_data;
     int last_blocks_len = flashDriver_buf_len%MCU_BLOCK_LEN;
     if(last_blocks_len>0){
        blocks_num = flashDriver_buf_len/MCU_BLOCK_LEN+1;
     }else{
        blocks_num = flashDriver_buf_len/MCU_BLOCK_LEN;
        last_blocks_len = MCU_BLOCK_LEN;
     }
     uint8_t* tmp_flashDriver_buf = (uint8_t*)flashDriver_buf;
     do{
	      BREAK_ON_ERR(start_send_data(screen_mcu_p, flashDriver_buf, flashDriver_buf_len, blocks_num));
	      
	      do{
	         ret = wait_time(screen_mcu_p, CHERRY_UPDATE_WAIT_TIMEOUT_US);
             if(ret || chery_info_p->update_step != 4){
                if(chery_info_p->update_step == 6)
                    ret = 0;
                else
                    ret = -1;
                break;
             }
             if(chery_info_p->update_request_index == blocks_num-1){
	            BREAK_ON_ERR(send_data(screen_mcu_p, type, tmp_flashDriver_buf, last_blocks_len, chery_info_p->update_request_index));
	         }else{
	            BREAK_ON_ERR(send_data(screen_mcu_p, type, tmp_flashDriver_buf, MCU_BLOCK_LEN, chery_info_p->update_request_index));
	         }
             screen_mcu_p->update_progress = chery_info_p->update_request_index*100/blocks_num;
	         snprintf(screen_mcu_p->reply, 32, "update:%d", screen_mcu_p->update_progress);	 
          }while(1);
	 }while(0);
     
     if(ret){
        warn_out("%s, busnum:%d, i2caddr:0x%02x, send_data_file failed\n",  __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
     }
     return ret;
}

static int reset_mcu(struct screen_mcu_drv *screen_mcu_p){
    uint8_t write_byte[7] = {0};
    write_byte[0] = 0xca;
    write_byte[1] = 0x5a;
    write_byte[2] = 0x05;
    write_byte[3] = 0x00;
    write_byte[4] = 0x01;
    write_byte[5] = 0x01;
    write_byte[6] = calCRC(write_byte, 6);
    return i2c_write_bat(screen_mcu_p, screen_mcu_p->i2caddr, write_byte, 7);
}

static int start_update(struct screen_mcu_drv *screen_mcu_p){
    int ret =-1;
    struct chery_info *chery_info_p = screen_mcu_p->priv_data;
    char updateFle_path[128]={'\0'};
    char * p_temp = strstr(screen_mcu_p->request, " -p ");
	snprintf(screen_mcu_p->reply, 32, "update:0");	            
	ret = wait_time(screen_mcu_p, 100000);
	if(p_temp){
	    strcpy(updateFle_path, p_temp+4);
	    info_out("%s, busnum:%d, i2caddr:0x%02x,updateFle_path:%s\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, updateFle_path);
	    
	    int update_buf_len = 0;
	    char* update_buf = load_file(updateFle_path, &update_buf_len);
	    if(update_buf){
	        do{
	            BREAK_ON_ERR(request_update(screen_mcu_p));
	            wait_time(screen_mcu_p, 2000000);
	            BREAK_ON_ERR(check_bootloader_mode(screen_mcu_p));
	            ret = wait_time(screen_mcu_p, CHERRY_UPDATE_WAIT_TIMEOUT_US);
	            if(ret || chery_info_p->update_step != 1){
	                break;
	            }
	            BREAK_ON_ERR(send_data_file(screen_mcu_p, 0x03, update_buf, update_buf_len));
	            usleep(1000000);
	            BREAK_ON_ERR(reset_mcu(screen_mcu_p));
	            chery_info_p->update_step = 7;
	        }while(0);
	    }
	    if(update_buf){
	        free(update_buf);
	    }
	}
	if(ret){
	    snprintf(screen_mcu_p->reply, 32, "update:failed");	            
	    err_out("%s, busnum:%d, i2caddr:0x%02x,update failed update_step:%d\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, chery_info_p->update_step);
	}else{
	    snprintf(screen_mcu_p->reply, 32, "update:success");	
	    err_out("%s, busnum:%d, i2caddr:0x%02x,update success\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
	}
	screen_mcu_p->is_updating = 0;
    return ret;
}

static int handle_request(struct screen_mcu_drv *screen_mcu_p){
    struct chery_info *chery_info_p = screen_mcu_p->priv_data;
	if(!strcmp(screen_mcu_p->request, "lcd power on")){
		do_control_action(screen_mcu_p, 1, screen_mcu_p->bl_on?1:0, screen_mcu_p->bl);				
	}else if(!strcmp(screen_mcu_p->request, "lcd power off")){
		do_control_action(screen_mcu_p, 0, screen_mcu_p->bl_on?1:0, screen_mcu_p->bl);
	}else if(!strcmp(screen_mcu_p->request, "lcd get supplier sver")){
		 snprintf(screen_mcu_p->reply, 32, "%02d.%02d.%02d", chery_info_p->sw_ver[0], chery_info_p->screen_type, chery_info_p->sw_ver[1]);
	}else if(!strcmp(screen_mcu_p->request, "lcd get supplier hver")){
		 snprintf(screen_mcu_p->reply, 32, "%02d.%02d.%02d", chery_info_p->hw_ver[0], chery_info_p->screen_type, chery_info_p->hw_ver[1]);
	}else if(!strcmp(screen_mcu_p->request, "lcd get bridge ver")){
		 snprintf(screen_mcu_p->reply, 32, "%02d.%02d.%02d", chery_info_p->bridge_ver[0], chery_info_p->bridge_ver[1], chery_info_p->bridge_ver[2]);
	}else if(!strcmp(screen_mcu_p->request, "lcd get tp ver")){
		 snprintf(screen_mcu_p->reply, 32, "%02d.%02d.%02d", chery_info_p->tp_ver[0], chery_info_p->tp_ver[1], chery_info_p->tp_ver[2]);
	}else if(!strncmp(screen_mcu_p->request, "lcd update -p ",  strlen("lcd update -p "))){
	    stop_heartbeat_check();
	    start_update(screen_mcu_p);
	}else{
	    snprintf(screen_mcu_p->reply, 32, "unknown");
	}
	return 0;
}

static int request_display_info(struct screen_mcu_drv *screen_mcu_p){
    uint8_t write_buf[7] = {0};
    
    write_buf[0] = 0x81;
    write_buf[6] = calCRC(write_buf, 6);
    return i2c_write_bat(screen_mcu_p, screen_mcu_p->i2caddr, write_buf, 7);
}
static int is_versions_valid(struct screen_mcu_drv *screen_mcu_p){
    struct chery_info *chery_info_p =  screen_mcu_p->priv_data;
    
    return  chery_info_p->bridge_ver[0] != 0 ||
                chery_info_p->bridge_ver[1] != 0 ||
                chery_info_p->bridge_ver[2] != 0 ||
                chery_info_p->hw_ver[0] != 0 ||
                chery_info_p->hw_ver[1] != 0 ||
                chery_info_p->sw_ver[0] != 0 ||
                chery_info_p->sw_ver[1] != 0 ||
                chery_info_p->tp_ver[0] != 0 ||
                chery_info_p->tp_ver[1] != 0 ||
                chery_info_p->tp_ver[2] != 0;
    
}

static int init_versions(struct screen_mcu_drv *screen_mcu_p){
    struct chery_info *chery_info_p =  screen_mcu_p->priv_data;
    chery_info_p->bridge_ver[0] = 0;
    chery_info_p->bridge_ver[1] = 0;
    chery_info_p->bridge_ver[2] = 0;
    chery_info_p->hw_ver[0] = 0;
    chery_info_p->hw_ver[1] = 0;
    chery_info_p->sw_ver[0] = 0;
    chery_info_p->sw_ver[1] = 0;
    chery_info_p->tp_ver[0] = 0;
    chery_info_p->tp_ver[1] = 0;
    chery_info_p->tp_ver[2] = 0;
    chery_info_p->screen_type = 1;
    return 0;
}

static void *request_handler_thread(void *thread_param){
	int wret;
	struct screen_mcu_drv *screen_mcu_p = thread_param;
	//struct chery_info *chery_info_p =  screen_mcu_p->priv_data;
	pthread_mutex_lock(&screen_mcu_p->lock);
	do{
	    if(screen_mcu_p->status){
		    if(screen_mcu_p->attach_chg){
		        if(!request_display_info(screen_mcu_p)){
			        screen_mcu_p->attach_chg = 0;
			    }
		    }else{
		        if(!is_versions_valid(screen_mcu_p)){
		            request_display_info(screen_mcu_p);
		        }
		    }
		    if(screen_mcu_p->pwr_chg){
		        if(!do_control_action(screen_mcu_p, 1, screen_mcu_p->bl_on?1:0, screen_mcu_p->bl)){
		            screen_mcu_p->pwr_chg = 0;
		        }
		    }
		    if(screen_mcu_p->bkl_chg){
		        if(!do_control_action(screen_mcu_p, 1, screen_mcu_p->bl_on?1:0, screen_mcu_p->bl)){
		            screen_mcu_p->bkl_chg = 0;
		        }
		    }
		    if(screen_mcu_p->request_chg){
			    if(!handle_request(screen_mcu_p)){
				    screen_mcu_p->request_chg = 0;	
			    }
		    }
		    
		    check_heartbeat(screen_mcu_p);
		}
		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);
		now.tv_sec=now.tv_sec+1;
		wret = pthread_cond_timedwait(&screen_mcu_p->request_ready_cond, &screen_mcu_p->lock, &now);
		//info_out("%s, busnum:%d, i2caddr:0x%02x, screen_mcu_p->status:%d screen_mcu_p->bl:%d, new request\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, screen_mcu_p->status, screen_mcu_p->bl);
		//wret = pthread_cond_wait(&screen_mcu_p->request_ready_cond, &screen_mcu_p->lock);
	}while(wret == 0 || wret == ETIMEDOUT);	

	info_out("%s, busnum:%d, i2caddr:0x%02x, exit request_handler_thread\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
	pthread_mutex_unlock(&screen_mcu_p->lock);
	
	return NULL;
}

static int tp_irq_init(struct screen_mcu_drv *screen_mcu_p)
{
    int fd;
    int share_fd;
    char shm_name_buf[32]="tp_";
    char *name = "/dev/gpio";
	if(screen_mcu_p->tp_irq_gpio<=0){
		return -1;	
	}
	struct chery_info *chery_info_p =  screen_mcu_p->priv_data;
	screen_mcu_p->tp_irq_type = GPIO_INTERRUPT_TRIGGER_FALLING;
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
				screen_mcu_tp_t* tp_info = mmap(NULL, sizeof(screen_mcu_tp_t), PROT_READ | PROT_WRITE, MAP_SHARED, share_fd, 0);
				memset(tp_info, 0, sizeof(screen_mcu_tp_t));
				close(share_fd);
				sem_init(&tp_info->notify_sem, 1, 0);
				sem_init(&tp_info->mutex, 1, 1);
				chery_info_p->tp_info = tp_info;
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

static int repo_ts_data(struct screen_mcu_drv *screen_mcu_p, uint8_t* buf, int len){
    struct chery_info *chery_info_p =  screen_mcu_p->priv_data;
    screen_mcu_tp_t *tp_info = chery_info_p->tp_info;
    int notify_sem_value;
    uint8_t pc = 0;
    uint8_t i = 0;
    uint8_t next_tp_len = 0;
    uint8_t *tp_data = NULL;
    uint8_t ext_buf[64] = {0};
    
    sem_wait(&tp_info->mutex);
    pc = buf[1] & 0xf;
    tp_info->pointer_num = pc;
    next_tp_len = buf[14];
    if(pc > 2 && pc < 11 && (next_tp_len == (pc-2)*6+3)){
        i2c_read_bytes(screen_mcu_p, screen_mcu_p->i2caddr, 0xfe, ext_buf, next_tp_len);
    }else{
        if(pc > 2){
            warn_out("%s %s:ts data erro\n", screen_mcu_p->name, __func__);
            sem_post(&tp_info->mutex);
            return 0;
        }
    }
    tp_data = buf + 2;
    do{
        tp_info->events[i].id = tp_data[0];
        tp_info->events[i].status = tp_data[1];
        tp_info->events[i].x = (tp_data[2]<<8) | tp_data[3];
        tp_info->events[i].y = (tp_data[4]<<8) | tp_data[5];
        if(i==1){
            tp_data = ext_buf + 1;
        }else{
            tp_data+=6;
        }
        //info_out("%s %s %s p %d i %d s %d x %d y %d\n", __func__, screen_mcu_p->name, __func__, i, tp_info->events[i].id, tp_info->events[i].status, tp_info->events[i].x, tp_info->events[i].y);
        i++;
    }while(i < pc);
    sem_getvalue(&tp_info->notify_sem, &notify_sem_value);
    if(notify_sem_value != 0){
        warn_out("%s %s notify_sem_value:%d, tp data isn't consumed\n", screen_mcu_p->name, __func__, notify_sem_value);
    }else{
        sem_post(&tp_info->notify_sem);
    }
    sem_post(&tp_info->mutex);
    return pc;
}

static int repo_mcu_log(struct screen_mcu_drv *screen_mcu_p, uint8_t* buf, int len){
    uint8_t next_tp_len = 0;
    uint8_t ext_buf[32] = {0};
    char log_buf[32] ={'\0'};
    next_tp_len = buf[14];
    
    strncpy(log_buf, (char*)&buf[1], 13);
    if(next_tp_len>3&&next_tp_len<21){
        if(!i2c_read_bytes(screen_mcu_p, screen_mcu_p->i2caddr, 0xfe, ext_buf, next_tp_len) && ext_buf[0] == 0x05){
            strncpy(log_buf+13, (char*)&ext_buf[1], next_tp_len-3);
        }else{
            return -1;
        }
    }else{
        return -1;
    }
    
    info_out("%s %s %s\n", __func__, screen_mcu_p->name, log_buf);
    return 0;
}

static int repo_mcu_dtc(struct screen_mcu_drv *screen_mcu_p, uint8_t* buf, int len){
    
    info_out("%s %s %2x %2x %2x %2x %2x %2x\n", __func__, screen_mcu_p->name, buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
    return 0;
}

static void lock_cleanup(void *arg){
    struct screen_mcu_drv *screen_mcu_p = arg;
    info_out("%s, busnum:%d, i2caddr:0x%02x\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
    pthread_mutex_unlock(&screen_mcu_p->lock);
}

static void *tp_irq_thread(void *thread_param){
	int err;
	uint8_t buf[16] = {0};
	uint8_t write_buf[7] = {0};
	struct screen_mcu_drv *screen_mcu_p = thread_param;
	struct chery_info *chery_info_p =  screen_mcu_p->priv_data;
	struct sigevent event;
	pthread_cleanup_push(lock_cleanup, screen_mcu_p);
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
        info_out("%s, busnum:%d, i2caddr:0x%02x,InterruptWait_r \n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
        pthread_mutex_lock(&screen_mcu_p->lock);
        if(!i2c_read_bytes(screen_mcu_p, screen_mcu_p->i2caddr, 0xfe, buf, 16)){
            info_out("%s, busnum:%d, i2caddr:0x%02x, cmd:0x%02x %d\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, buf[0], err);
            if(buf[0] == 0x10){
               if(buf[1] == 0x01){
                   write_buf[0] = 0x80;
                   write_buf[1] = 0x01;
                   write_buf[6] = calCRC(write_buf, 6);
                   i2c_write_bat(screen_mcu_p, screen_mcu_p->i2caddr, write_buf, 7);
               }
            }else if(buf[0] == 0x11){
                if(!do_control_action(screen_mcu_p, 1, screen_mcu_p->bl_on?1:0, screen_mcu_p->bl)){
                    get_heartbeat();
                }
            }else if(buf[0] == 0x01){
				get_heartbeat();
                repo_ts_data(screen_mcu_p, buf, 16); 
            }else if(buf[0] == 0x04){
                repo_mcu_log(screen_mcu_p, buf, 16);
            }else if(buf[0] == 0x9){
                chery_info_p->bridge_ver[0] = buf[1];
                chery_info_p->bridge_ver[1] = buf[2];
                chery_info_p->bridge_ver[2] = buf[3];
            }else if(buf[0] == 0x12){
            }else if(buf[0] == 0x13){
            }else if(buf[0] == 0x14){
                chery_info_p->screen_type = buf[3];
            }else if(buf[0] == 0x15){
                chery_info_p->hw_ver[0] = buf[1];
                chery_info_p->hw_ver[1] = buf[2];
                chery_info_p->sw_ver[0] = buf[3];
                chery_info_p->sw_ver[1] = buf[4];
                chery_info_p->tp_ver[0] = buf[5];
                chery_info_p->tp_ver[1] = buf[6];
                chery_info_p->tp_ver[2] = buf[7];
            }else if(buf[0] == 0x16){
                repo_mcu_dtc(screen_mcu_p, buf, 16);
            }
            //for update 
            else if(buf[0] == 0xfb){
                if(buf[5] == 0x01){
                    chery_info_p->update_step = 1;
                    pthread_cond_signal(&screen_mcu_p->request_ready_cond);
                }
            }else if(buf[0] == 0xf2){
                if(buf[5] == 0x01){
                    chery_info_p->update_step = 2;
                    pthread_cond_signal(&screen_mcu_p->request_ready_cond);
                }
            }else if(buf[0] == 0xf1){
                if(buf[5] == 0x01){
                    chery_info_p->update_step = 3;
                    pthread_cond_signal(&screen_mcu_p->request_ready_cond);
                }
            }else if(buf[0] == 0xf3){
                chery_info_p->update_step = 4;
                chery_info_p->update_request_index = (buf[5]<<8)|(buf[6]);
                pthread_cond_signal(&screen_mcu_p->request_ready_cond);
            }else if(buf[0] == 0xf4){
                if(buf[5] == 0x01){
                    chery_info_p->update_step = 6;
                }else{
                    chery_info_p->update_step = 5;
                }
                pthread_cond_signal(&screen_mcu_p->request_ready_cond);
            }
            //update end
         }
         pthread_mutex_unlock(&screen_mcu_p->lock);
	}
	pthread_cleanup_pop(0);
	return NULL;
}

static unsigned screen_mcu_chery_attach(struct screen_mcu_drv *screen_mcu_p){
	struct sched_param param;
    int policy;
    pthread_mutex_lock(&screen_mcu_p->lock);
	info_out("%s, busnum:%d, i2caddr:0x%02x\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
	
	screen_mcu_p->attach_chg = -1;
	screen_mcu_p->pwr_chg = 0;
	screen_mcu_p->bkl_chg = 0;
	if(!tp_irq_init(screen_mcu_p)){
		pthread_create(&screen_mcu_p->tp_irq_thread, NULL, tp_irq_thread, screen_mcu_p);	
		if(0!= pthread_getschedparam(screen_mcu_p->tp_irq_thread,&policy,&param))
        {
            err_out("%s:pthread_getschedparam failed!!!", __FUNCTION__);
        }else{
            param.sched_priority = 13;
            pthread_setname_np(screen_mcu_p->tp_irq_thread,"cherry_irq_thread_worker");
            pthread_setschedparam(screen_mcu_p->tp_irq_thread,policy,&param);
        }
	}
	init_versions(screen_mcu_p);
	pthread_cond_signal(&screen_mcu_p->request_ready_cond);
	pthread_mutex_unlock(&screen_mcu_p->lock);
	
	start_heartbeat_check();
		
	return 0;
}

static unsigned screen_mcu_chery_detach(struct screen_mcu_drv *screen_mcu_p){
	info_out("%s, busnum:%d, i2caddr:0x%02x\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
	stop_heartbeat_check();
    //pthread_mutex_lock(&screen_mcu_p->lock);
    struct chery_info *chery_info_p =  screen_mcu_p->priv_data;
	if(screen_mcu_p->tp_irq_thread){
		pthread_cancel(screen_mcu_p->tp_irq_thread);
		pthread_join(screen_mcu_p->tp_irq_thread, NULL);
		screen_mcu_p->tp_irq_thread=0;
	}
	if(chery_info_p && chery_info_p->tp_info){
	    sem_destroy(&chery_info_p->tp_info->notify_sem);
	    sem_destroy(&chery_info_p->tp_info->mutex);
	    munmap(chery_info_p->tp_info, sizeof(screen_mcu_tp_t));
	    chery_info_p->tp_info = NULL;
	    char shm_name_buf[32]="tp_";
	    strcat(shm_name_buf,screen_mcu_p->name);
		shm_unlink(shm_name_buf);
	}
	//pthread_mutex_unlock(&screen_mcu_p->lock);
	return 0;
}

static unsigned screen_mcu_safeinfo_set(char *recv_msg, size_t size, off64_t *offset, void *file_ctx){
    struct screen_mcu_drv *screen_mcu_p = file_ctx;
    info_out("%s, busnum:%d, i2caddr:0x%02x, size:%d, *offset:%d\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, size, *offset);
    uint8_t write_byte[10] = {0};
	if(screen_mcu_p->status && size >= 8){
        write_byte[0] = 0xa0;
        memcpy(write_byte+1, recv_msg, 8);
        if (!access("/syslog/safe_crc_erro", F_OK)){
            write_byte[9] = calCRC(write_byte, 9)+1;
        }else{
            write_byte[9] = calCRC(write_byte, 9);
        }
        pthread_mutex_lock(&screen_mcu_p->lock);
        if(i2c_write_bat(screen_mcu_p, screen_mcu_p->i2caddr, write_byte, 10)){
            warn_out("%s, busnum:%d, i2caddr:0x%02x, i2c_write_bat failed\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
        }
        pthread_mutex_unlock(&screen_mcu_p->lock);
	}else{
	     warn_out("%s, busnum:%d, i2caddr:0x%02x, failed, status:%d, size:%d\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, screen_mcu_p->status, size);
	}
	return size;
}

static unsigned screen_mcu_safeinfo_get(char *reply_msg, size_t size, off64_t *offset, void *file_ctx){
	return 0;
}

unsigned screen_mcu_chery_init(struct screen_mcu_drv *screen_mcu_p){
    struct stat st;
    char file_name[MAX_FILE_NAME_LEN];
    screen_mcu_p->attach_handler = &screen_mcu_chery_attach;
    screen_mcu_p->set_request_handler = &set_request_chery;
	screen_mcu_p->get_reply_handler = &get_reply_chery;
	screen_mcu_p->detach_handler = &screen_mcu_chery_detach;
	screen_mcu_p->bl_on = 1;
	screen_mcu_p->bl = 50;
	
	if(screen_mcu_p->i2cdev <= 0){
	    screen_mcu_p->i2cdev = get_i2cdev_with_bus(screen_mcu_p->busnum);
	    if(screen_mcu_p->i2cdev <= 0){
		    err_out("%s, busnum:%d, i2caddr:0x%02x, can't open i2cdev:%d\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, screen_mcu_p->i2cdev);
		    return -1;	
	    }
	    i2c_set_bus_speed(screen_mcu_p->i2cdev, I2C_SPEED_FAST, NULL);
	}
	
	if(!screen_mcu_p->priv_data){
	    struct chery_info *chery_info_p = (struct chery_info *)malloc(sizeof(struct chery_info));
	    memset(chery_info_p, 0x00, sizeof(struct chery_info));
	    screen_mcu_p->priv_data = chery_info_p;
	}
	
	if(!screen_mcu_p->request_handler_thread){
		pthread_create(&screen_mcu_p->request_handler_thread, NULL, request_handler_thread, screen_mcu_p);
	}
	
	if (stat("/proc/mount/dev/screen_mcu", &st)) {
		    pdbg_init("/dev/screen_mcu"); //create a node for user function
		    usleep(20000);
	}
	
	 struct pdbg_ops_s ops_safeinfo;
	 memset(&ops_safeinfo, 0x0, sizeof(struct pdbg_ops_s));
	 ops_safeinfo.read = screen_mcu_safeinfo_get;
	 ops_safeinfo.write  = screen_mcu_safeinfo_set;
	 snprintf(file_name, MAX_FILE_NAME_LEN, "/%s/safeinfo", screen_mcu_p->name);
	 pdbg_create_file(file_name, 0666, ops_safeinfo, screen_mcu_p);	
	
	return 0;
}


