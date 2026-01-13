#include <ctype.h>
#include <gpio_client.h>
#include <sys/mman.h>
#include <hw/inout.h>
#include <libgen.h>
#include <sys/iofunc.h>
#include "screen_mcu_huayang.h"

#define MCU_BLOCK_LEN 128

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

static int set_request_huayang(void *screen_mcu_drv, char *request){
	struct screen_mcu_drv *screen_mcu_p = (struct screen_mcu_drv *)screen_mcu_drv;
	unsigned nbytes = strlen(request);
	if(screen_mcu_p->is_updating == 1){
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
static int get_reply_huayang(void *screen_mcu_drv, char *reply_msg){
	struct screen_mcu_drv *screen_mcu_p = (struct screen_mcu_drv *)screen_mcu_drv;
	unsigned nbytes=0;
	pthread_mutex_lock(&screen_mcu_p->lock);
	nbytes = snprintf(reply_msg, 32, "%s\n", screen_mcu_p->reply);
	info_out("%s, busnum:%d, i2caddr:0x%02x, nbytes%d, reply_msg:%s\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, nbytes, reply_msg);
	pthread_mutex_unlock(&screen_mcu_p->lock);
	return nbytes;
}

static int do_control_action(struct screen_mcu_drv *screen_mcu_p, uint8_t power, uint8_t tilt, uint8_t backlight_power, uint8_t backlight, uint8_t key_func){
	uint8_t write_byte[6] = {0};
	write_byte[0] = 0x82;
	write_byte[1] = power;
	write_byte[2] = tilt;
	write_byte[3] = backlight_power;
	write_byte[4] = backlight;
	write_byte[5] = key_func;
	return i2c_write_bat(screen_mcu_p, screen_mcu_p->i2caddr, write_byte, 6);
}

static int getkeystatus(struct screen_mcu_drv *screen_mcu_p, uint8_t* tilt_degree, uint8_t* powerkey_status){
    int ret;
    uint8_t buf[3] = {0};
    ret = i2c_read_bytes(screen_mcu_p, screen_mcu_p->i2caddr, 0x44, buf, 7);
    if(!ret){
        *tilt_degree = buf[0];
        *powerkey_status = buf[2];
    }
    info_out("%s, busnum:%d, i2caddr:0x%02x,*tilt_degree:%d, *powerkey_status:%d ret:%d\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, *tilt_degree, *powerkey_status, ret);
    return ret;
}

static int getbacklightinfo(struct screen_mcu_drv *screen_mcu_p, uint8_t* bklight, uint8_t* onoff){
    int ret;
    uint8_t buf[2] = {0};
    ret = i2c_read_bytes(screen_mcu_p, screen_mcu_p->i2caddr, 0x43, buf, 2);
    if(!ret){
        *onoff = buf[0];
        *bklight = buf[1];
    }
    info_out("%s, busnum:%d, i2caddr:0x%02x,*onoff:%d, *bklight:%d ret:%d\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, *onoff, *bklight, ret);
    return ret;
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
    uint8_t write_byte[2] = {0};
    write_byte[0] = 0xb0;
    write_byte[1] = 0x01;
    return i2c_write_bat(screen_mcu_p, screen_mcu_p->i2caddr, write_byte, 2);
}

static int get_mcu_mode(struct screen_mcu_drv *screen_mcu_p){
    uint8_t buf[1] = {0};
    i2c_read_bytes(screen_mcu_p, screen_mcu_p->i2caddr, 0x10, buf, 1);
    return buf[0];
}

static int check_bootloader_mode(struct screen_mcu_drv *screen_mcu_p){
    if(get_mcu_mode(screen_mcu_p)==0x02){
        return 0;
    }
    warn_out("%s, busnum:%d, i2caddr:0x%02x, not in bootloader mode \n",  __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
    return -1;
}

static int check_tp_update_mode(struct screen_mcu_drv *screen_mcu_p){
    if(get_mcu_mode(screen_mcu_p)==0x03){
        return 0;
    }
    warn_out("%s, busnum:%d, i2caddr:0x%02x, not in tp update mode \n",  __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
    return -1;
}


static int check_app_mode(struct screen_mcu_drv *screen_mcu_p){
    if(get_mcu_mode(screen_mcu_p)==0x01){
        return 0;
    }
    warn_out("%s, busnum:%d, i2caddr:0x%02x, not in app mode \n",  __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
    return -1;
}

#define BREAK_ON_ERR(x)	\
	ret = x;	\
	if (ret)	\
		break

static int start_send_data(struct screen_mcu_drv *screen_mcu_p,uint8_t type, int data_len, int blocks_num){
     uint8_t write_byte[8] = {0};
    
     write_byte[0] = 0xc7;
     write_byte[1] = type;
     write_byte[2] = 0x02;
     write_byte[3] = blocks_num&0xff;
     write_byte[4] = (data_len>>24)&0xff;
     write_byte[5] = (data_len>>16)&0xff;
     write_byte[6] = (data_len>>8)&0xff;
     write_byte[7] = data_len&0xff;
     
     return i2c_write_bat(screen_mcu_p, screen_mcu_p->i2caddr, write_byte, 8);
}

static int send_data(struct screen_mcu_drv *screen_mcu_p,uint8_t type, uint8_t* data, int data_len, int blocks_index){
    uint8_t read_buf[1] = {0};
    uint8_t* write_byte = (uint8_t*)malloc(data_len+5+1);
    write_byte[0] = 0xc8;
    write_byte[1] = type;
    write_byte[2] = (blocks_index>>8)&0xff;
    write_byte[3] = blocks_index&0xff;
  
    write_byte[4] = (data_len>>8)&0xff;
    write_byte[5] = data_len&0xff;
    
    memcpy(&(write_byte[6]), data, data_len);
    
    int ret = i2c_write_bat(screen_mcu_p, screen_mcu_p->i2caddr, write_byte, data_len+5+1);
    
    if(!ret){
         int try_time = 3;
         ret = -1;
         do{
              if(type == 0x03){
                    usleep(150000);
              }else{
                    usleep(2000);
              }
              i2c_read_bytes(screen_mcu_p, screen_mcu_p->i2caddr, 0xc2, read_buf, 1);
              if(read_buf[0] == 0x01){
                    ret = 0;
                    if(type == 0x03){
                        usleep(460000);
                    }
                    break;
               }else if(read_buf[0] == 0x00){
                    if(type == 0x03){
                        usleep(460000);
                    }
               }
         }while(try_time--);
    }
    free(write_byte);
    if(ret){
        warn_out("%s, busnum:%d, i2caddr:0x%02x, send_data failed\n",  __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
    }
    return ret;
}

static int end_send_data(struct screen_mcu_drv *screen_mcu_p,uint8_t type, uint8_t* data, int data_len){
    int ret = -1;
    uint8_t write_byte[5] = {0};
    uint8_t read_buf[2] = {0};
    int check_sum = 0;
    int i = 0;
    do{
        check_sum = check_sum+data[i];
        i++;
    }while(i<data_len);
    write_byte[0] = 0xc9;
    write_byte[1] = type;
    write_byte[2] = (check_sum>>8)&0xff;
    write_byte[3] = check_sum&0xff;
    write_byte[4] = 0x01;
    
    ret = i2c_write_bat(screen_mcu_p, screen_mcu_p->i2caddr, write_byte, 5);
    
    if(!ret){
         int try_time = 3;
         ret = -1;
         do{
              usleep(2000);
              i2c_read_bytes(screen_mcu_p, screen_mcu_p->i2caddr, 0xce, read_buf, 2);
              if(read_buf[0] == type && read_buf[1] == 0x01){
                    ret = 0;
                    break;
               }
         }while(try_time--);
    }
    
    if(ret){
        warn_out("%s, busnum:%d, i2caddr:0x%02x, end_send_data failed\n",  __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
    }
    
    return ret;    
}


static int send_data_file(struct screen_mcu_drv *screen_mcu_p, int type, char* flashDriver_buf, int flashDriver_buf_len){
     int ret =-1;
     int blocks_num = 0;
     int update_progress;
     int last_blocks_len = flashDriver_buf_len%MCU_BLOCK_LEN;
     if(last_blocks_len>0){
        blocks_num = flashDriver_buf_len/MCU_BLOCK_LEN+1;
     }else{
        blocks_num = flashDriver_buf_len/MCU_BLOCK_LEN;
        last_blocks_len = MCU_BLOCK_LEN;
     }
     int block_index = 1;
     uint8_t* tmp_flashDriver_buf = (uint8_t*)flashDriver_buf;
     do{
	      BREAK_ON_ERR(start_send_data(screen_mcu_p, type, flashDriver_buf_len, blocks_num));
	      if(type == 0x03){
	        usleep(100000);
	      }
	      while(block_index<blocks_num) {
	         warn_out("%s, busnum:%d, i2caddr:0x%02x, block_index:%d\n",  __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, block_index);
	        if(send_data(screen_mcu_p, type, tmp_flashDriver_buf, MCU_BLOCK_LEN, block_index)){
	            break;
	        }
	        update_progress = block_index*100/blocks_num;
	        if(screen_mcu_p->update_progress != update_progress){
	            screen_mcu_p->update_progress = update_progress;
	            snprintf(screen_mcu_p->reply, 32, "update:%d", screen_mcu_p->update_progress);	            
	            wait_time(screen_mcu_p, 100000);
	        }
            tmp_flashDriver_buf = tmp_flashDriver_buf+MCU_BLOCK_LEN;
	        block_index++;
	      }
	      if(block_index == blocks_num){
	        BREAK_ON_ERR(send_data(screen_mcu_p, type, tmp_flashDriver_buf, last_blocks_len, block_index));
	        BREAK_ON_ERR(end_send_data(screen_mcu_p, type, (uint8_t*)flashDriver_buf, flashDriver_buf_len));
	        screen_mcu_p->update_progress = 100;
	        snprintf(screen_mcu_p->reply, 32, "update:%d", screen_mcu_p->update_progress);	            
	        wait_time(screen_mcu_p, 100000);
	      }else{
	        ret = -1;
	      }
	 }while(0);
     
     if(ret){
        warn_out("%s, busnum:%d, i2caddr:0x%02x, send_data_file failed\n",  __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
     }
     return ret;
}

static int check_update_file(struct screen_mcu_drv *screen_mcu_p){
    int ret = -1;
    
    uint8_t read_buf[1] = {0};
    
    int try_time = 3;
    do{
        usleep(2000);
        i2c_read_bytes(screen_mcu_p, screen_mcu_p->i2caddr, 0xcd, read_buf, 1);
        if(read_buf[0] == 0x01){
            ret = 0;
            break;
         }
    }while(try_time--);
    
    if(ret){
        warn_out("%s, busnum:%d, i2caddr:0x%02x, check_update_file failed\n",  __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
     }
    return ret;
}

static int erase_flash(struct screen_mcu_drv *screen_mcu_p, int update_buf_len){
    int ret = -1;
    uint8_t read_buf[1] = {0};
    uint8_t write_byte[10] = {0};
    write_byte[0] = 0xC0;
    write_byte[1] = 0x00;
    write_byte[2] = 0x00;
    write_byte[3] = 0xD4;
    write_byte[4] = 0x00;
    write_byte[5] = (update_buf_len>>24)&0xff;
    write_byte[6] = (update_buf_len>>16)&0xff;
    write_byte[7] = (update_buf_len>>8)&0xff;
    write_byte[8] = update_buf_len&0xff;
    write_byte[9] = 0x01;
    ret = i2c_write_bat(screen_mcu_p, screen_mcu_p->i2caddr, write_byte, 10);
    
    usleep(1000000);
    
     if(!ret){
         int try_time = 3;
         ret = -1;
         do{
              usleep(2000);
              i2c_read_bytes(screen_mcu_p, screen_mcu_p->i2caddr, 0xc1, read_buf, 1);
              if(read_buf[0] == 0x01){
                    ret = 0;
                    break;
               }
         }while(try_time--);
    }
    
    if(ret){
        warn_out("%s, busnum:%d, i2caddr:0x%02x, erase_flash failed\n",  __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
    }
    
    return ret;
}

static int reset_mcu(struct screen_mcu_drv *screen_mcu_p){
    int ret = -1;
    uint8_t write_byte[2] = {0};
    write_byte[0] = 0xBF;
    write_byte[1] = 0x01;
    
    ret = i2c_write_bat(screen_mcu_p, screen_mcu_p->i2caddr, write_byte, 2);
    
    if(ret){
        warn_out("%s, busnum:%d, i2caddr:0x%02x, erase_flash failed\n",  __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
    }
    
    return ret;
}

static int request_update_tp(struct screen_mcu_drv *screen_mcu_p){
    uint8_t write_byte[2] = {0};
    write_byte[0] = 0xb0;
    write_byte[1] = 0x02;
    return i2c_write_bat(screen_mcu_p, screen_mcu_p->i2caddr, write_byte, 2);
}

static int get_version_info(struct screen_mcu_drv *screen_mcu_p);

static int start_update(struct screen_mcu_drv *screen_mcu_p){
    int ret =-1;
    char updateFle_path[128]={'\0'};
    char flashDriver_path[128]={'\0'};
    char * p_temp = strstr(screen_mcu_p->request, " -p ");
	char * f_temp = strstr(screen_mcu_p->request," -f "); 
	snprintf(screen_mcu_p->reply, 32, "update:0");	            
	wait_time(screen_mcu_p, 100000);
	if(p_temp && f_temp){
	    strncpy(updateFle_path, p_temp+4, f_temp-p_temp-4);
	    strcpy(flashDriver_path, f_temp+4);
	    info_out("%s, busnum:%d, i2caddr:0x%02x,updateFle_path:%s, flashDriver_path:%s\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, updateFle_path, flashDriver_path);
	    
	    int update_buf_len;
	    int flashDriver_buf_len;
	    char* update_buf = load_file(updateFle_path, &update_buf_len);
	    char* flashDriver_buf = load_file(flashDriver_path, &flashDriver_buf_len);
	    if(update_buf && flashDriver_buf){
	        do{
	            BREAK_ON_ERR(request_update(screen_mcu_p));
	            usleep(100000);
	            BREAK_ON_ERR(check_bootloader_mode(screen_mcu_p));
	            BREAK_ON_ERR(send_data_file(screen_mcu_p, 0x01, flashDriver_buf, flashDriver_buf_len));
	            BREAK_ON_ERR(erase_flash(screen_mcu_p, update_buf_len));
	            BREAK_ON_ERR(send_data_file(screen_mcu_p, 0x02, update_buf, update_buf_len));
	            BREAK_ON_ERR(check_update_file(screen_mcu_p));
	            usleep(1000000);
	            BREAK_ON_ERR(reset_mcu(screen_mcu_p));
	            wait_time(screen_mcu_p, 1000000);
	            BREAK_ON_ERR(check_app_mode(screen_mcu_p));
	            wait_time(screen_mcu_p, 100000);
	            get_version_info(screen_mcu_p);
	        }while(0);
	    }
	    if(update_buf){
	        free(update_buf);
	    }
	    if(flashDriver_buf){
	        free(flashDriver_buf);
	    }  
	}
	if(ret){
	    snprintf(screen_mcu_p->reply, 32, "update:failed");	            
	    err_out("%s, busnum:%d, i2caddr:0x%02x,update failed\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
	}else{
	    snprintf(screen_mcu_p->reply, 32, "update:success");	
	    err_out("%s, busnum:%d, i2caddr:0x%02x,update success\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
	}
	screen_mcu_p->is_updating = 0;
    return ret;
}

static int start_update_tp(struct screen_mcu_drv *screen_mcu_p){
    int ret =-1;
    char updateFle_path[128]={'\0'};
    char * p_temp = strstr(screen_mcu_p->request, " -p ");
	snprintf(screen_mcu_p->reply, 32, "update:0");	            
	wait_time(screen_mcu_p, 100000);
	if(p_temp){
	    strcpy(updateFle_path, p_temp+4);
	    info_out("%s, busnum:%d, i2caddr:0x%02x,updateFle_path:%s\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, updateFle_path);
	    
	    int update_buf_len;
	    char* update_buf = load_file(updateFle_path, &update_buf_len);
	    if(update_buf){
	        do{
	            BREAK_ON_ERR(request_update_tp(screen_mcu_p));
	            usleep(3000000);
	            BREAK_ON_ERR(check_tp_update_mode(screen_mcu_p));
	            usleep(100000);
	            BREAK_ON_ERR(send_data_file(screen_mcu_p, 0x03, update_buf, update_buf_len));
	            usleep(1000000);
	            BREAK_ON_ERR(reset_mcu(screen_mcu_p));
	            wait_time(screen_mcu_p, 1000000);
	            BREAK_ON_ERR(check_app_mode(screen_mcu_p));
	            get_version_info(screen_mcu_p);
	        }while(0);
	    }
	    if(update_buf){
	        free(update_buf);
	    }
	}
	if(ret){
	    snprintf(screen_mcu_p->reply, 32, "update:failed");	            
	    err_out("%s, busnum:%d, i2caddr:0x%02x,update failed\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
	}else{
	    snprintf(screen_mcu_p->reply, 32, "update:success");	
	    err_out("%s, busnum:%d, i2caddr:0x%02x,update success\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
	}
	screen_mcu_p->is_updating = 0;
    return ret;
}

static int handle_request(struct screen_mcu_drv *screen_mcu_p){
    uint8_t bklight = -1;
    uint8_t backlightonoff = -1;
    uint8_t tilt_degree = -1;
    uint8_t powerkey_status = -1;
    struct huayang_info *huayang_info_p = screen_mcu_p->priv_data;
	if(!strcmp(screen_mcu_p->request, "lcd power on")){
		do_control_action(screen_mcu_p, 2, 0, 0, 0, 0);				
	}else if(!strcmp(screen_mcu_p->request, "lcd power off")){
		do_control_action(screen_mcu_p, 1, 0, 0, 0, 0);
	}else if(!strcmp(screen_mcu_p->request, "lcd tilt forward")){
		do_control_action(screen_mcu_p, 0, 1, 0, 0, 0);
	}else if(!strcmp(screen_mcu_p->request, "lcd tilt backward")){
		do_control_action(screen_mcu_p, 0, 2, 0, 0, 0);
	}else if(!strcmp(screen_mcu_p->request, "lcd enable key")){
		do_control_action(screen_mcu_p, 0, 0, 0, 0, 2);
	}else if(!strcmp(screen_mcu_p->request, "lcd disable key")){
		do_control_action(screen_mcu_p, 0, 0, 0, 0, 1);
	}else if(!strcmp(screen_mcu_p->request, "lcd get backlight")){
	    if(!getbacklightinfo(screen_mcu_p, &bklight, &backlightonoff)){
	         snprintf(screen_mcu_p->reply, 32, "%d", bklight);
	    }else{
	        snprintf(screen_mcu_p->reply, 32, "erro");
	    }
	}else if(!strcmp(screen_mcu_p->request, "lcd get backlight state")){
	    if(!getbacklightinfo(screen_mcu_p, &bklight, &backlightonoff)){
	         snprintf(screen_mcu_p->reply, 32, "%d", backlightonoff);
	    }else{
	         snprintf(screen_mcu_p->reply, 32, "erro");
	    }
	}else if(!strcmp(screen_mcu_p->request, "lcd get tilt degree")){
	    if(!getkeystatus(screen_mcu_p, &tilt_degree, &powerkey_status)){
	         snprintf(screen_mcu_p->reply, 32, "%d", tilt_degree);
	    }else{
	        snprintf(screen_mcu_p->reply, 32, "erro");
	    }
	}else if(!strcmp(screen_mcu_p->request, "lcd get powerkey state")){
	     if(!getkeystatus(screen_mcu_p, &tilt_degree, &powerkey_status)){
	         snprintf(screen_mcu_p->reply, 32, "%d", powerkey_status);
	    }else{
	        snprintf(screen_mcu_p->reply, 32, "erro");
	    }
	}else if(!strcmp(screen_mcu_p->request, "lcd get supplier sver")){
		 snprintf(screen_mcu_p->reply, 32, "%d.%d", huayang_info_p->sw_ver[0], huayang_info_p->sw_ver[1]);
	}else if(!strcmp(screen_mcu_p->request, "lcd get supplier hver")){
		 snprintf(screen_mcu_p->reply, 32, "%d.%d", huayang_info_p->hw_ver[0], huayang_info_p->hw_ver[1]);
	}else if(!strcmp(screen_mcu_p->request, "lcd get supplier blver")){
		 snprintf(screen_mcu_p->reply, 32, "%d.%d", huayang_info_p->bl_ver[0], huayang_info_p->bl_ver[1]);
	}else if(!strncmp(screen_mcu_p->request, "lcd update -p ",  strlen("lcd update -p "))){
	    start_update(screen_mcu_p);
	}else if(!strncmp(screen_mcu_p->request, "lcd update tp -p ",  strlen("lcd update tp -p "))){
	    start_update_tp(screen_mcu_p);
	}else{
	}
	return 0;
}

static int get_version_info(struct screen_mcu_drv *screen_mcu_p){
    	
    int ret = -1;
    uint8_t buf[4] = {0};
    struct huayang_info *huayang_info_p = screen_mcu_p->priv_data;
    ret = i2c_read_bytes(screen_mcu_p, screen_mcu_p->i2caddr, 0x47, buf, 6);
    if(!ret){
        //huayang_info_p->sw_ver[0] = buf[0];
        //huayang_info_p->sw_ver[1] = buf[1];
        //huayang_info_p->hw_ver[0] = buf[2];
        //huayang_info_p->hw_ver[1] = buf[3];
        memcpy(huayang_info_p->hw_ver, &buf[0], 2);
        memcpy(huayang_info_p->sw_ver, &buf[2], 2);
        memcpy(huayang_info_p->bl_ver, &buf[4], 2);
        huayang_info_p = screen_mcu_p->priv_data;
        info_out("%s, busnum:%d, i2caddr:0x%02x, %d.%d, %d.%d\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, huayang_info_p->sw_ver[0], huayang_info_p->sw_ver[1], huayang_info_p->hw_ver[0], huayang_info_p->hw_ver[1]);
     }
    return ret;
}

static void *request_handler_thread(void *thread_param){
	int wret;
	struct screen_mcu_drv *screen_mcu_p = thread_param;
	//struct huayang_info *huayang_info_p =  screen_mcu_p->priv_data;
	pthread_mutex_lock(&screen_mcu_p->lock);
	do{
	    if(screen_mcu_p->status){
		    if(screen_mcu_p->attach_chg){
		        get_version_info(screen_mcu_p);
			    screen_mcu_p->attach_chg = 0;
		    }
		    if(screen_mcu_p->pwr_chg){
		        if(!do_control_action(screen_mcu_p, 0, 0, screen_mcu_p->bl_on?2:1, 0, 0)){
		            screen_mcu_p->pwr_chg = 0;
		        }
		    }
		    if(screen_mcu_p->bkl_chg){
		        if(!do_control_action(screen_mcu_p, 0, 0, 0, screen_mcu_p->bl, 0)){
		            screen_mcu_p->bkl_chg = 0;
		        }
		    }
		    if(screen_mcu_p->request_chg){
			    if(!handle_request(screen_mcu_p)){
				    screen_mcu_p->request_chg = 0;	
			    }
		    }
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

static void *dtc_irq_thread(void *thread_param){
	int err;
	struct screen_mcu_drv *screen_mcu_p = thread_param;
	struct sigevent event;
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
		info_out("%s, busnum:%d, i2caddr:0x%02x,process irq %d\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, err);
	}
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
	struct huayang_info *huayang_info_p =  screen_mcu_p->priv_data;
	screen_mcu_p->tp_irq_type = GPIO_INTERRUPT_TRIGGER_LOW;
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
				huayang_info_p->tp_info = tp_info;
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

static int mcu_ts_read_data(struct screen_mcu_drv *screen_mcu_p){
    int ret = -1;
    struct huayang_info *huayang_info_p =  screen_mcu_p->priv_data;
    screen_mcu_tp_t *tp_info = huayang_info_p->tp_info;
    uint8_t buf[128];
    int notify_sem_value;
    ret = i2c_read_bytes(screen_mcu_p, screen_mcu_p->i2caddr, 0x41, buf, 16);
    if(!ret){
        sem_wait(&tp_info->mutex);
        uint8_t pc;
        uint8_t i;
        uint8_t next_tp_len;
        uint8_t *tp_data;

        pc = buf[0] & 0xf;
        tp_info->pointer_num = pc;
        if(pc > 2 && pc <=10){
            next_tp_len = (pc-2)*6;
            if(next_tp_len != buf[13]){
                warn_out("%s %s next_tp_len != buf[13]\n", screen_mcu_p->name, __func__);
            }
            i2c_read_bytes(screen_mcu_p, screen_mcu_p->i2caddr, 0x42, buf+16, next_tp_len);
        }
        tp_data = buf + 1;
        for (i=0; i<pc; i++, tp_data+=6) {
            tp_info->events[i].id = tp_data[0];
            tp_info->events[i].status = tp_data[1];
            tp_info->events[i].x = (tp_data[2]<<8) | tp_data[3];
            tp_info->events[i].y = (tp_data[4]<<8) | tp_data[5];
            if(i==1){
                tp_data+=3;			
            }
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

static int repo_screen_event(struct screen_mcu_drv *screen_mcu_p, char* key, int value){
    static uint8_t has_mk_dir = 0;
    char buf[64];
    int len=0;
    int write_len=0;
    struct stat stat_buf;
    if (!has_mk_dir && stat("/var/pps/screen", &stat_buf) != 0){
	    mkdir("/var/pps/screen", 0775);
	}else{
	    has_mk_dir = 1;
	}
	
    int fd = open("/var/pps/screen/event_panel1-0", O_RDWR | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO | O_CLOEXEC);
    if(fd < 0){ 
        warn_out("%s %s can't open /var/pps/screen/event_panel1-0\n", screen_mcu_p->name, __func__);
        return -1;
    }
    len = snprintf(buf, 64, "%s::%d", key, value);
    info_out("%s, busnum:%d, i2caddr:0x%02x, %s\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, buf);
    write_len = write(fd, buf, len);
    close(fd);
    if(write_len <= 0){
        warn_out("%s %s failed %d \n", screen_mcu_p->name, __func__, write_len);
    }
    return write_len;
}

static int get_screen_func_stat(struct screen_mcu_drv *screen_mcu_p){
    uint8_t buf[64];
    int ret = -1;
     ret = i2c_read_bytes(screen_mcu_p, screen_mcu_p->i2caddr, 0x45, buf, 5);
    if(!ret){
        
    }
    return ret;
}

static int get_screen_event(struct screen_mcu_drv *screen_mcu_p){
    uint8_t buf[64];
    int ret = -1;
    struct huayang_info *huayang_info_p =  screen_mcu_p->priv_data;
    ret = i2c_read_bytes(screen_mcu_p, screen_mcu_p->i2caddr, 0x44, buf, 7);
    if(!ret){
        //info_out("%s, busnum:%d, i2caddr:0x%02x,buf[0]:0x%02x,buf[1]:0x%02x,buf[2]:0x%02x,buf[3]:0x%02x\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, buf[0], buf[1], buf[2], buf[3]);
        if(huayang_info_p->tilt_degree!=buf[0]){
            huayang_info_p->tilt_degree = buf[0];
            repo_screen_event(screen_mcu_p, "ceil_tilt_degree", huayang_info_p->tilt_degree);
        }
        if(huayang_info_p->tilt_stat!=buf[1]){
            huayang_info_p->tilt_stat = buf[1];
            repo_screen_event(screen_mcu_p, "ceil_tilt_stat", huayang_info_p->tilt_stat);
        }
        if(huayang_info_p->power_stat!=buf[2]){
            huayang_info_p->power_stat = buf[2];
            repo_screen_event(screen_mcu_p, "ceil_power_stat", huayang_info_p->power_stat);
        }
        if(huayang_info_p->open_stat!=buf[3]){
            huayang_info_p->open_stat = buf[3];
            repo_screen_event(screen_mcu_p, "ceil_open_stat", huayang_info_p->open_stat);
        } 
        if(huayang_info_p->anti_pinch_stat!=buf[4]){
            huayang_info_p->anti_pinch_stat = buf[4];
            repo_screen_event(screen_mcu_p, "ceil_anti_pinch_stat", huayang_info_p->anti_pinch_stat);
        }
         if(huayang_info_p->key_lock_stat!=buf[5]){
            huayang_info_p->key_lock_stat = buf[5];
            repo_screen_event(screen_mcu_p, "ceil_key_lock_stat", huayang_info_p->key_lock_stat);
        }
         if(huayang_info_p->key_stat!=buf[6]){
            huayang_info_p->key_stat = buf[6];
            repo_screen_event(screen_mcu_p, "ceil_key_stat", huayang_info_p->key_stat);
        }
    }
    return ret;
}

static void lock_cleanup(void *arg){
    struct screen_mcu_drv *screen_mcu_p = arg;
    struct huayang_info *huayang_info_p =  screen_mcu_p->priv_data;
    info_out("%s, busnum:%d, i2caddr:0x%02x\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
    repo_screen_event(screen_mcu_p, "ceil_conn_stat", screen_mcu_p->status);
    huayang_info_p->open_stat = 0;
    repo_screen_event(screen_mcu_p, "ceil_open_stat", huayang_info_p->open_stat);
    pthread_mutex_unlock(&screen_mcu_p->lock);
}

static void *tp_irq_thread(void *thread_param){
	int err;
	uint8_t buf;
	struct screen_mcu_drv *screen_mcu_p = thread_param;
	struct sigevent event;
	pthread_cleanup_push(lock_cleanup, screen_mcu_p);
	waitfor("/var/pps", 3000, 0);
	pthread_mutex_lock(&screen_mcu_p->lock);
    repo_screen_event(screen_mcu_p, "ceil_conn_stat", screen_mcu_p->status);
	get_screen_event(screen_mcu_p);
	get_screen_func_stat(screen_mcu_p);
	pthread_mutex_unlock(&screen_mcu_p->lock);
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

        pthread_mutex_lock(&screen_mcu_p->lock);
        if(!i2c_read_bytes(screen_mcu_p, screen_mcu_p->i2caddr, 0x4f, &buf, 1)){
            info_out("%s, busnum:%d, i2caddr:0x%02x,process irq buf:0x%02x %d\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, buf, err);
            if(buf == 0x41){
                mcu_ts_read_data(screen_mcu_p);
            }else if(buf == 0x44){
                get_screen_event(screen_mcu_p);
            }else if(buf == 0x45){
                get_screen_func_stat(screen_mcu_p);
            }
         }
         pthread_mutex_unlock(&screen_mcu_p->lock);
	}
	pthread_cleanup_pop(0);
	return NULL;
}

static unsigned screen_mcu_huayang_attach(struct screen_mcu_drv *screen_mcu_p){
    struct sched_param param;
    int policy;
    pthread_mutex_lock(&screen_mcu_p->lock);
	info_out("%s, busnum:%d, i2caddr:0x%02x\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
	
	screen_mcu_p->attach_chg = -1;
	screen_mcu_p->pwr_chg = 0;
	screen_mcu_p->bkl_chg = 0;
    if(!dtc_irq_init(screen_mcu_p)){
		pthread_create(&screen_mcu_p->dtc_irq_thread, NULL, dtc_irq_thread, screen_mcu_p);	
	}
	if(!tp_irq_init(screen_mcu_p)){
		pthread_create(&screen_mcu_p->tp_irq_thread, NULL, tp_irq_thread, screen_mcu_p);
		if(0!= pthread_getschedparam(screen_mcu_p->tp_irq_thread,&policy,&param))
        {
            err_out("%s:pthread_getschedparam failed!!!", __FUNCTION__);
        }else{
            param.sched_priority = 13;
            pthread_setname_np(screen_mcu_p->tp_irq_thread,"huayang_irq_thread_worker");
            pthread_setschedparam(screen_mcu_p->tp_irq_thread,policy,&param);
        }	
	}
	pthread_cond_signal(&screen_mcu_p->request_ready_cond);
	pthread_mutex_unlock(&screen_mcu_p->lock);
		
	return 0;
}

static unsigned screen_mcu_huayang_detach(struct screen_mcu_drv *screen_mcu_p){
	info_out("%s, busnum:%d, i2caddr:0x%02x\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
    //pthread_mutex_lock(&screen_mcu_p->lock);
    struct huayang_info *huayang_info_p =  screen_mcu_p->priv_data;
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
	if(huayang_info_p && huayang_info_p->tp_info){
	    sem_destroy(&huayang_info_p->tp_info->notify_sem);
	    sem_destroy(&huayang_info_p->tp_info->mutex);
	    munmap(huayang_info_p->tp_info, sizeof(screen_mcu_tp_t));
	    huayang_info_p->tp_info = NULL;
	    char shm_name_buf[32]="tp_";
	    strcat(shm_name_buf,screen_mcu_p->name);
		shm_unlink(shm_name_buf);
	}
	//pthread_mutex_unlock(&screen_mcu_p->lock);
	return 0;
}

static void *repo_conn_stat(void *thread_param){
    struct screen_mcu_drv *screen_mcu_p = thread_param;
    waitfor("/var/pps", 3000, 0);
    pthread_mutex_lock(&screen_mcu_p->lock);
	repo_screen_event(screen_mcu_p, "ceil_conn_stat", screen_mcu_p->status);
	pthread_mutex_unlock(&screen_mcu_p->lock);
	return NULL;
}

unsigned screen_mcu_huayang_init(struct screen_mcu_drv *screen_mcu_p){
    screen_mcu_p->attach_handler = &screen_mcu_huayang_attach;
    screen_mcu_p->set_request_handler = &set_request_huayang;
	screen_mcu_p->get_reply_handler = &get_reply_huayang;
	screen_mcu_p->detach_handler = &screen_mcu_huayang_detach;
	screen_mcu_p->bl_on = 1;
	screen_mcu_p->bl = 50;
	
	if(screen_mcu_p->i2cdev <= 0){
	    screen_mcu_p->i2cdev = get_i2cdev_with_bus(screen_mcu_p->busnum);
	    if(screen_mcu_p->i2cdev <= 0){
		    err_out("%s, busnum:%d, i2caddr:0x%02x, can't open i2cdev:%d\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, screen_mcu_p->i2cdev);
		    return -1;	
	    }
	}
	
	if(!screen_mcu_p->priv_data){
	    struct huayang_info *huayang_info_p = (struct huayang_info *)malloc(sizeof(struct huayang_info));
	    memset(huayang_info_p, 0x00, sizeof(struct huayang_info));
	    huayang_info_p->tilt_degree = 255;
	    huayang_info_p->tilt_stat = 255;
	    huayang_info_p->power_stat = 255;
	    huayang_info_p->open_stat = 255;
	    huayang_info_p->anti_pinch_stat = 255;
	    huayang_info_p->key_lock_stat = 255;
	    huayang_info_p->key_stat = 255;
	    screen_mcu_p->priv_data = huayang_info_p;
	}
	
	if(!screen_mcu_p->request_handler_thread){
		pthread_create(&screen_mcu_p->request_handler_thread, NULL, request_handler_thread, screen_mcu_p);
	}
	
	pthread_t repo_conn_stat_thread = NULL;
	pthread_create(&repo_conn_stat_thread, NULL, repo_conn_stat, screen_mcu_p);
	return 0;
}


