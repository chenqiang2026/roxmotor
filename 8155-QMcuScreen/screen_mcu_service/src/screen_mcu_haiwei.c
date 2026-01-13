#include <ctype.h>
#include <gpio_client.h>
#include <sys/mman.h>
#include <hw/inout.h>
#include <sys/iofunc.h>
#include "screen_mcu_haiwei.h"

#define MCU_REG_DIAG_WRITE_ADDRESS			0x40	//diagnosis wirte
#define MCU_REG_DIAG_READ_ADDRESS			0x41	//diagnosis	read
#define MCU_REG_BOOTLOADER_WRITE_ADDRESS	0x60	//bootloader write
#define MCU_REG_BOOTLOADER_READ_ADDRESS		0x61	//bootloader response

#define MCU_BOOTLOADER_STATUS_OK				0
#define MCU_BOOTLOADER_STATUS_NO_OK				1
#define MCU_BOOTLOADER_STATUS_BUSY				2
#define MCU_BOOTLOADER_STATUS_RETRANSMISSION	3

#define MCU_SERVICE_ID_MCU_READ			0x22	//mcu read
#define MCU_SERVICE_ID_ERASE_RANGE		0x81	//Erase Range Command
#define MCU_SERVICE_ID_REQUEST_DOWNLOAD	0x82	//Request Download  Command
#define MCU_SERVICE_ID_TRANSFER_DATA	0x83	//Transfer Data  Command
#define MCU_SERVICE_ID_CHECKSUM			0x84	//Checksum  Command
#define MCU_SERVICE_ID_RESET			0x85	//Reset Command

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
#define DID_NUMBER_SESSION		0xFEF7	//session

#define MCU_APP_MODE			0x01
#define MCU_BOOTLOADER_MODE		0x02

#define MCU_BLOCK_LEN 128
#define MCU_PROTOCOL_HEAD_LEN 4
#define MCU_PROTOCOL_CRC 2

static uint8_t mcu_calc_crc8(const uint8_t *data, uint8_t len)
{
  uint32_t idx;
  uint8_t idx2;
  uint8_t crc_val = 0xFF;
  uint8_t crc_poly = 0x1D;

  for(idx = 0; idx < len; idx++) {
    crc_val ^= data[idx];
    for(idx2 = 0; idx2 < 8; idx2++) {
      if((crc_val & (uint8_t)0x80) > 0) {
        crc_val = ((uint8_t)(crc_val << 1)) ^ crc_poly;
      } else {
        crc_val = (uint8_t)(crc_val << 1);
      }
    }
  }
  return crc_val;
}

static uint16_t mcu_crc16Clac(const uint8_t *data, unsigned int len)
{
	uint16_t wCRCin = 0x0000;
	uint16_t wCPoly = 0x1021;
	int i;

	while (len--) {
		wCRCin ^= (*(data++) << 8);
		for (i = 0; i < 8; i++) {
			if (wCRCin & 0x8000) {
				wCRCin = (wCRCin << 1) ^ wCPoly;
			} else {
				wCRCin = wCRCin << 1;
			}
		}
	}
	return wCRCin;
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

static int set_request_haiwei(void *screen_mcu_drv, char *request){
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
	        if(!strncmp(screen_mcu_p->request, "lcd update -p ",  strlen("lcd update -p "))){
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
static int get_reply_haiwei(void *screen_mcu_drv, char *reply_msg){
	struct screen_mcu_drv *screen_mcu_p = (struct screen_mcu_drv *)screen_mcu_drv;
	unsigned nbytes=0;
	pthread_mutex_lock(&screen_mcu_p->lock);
	nbytes = snprintf(reply_msg, 32, "%s\n", screen_mcu_p->reply);
	info_out("%s, busnum:%d, i2caddr:0x%02x, nbytes%d, reply_msg:%s\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, nbytes, reply_msg);
	pthread_mutex_unlock(&screen_mcu_p->lock);
	return nbytes;
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

static int get_did_info(struct screen_mcu_drv *screen_mcu_p, const uint16_t did_number, uint8_t * rxdata){
	uint8_t write_byte[5] = {0};
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
				info_out("%s, busnum:%d, i2caddr:0x%02x, buf[0]:0x%02x\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, buf[0]);
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
static int get_mcu_bootloader_response(struct screen_mcu_drv *screen_mcu_p){
    int ret = -1;
    uint8_t buf[6] = {0};
    uint16_t tmp_crc = 0;
	ret = i2c_read_bytes(screen_mcu_p, screen_mcu_p->i2caddr, MCU_REG_BOOTLOADER_READ_ADDRESS, buf, 6);
	if(!ret){
	    //tmp_crc = mcu_crc16Clac(buf, 4);
	    //if((tmp_crc & 0x00ff) == buf[4] && ((tmp_crc & 0xff00) >> 8) == buf[5]){
	        ret = (buf[3] & 0xff);
	    //}else{
	    //    ret = -1;
	    //}
	}
    
    if(ret<0){
         warn_out("%s, busnum:%d, i2caddr:0x%02x, get_mcu_bootloader_response failed (tmp_crc&0x00ff):0x%02x, ((tmp_crc & 0xff00) >> 8):0x%02x\n",  __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, tmp_crc&0x00ff, ((tmp_crc & 0xff00) >> 8));
    }
    return ret;
}

static int request_bootloader(struct screen_mcu_drv *screen_mcu_p){
    uint8_t write_byte[4] = {0};
    uint8_t buf[3] = {0};
	int ret = -1;
	write_byte[0] = MCU_REG_DIAG_WRITE_ADDRESS;
	write_byte[1] = 2;
	write_byte[2] = 0x10;
	write_byte[3] = 0x02;
	ret = i2c_write_bat(screen_mcu_p, screen_mcu_p->i2caddr, write_byte, 4);
	
	if(!ret){
	    usleep(1000000);
	    ret = i2c_read_bytes(screen_mcu_p, screen_mcu_p->i2caddr, MCU_REG_DIAG_READ_ADDRESS, buf, 3);
	    if(!ret){
	        if(buf[1] == 0x50){
	            ret = 0;
	        }else{
	            ret = -1;
	        }
	    }
	}
	
	if(ret){
	    warn_out("%s, busnum:%d, i2caddr:0x%02x, request_bootloader failed\n",  __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
	}
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

static int request_erase_flash(struct screen_mcu_drv *screen_mcu_p){
    int ret = -1;
    uint16_t tmp_crc16 = 0;
	uint8_t write_byte[14] = {0};

	write_byte[0] = MCU_REG_BOOTLOADER_WRITE_ADDRESS;
	write_byte[1] = 11;	//data length LSB
	write_byte[2] = 0;	//data length MSB
	write_byte[3] = MCU_SERVICE_ID_ERASE_RANGE;	//service id
	//write_byte[4] = data[60];//start addr
	//write_byte[5] = data[61];
	//write_byte[6] = data[62];
	//write_byte[7] = data[63];
	//write_byte[8] = data[64];//block len
	//write_byte[9] = data[65];
	//write_byte[10] = data[66];
	//write_byte[11] = data[67];
	tmp_crc16 = mcu_crc16Clac(&write_byte[1], 11);
	write_byte[12] = tmp_crc16 & 0x00ff;			//data length LSB
	write_byte[13] = (tmp_crc16 & 0xff00) >> 8;		//data length MSB
	ret = i2c_write_bat(screen_mcu_p, screen_mcu_p->i2caddr, write_byte, 14);	
	usleep(500000);
	if(get_mcu_bootloader_response(screen_mcu_p) == MCU_BOOTLOADER_STATUS_OK){
	    ret = 0;
	}else{
	    ret = -1;
	    warn_out("%s, busnum:%d, i2caddr:0x%02x, request_erase_flash failed\n",  __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
	}
    return ret;       
}

static int begin_send_update_file(struct screen_mcu_drv *screen_mcu_p){
    int ret = -1;
    uint16_t tmp_crc16 = 0;
	uint8_t write_byte[14] = {0};

	write_byte[0] = MCU_REG_BOOTLOADER_WRITE_ADDRESS;
	write_byte[1] = 11;	//data length LSB
	write_byte[2] = 0;	//data length MSB
	write_byte[3] = MCU_SERVICE_ID_REQUEST_DOWNLOAD;	//service id
	//write_byte[4] = data[60];//start addr
	//write_byte[5] = data[61];
	//write_byte[6] = data[62];
	//write_byte[7] = data[63];
	//write_byte[8] = data[64];//block len
	//write_byte[9] = data[65];
	//write_byte[10] = data[66];
	//write_byte[11] = data[67];
	tmp_crc16 = mcu_crc16Clac(&write_byte[1], 11);
	write_byte[12] = tmp_crc16 & 0x00ff;			//data length LSB
	write_byte[13] = (tmp_crc16 & 0xff00) >> 8;		//data length MSB
	ret = i2c_write_bat(screen_mcu_p, screen_mcu_p->i2caddr, write_byte, 14);
	if(!ret){
	    usleep(500000);
	    if(get_mcu_bootloader_response(screen_mcu_p) == MCU_BOOTLOADER_STATUS_OK){
	        ret = 0;
	    }else{
	        ret = -1;
	        warn_out("%s, busnum:%d, i2caddr:0x%02x, begin_send_update_file failed\n",  __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
	    }
	}
    return ret;   
}

static int send_data(struct screen_mcu_drv *screen_mcu_p, uint8_t* data, int data_len, int blocks_index, int * block_crc){
    int ret = -1;
    uint16_t tmp_crc16 = 0;
	uint8_t* write_byte = (uint8_t*)malloc(data_len+MCU_PROTOCOL_HEAD_LEN + MCU_PROTOCOL_CRC+1);
    write_byte[0] = MCU_REG_BOOTLOADER_WRITE_ADDRESS;
    write_byte[1] = (data_len + MCU_PROTOCOL_HEAD_LEN) & 0x00ff;		//data length LSB
	write_byte[2] = ((data_len + MCU_PROTOCOL_HEAD_LEN) & 0xff00) >> 8;	//data length MSB
	write_byte[3] = MCU_SERVICE_ID_TRANSFER_DATA;	//service id
    write_byte[4] = (blocks_index%0xff) + 1;
    memcpy(write_byte + 5, data, data_len);
    tmp_crc16 = mcu_crc16Clac(&write_byte[1], data_len + MCU_PROTOCOL_HEAD_LEN);
	write_byte[data_len + 5] = tmp_crc16 & 0x00ff;		//data length LSB
	write_byte[data_len + 6] = (tmp_crc16 & 0xff00) >> 8;	//data length MSB
    ret = i2c_write_bat(screen_mcu_p, screen_mcu_p->i2caddr, write_byte, data_len+MCU_PROTOCOL_HEAD_LEN + MCU_PROTOCOL_CRC+1);
    * block_crc = mcu_crc16Clac(&write_byte[5], data_len);
    if(!ret){
        usleep(50000);
	    do {
		    ret = get_mcu_bootloader_response(screen_mcu_p);
			if(ret == MCU_BOOTLOADER_STATUS_BUSY){
				usleep(100000);	
			}
	    } while (ret == MCU_BOOTLOADER_STATUS_BUSY);
	}
	if(ret){
	    warn_out("%s, busnum:%d, i2caddr:0x%02x, send_data failed ret:%d\n",  __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, ret);
	}
    return ret;
}

static int end_send_update_file(struct screen_mcu_drv *screen_mcu_p, int file_crc){
    int ret = -1;
    uint16_t tmp_crc16 = 0;
	uint8_t write_byte[16] = {0};

	write_byte[0] = MCU_REG_BOOTLOADER_WRITE_ADDRESS;
	write_byte[1] = 13;	//data length LSB
	write_byte[2] = 0;	//data length MSB
	write_byte[3] = MCU_SERVICE_ID_CHECKSUM;	//service id
	write_byte[12] = file_crc & 0x00ff;
	write_byte[13] = (file_crc & 0xff00) >> 8;
	tmp_crc16 = mcu_crc16Clac(&write_byte[1], 13);
	write_byte[14] = tmp_crc16 & 0x00ff;		//data length LSB
	write_byte[15] = (tmp_crc16 & 0xff00) >> 8;	//data length MSB


	do{
		ret = i2c_write_bat(screen_mcu_p, screen_mcu_p->i2caddr, write_byte, 16);
		
		if(!ret){
		    usleep(500000);
			do {
				ret = get_mcu_bootloader_response(screen_mcu_p);
				if(ret == MCU_BOOTLOADER_STATUS_BUSY){
					usleep(500000);	
				}
	    	} while (ret == MCU_BOOTLOADER_STATUS_BUSY);
			if(ret == MCU_BOOTLOADER_STATUS_RETRANSMISSION){
			    continue;
			}
		}
	}while(0);
    
    return ret;   
}

#define BREAK_ON_ERR(x)	\
	ret = x;	\
	if (ret)	\
		break

static int send_data_file(struct screen_mcu_drv *screen_mcu_p, char* file_buf, int buf_len){
    int ret = -1;
    
    int file_crc = 0;
    int block_crc = 0;
    int blocks_num = 0;
    int last_blocks_len = buf_len%MCU_BLOCK_LEN;
    if(last_blocks_len>0){
        blocks_num = buf_len/MCU_BLOCK_LEN+1;
    }else{
        blocks_num = buf_len/MCU_BLOCK_LEN;
        last_blocks_len = MCU_BLOCK_LEN;
    }
    int block_index = 0;
    uint8_t* tmp_file_buf = (uint8_t*)file_buf;
    do{
        BREAK_ON_ERR(begin_send_update_file(screen_mcu_p));
        while(block_index<blocks_num) {
            info_out("%s, busnum:%d, i2caddr:0x%02x, block_index:%d\n",  __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, block_index);
            if(block_index == blocks_num -1){
                ret = send_data(screen_mcu_p, tmp_file_buf, last_blocks_len, block_index, &block_crc);
            }else{
                ret = send_data(screen_mcu_p, tmp_file_buf, MCU_BLOCK_LEN, block_index, &block_crc);
            }
	        if(ret == MCU_BOOTLOADER_STATUS_RETRANSMISSION){
	            continue;
	        }else if(ret != MCU_BOOTLOADER_STATUS_OK){
	            ret = -1;
	            break;
	        }
	        screen_mcu_p->update_progress = block_index*100/blocks_num;
	        if(screen_mcu_p->update_progress%5 == 0){
	            snprintf(screen_mcu_p->reply, 32, "update:%d", screen_mcu_p->update_progress);	            
	            wait_time(screen_mcu_p, 100000);
	        }
            tmp_file_buf = tmp_file_buf+MCU_BLOCK_LEN;
            file_crc = file_crc+block_crc;
            info_out("%s, busnum:%d, i2caddr:0x%02x, block_crc:0x%04x, file_crc:0x%04x\n",  __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, block_crc, file_crc);
	        block_index++;
	    }
        if(!ret){
           BREAK_ON_ERR(end_send_update_file(screen_mcu_p, file_crc)); 
        }
        screen_mcu_p->update_progress = 100;
	    snprintf(screen_mcu_p->reply, 32, "update:%d", screen_mcu_p->update_progress);	            
	    wait_time(screen_mcu_p, 100000);
    }while(0);
    
    if(ret){
        warn_out("%s, busnum:%d, i2caddr:0x%02x, send_data_file failed\n",  __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
    }
    return ret;
}

static int send_data_file_multi_blocks(struct screen_mcu_drv *screen_mcu_p, char* file_buf, int buf_len, int offset, int all_len){
    int ret = -1;
    
    int file_crc = 0;
    int block_crc = 0;
    int blocks_num = 0;
    int last_blocks_len = buf_len%MCU_BLOCK_LEN;
    if(last_blocks_len>0){
        blocks_num = buf_len/MCU_BLOCK_LEN+1;
    }else{
        blocks_num = buf_len/MCU_BLOCK_LEN;
        last_blocks_len = MCU_BLOCK_LEN;
    }
    int block_index = 0;
    uint8_t* tmp_file_buf = (uint8_t*)file_buf;
    do{
		info_out("%s, busnum:%d, i2caddr:0x%02x, offset:%d, all_len:%d\n",  __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, offset, all_len);
        BREAK_ON_ERR(begin_send_update_file(screen_mcu_p));
        while(block_index<blocks_num) {
            info_out("%s, busnum:%d, i2caddr:0x%02x, block_index:%d\n",  __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, block_index);
            if(block_index == blocks_num -1){
                ret = send_data(screen_mcu_p, tmp_file_buf, last_blocks_len, block_index, &block_crc);
            }else{
                ret = send_data(screen_mcu_p, tmp_file_buf, MCU_BLOCK_LEN, block_index, &block_crc);
            }
	        if(ret == MCU_BOOTLOADER_STATUS_RETRANSMISSION){
	            continue;
	        }else if(ret != MCU_BOOTLOADER_STATUS_OK){
	            ret = -1;
	            break;
	        }
	        screen_mcu_p->update_progress = (offset+block_index*MCU_BLOCK_LEN)*100/all_len;
	        if(screen_mcu_p->update_progress%5 == 0){
	            snprintf(screen_mcu_p->reply, 32, "update:%d", screen_mcu_p->update_progress);	            
	            wait_time(screen_mcu_p, 100000);
	        }
            tmp_file_buf = tmp_file_buf+MCU_BLOCK_LEN;
            file_crc = file_crc+block_crc;
            info_out("%s, busnum:%d, i2caddr:0x%02x, block_crc:0x%04x, file_crc:0x%04x, update_progress:%d\n",  __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, block_crc, file_crc, screen_mcu_p->update_progress);
	        block_index++;
	    }
        if(!ret){
           BREAK_ON_ERR(end_send_update_file(screen_mcu_p, file_crc)); 
        }
        //screen_mcu_p->update_progress = 100;
	    snprintf(screen_mcu_p->reply, 32, "update:%d", screen_mcu_p->update_progress);	            
	    wait_time(screen_mcu_p, 100000);
    }while(0);
    
    if(ret){
        warn_out("%s, busnum:%d, i2caddr:0x%02x, send_data_file failed\n",  __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
    }
    return ret;
}

static int request_reset_mcu(struct screen_mcu_drv *screen_mcu_p){
    int ret = -1;
    uint16_t tmp_crc16 = 0;
	uint8_t write_byte[7] = {0};

	write_byte[0] = MCU_REG_BOOTLOADER_WRITE_ADDRESS;
	write_byte[1] = 4;	//data length LSB
	write_byte[2] = 0;	//data length MSB
	write_byte[3] = MCU_SERVICE_ID_RESET;	//service id
	write_byte[4] = 1;	//reset(0x01)
	tmp_crc16 = mcu_crc16Clac(&write_byte[1], 4);
	write_byte[5] = tmp_crc16 & 0x00ff;			//data length LSB
	write_byte[6] = (tmp_crc16 & 0xff00) >> 8;	//data length MSB
	ret = i2c_write_bat(screen_mcu_p, screen_mcu_p->i2caddr, write_byte, 7);
	
    return ret;
}

static int request_erase_flash_specific(struct screen_mcu_drv *screen_mcu_p, int type, int block_len){
    int ret = -1;
    uint16_t tmp_crc16 = 0;
	uint8_t write_byte[14] = {0};
	int start_addr = 0;
	if(type == '1'){
		start_addr = 0x0000;
	}else if(type == '2'){
		start_addr = 0x10000;
	}else if(type == '3'){
		start_addr = 0x8000;	
	}else{
		start_addr = 0x0000;	
	}
	
	write_byte[0] = MCU_REG_BOOTLOADER_WRITE_ADDRESS;
	write_byte[1] = 11;	//data length LSB
	write_byte[2] = 0;	//data length MSB
	write_byte[3] = MCU_SERVICE_ID_ERASE_RANGE;	//service id
	write_byte[4] = start_addr&0xff;//start addr
	write_byte[5] = (start_addr>>8)&0xff;
	write_byte[6] = (start_addr>>16)&0xff;
	write_byte[7] = (start_addr>>24)&0xff;
	write_byte[8] = block_len&0xff;//block len
	write_byte[9] = (block_len>>8)&0xff;
	write_byte[10] = (block_len>>16)&0xff;
	write_byte[11] = (block_len>>24)&0xff;
	tmp_crc16 = mcu_crc16Clac(&write_byte[1], 11);
	write_byte[12] = tmp_crc16 & 0x00ff;			//data length LSB
	write_byte[13] = (tmp_crc16 & 0xff00) >> 8;		//data length MSB
	ret = i2c_write_bat(screen_mcu_p, screen_mcu_p->i2caddr, write_byte, 14);	

	do {
		usleep(500000);
		ret = get_mcu_bootloader_response(screen_mcu_p);
	} while (ret == MCU_BOOTLOADER_STATUS_BUSY);

    return ret;       
}

static int update_multi_blocks(struct screen_mcu_drv *screen_mcu_p,  char* update_buf){
	int ret =-1;
	uint8_t did_buf[32] = {'\0'};
	int block_infos[4][3] = {0};
	int block_num = (update_buf[36]<<24) | (update_buf[37]<<16) | (update_buf[38]<<8) | update_buf[39];
	int all_len = 0;
	if(block_num <= 0 || block_num > 4){
		err_out("%s, busnum:%d, i2caddr:0x%02x,block_num:%d\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, block_num);
		return ret;	
	}

	for(int i=0; i<block_num; i++){
		block_infos[i][0] = update_buf[40+12*i];//type;
		block_infos[i][1] = (update_buf[88+8*i]<<24) | (update_buf[88+8*i+1]<<16) | (update_buf[88+8*i+2]<<8) | update_buf[88+8*i+3];
		block_infos[i][2] = (update_buf[88+8*i+4]<<24) | (update_buf[88+8*i+5]<<16) | (update_buf[88+8*i+6]<<8) | update_buf[88+8*i+7];	
		info_out("%s, busnum:%d, i2caddr:0x%02x,block_infos[%d], type:%c start_addr:%d len:%d \n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, i, block_infos[i][0], block_infos[i][1], block_infos[i][2]);
		all_len = all_len+block_infos[i][2];
	}
	
	do{
        BREAK_ON_ERR(get_did_info(screen_mcu_p, DID_NUMBER_I2C_VERSION, did_buf));
        info_out("%s, busnum:%d, i2caddr:0x%02x,DID_NUMBER_I2C_VERSION:%s\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, did_buf);
        BREAK_ON_ERR(get_did_info(screen_mcu_p, DID_NUMBER_DISPLAY_PN, did_buf));
        info_out("%s, busnum:%d, i2caddr:0x%02x,DID_NUMBER_DISPLAY_PN:%s\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, did_buf);
        uint8_t update_pn[32] = {'\0'};
        strncpy((char *)update_pn, update_buf+12, 20);
        info_out("%s, busnum:%d, i2caddr:0x%02x,update_pn:%s\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, update_pn);
        BREAK_ON_ERR(strcmp((char *)update_pn, (char *)did_buf));
        BREAK_ON_ERR(get_did_info(screen_mcu_p, DID_NUMBER_SESSION, did_buf));
        if(did_buf[0] == MCU_APP_MODE){
            BREAK_ON_ERR(request_bootloader(screen_mcu_p));
        }else if (did_buf[0] == MCU_BOOTLOADER_MODE){
            //do nothing
        }else{
            err_out("%s, busnum:%d, i2caddr:0x%02x,DID_NUMBER_SESSION!=0x01:0x%02x\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, did_buf[0]);
            ret = -1;
            break;
        }
		for(int i=0, offset=0; i<block_num; i++){
		    BREAK_ON_ERR(request_erase_flash_specific(screen_mcu_p, block_infos[i][0], block_infos[i][2]));
		    BREAK_ON_ERR(send_data_file_multi_blocks(screen_mcu_p, update_buf+block_infos[i][1], block_infos[i][2], offset, all_len));
			offset = offset+block_infos[i][2];
		}
		if(ret){
			err_out("%s, busnum:%d, i2caddr:0x%02x, erro downloader file\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
			break;		
		}
        BREAK_ON_ERR(request_reset_mcu(screen_mcu_p));
        wait_time(screen_mcu_p, 2000000);
        for(int i=0; i<5; i++){
	        wait_time(screen_mcu_p, 5000000);
	        if(!get_did_info(screen_mcu_p, DID_NUMBER_SESSION, did_buf)){
	            if (did_buf[0] != MCU_APP_MODE) {
                    err_out("%s, busnum:%d, i2caddr:0x%02x,DID_NUMBER_SESSION!=0x01:0x%02x\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, did_buf[0]);
	            }else{
	                break;
	            }
	        }
	    }
	    if (did_buf[0] != MCU_APP_MODE) {
	        ret = -1;
	    }else{
	         BREAK_ON_ERR(get_did_info(screen_mcu_p, DID_NUMBER_SUPPLIER_SOFTWARE_VERSION, did_buf));
             info_out("%s, busnum:%d, i2caddr:0x%02x,DID_NUMBER_SUPPLIER_SOFTWARE_VERSION:%s\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, did_buf);
	    }
    }while(0);

	return ret;
}

static int start_update(struct screen_mcu_drv *screen_mcu_p){
    int ret =-1;
    uint8_t did_buf[32] = {'\0'};
    char updateFle_path[128]={'\0'};
	char * p_temp = strstr(screen_mcu_p->request," -p "); 
	snprintf(screen_mcu_p->reply, 32, "update:0");	            
	wait_time(screen_mcu_p, 100000);
	if(p_temp){
	    strcpy(updateFle_path, p_temp+4);
	    info_out("%s, busnum:%d, i2caddr:0x%02x,updateFle_path:%s\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, updateFle_path);
	    int update_buf_len = 0;
	    char* update_buf = load_file(updateFle_path, &update_buf_len);
	    if(update_buf){
			if(update_buf[0]==0xFE&&update_buf[1]==0xFE&&update_buf[2]==0xFE&&update_buf[3]==0xFE){
				ret = update_multi_blocks(screen_mcu_p, update_buf);							
			}else{
			    do{
			        BREAK_ON_ERR(get_did_info(screen_mcu_p, DID_NUMBER_I2C_VERSION, did_buf));
			        info_out("%s, busnum:%d, i2caddr:0x%02x,DID_NUMBER_I2C_VERSION:%s\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, did_buf);
			        BREAK_ON_ERR(get_did_info(screen_mcu_p, DID_NUMBER_DISPLAY_PN, did_buf));
			        info_out("%s, busnum:%d, i2caddr:0x%02x,DID_NUMBER_DISPLAY_PN:%s\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, did_buf);
			        uint8_t update_pn[32] = {'\0'};
			        strncpy((char *)update_pn, update_buf, 20);
			        info_out("%s, busnum:%d, i2caddr:0x%02x,update_pn:%s\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, update_pn);
			        BREAK_ON_ERR(strcmp((char *)update_pn, (char *)did_buf));
			        BREAK_ON_ERR(get_did_info(screen_mcu_p, DID_NUMBER_SESSION, did_buf));
			        if(did_buf[0] == MCU_APP_MODE){
			            BREAK_ON_ERR(request_bootloader(screen_mcu_p));
			        }else if (did_buf[0] == MCU_BOOTLOADER_MODE){
			            //do nothing
			        }else{
			            err_out("%s, busnum:%d, i2caddr:0x%02x,DID_NUMBER_SESSION!=0x01:0x%02x\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, did_buf[0]);
			            ret = -1;
			            break;
			        }
			        BREAK_ON_ERR(request_erase_flash(screen_mcu_p));
			        BREAK_ON_ERR(send_data_file(screen_mcu_p, update_buf+20, update_buf_len-20));
			        BREAK_ON_ERR(request_reset_mcu(screen_mcu_p));
			        wait_time(screen_mcu_p, 2000000);
			        for(int i=0; i<5; i++){
				        wait_time(screen_mcu_p, 1000000);
				        if(!get_did_info(screen_mcu_p, DID_NUMBER_SESSION, did_buf)){
				            if (did_buf[0] != MCU_APP_MODE) {
			                    err_out("%s, busnum:%d, i2caddr:0x%02x,DID_NUMBER_SESSION!=0x01:0x%02x\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, did_buf[0]);
				            }else{
				                break;
				            }
				        }
				    }
				    if (did_buf[0] != MCU_APP_MODE) {
				        ret = -1;
				    }else{
				         BREAK_ON_ERR(get_did_info(screen_mcu_p, DID_NUMBER_SUPPLIER_SOFTWARE_VERSION, did_buf));
			             info_out("%s, busnum:%d, i2caddr:0x%02x,DID_NUMBER_SUPPLIER_SOFTWARE_VERSION:%s\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, did_buf);
				    }
			    }while(0);
			}
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

static int getTFTState(struct screen_mcu_drv *screen_mcu_p, uint8_t* onoff){
    int ret;
    uint8_t buf[2] = {0};
    ret = i2c_read_bytes(screen_mcu_p, screen_mcu_p->i2caddr, 0x05, buf, 2);
    if(!ret){
        *onoff = buf[1];
    }
    info_out("%s, busnum:%d, i2caddr:0x%02x,*onoff:%d ret:%d\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, *onoff, ret);
    return ret;
}

static int setTFTState(struct screen_mcu_drv *screen_mcu_p, uint8_t onoff){
	uint8_t vals[] = {0x05, 0x03, 0x00, !!(onoff), 0};
	vals[4] = mcu_calc_crc8(vals, 4);

	return i2c_write_bat(screen_mcu_p, screen_mcu_p->i2caddr, vals, 5);
}

static int handle_request(struct screen_mcu_drv *screen_mcu_p){
    struct haiwei_info *haiwei_info_p =  screen_mcu_p->priv_data;
	if(!strcmp(screen_mcu_p->request, "lcd get backlight")){
	    uint8_t bklight = -1;
	    if(!getbacklight(screen_mcu_p, &bklight)){
	        snprintf(screen_mcu_p->reply, 32, "%d", bklight);
	    }else{
	        snprintf(screen_mcu_p->reply, 32, "erro");
	    }
	}else if(!strcmp(screen_mcu_p->request, "lcd get backlight state")){
	     uint8_t onoff = -1;
	     if(!getbacklightonoff(screen_mcu_p, &onoff)){
	        snprintf(screen_mcu_p->reply, 32, "%d", onoff);
	    }else{
	        snprintf(screen_mcu_p->reply, 32, "erro");
	    }
	}else if(!strcmp(screen_mcu_p->request, "lcd get TFT state")){
	     uint8_t onoff = -1;
	     if(!getTFTState(screen_mcu_p, &onoff)){
	        snprintf(screen_mcu_p->reply, 32, "%d", onoff);
	    }else{
	        snprintf(screen_mcu_p->reply, 32, "erro");
	    }
	}else if(!strcmp(screen_mcu_p->request, "lcd TFT on")){
	    setTFTState(screen_mcu_p, 1);
	}else if(!strcmp(screen_mcu_p->request, "lcd TFT off")){
	    setTFTState(screen_mcu_p, 0);
	}else if(!strcmp(screen_mcu_p->request, "lcd get supplier sver")){
		 if(haiwei_info_p->hw_ver[0]==2 && haiwei_info_p->hw_ver[3]>=2){
		 	snprintf(screen_mcu_p->reply, 32, "%d_%04d_%04d", haiwei_info_p->sw_ver[0], haiwei_info_p->sw_ver[1], haiwei_info_p->sw_ver[2]);
		 }else{
		 	snprintf(screen_mcu_p->reply, 32, "%d.%d.%d.%d", haiwei_info_p->sw_ver[0], haiwei_info_p->sw_ver[1], haiwei_info_p->sw_ver[2], haiwei_info_p->sw_ver[3]);
		 }
	}else if(!strcmp(screen_mcu_p->request, "lcd get supplier hver")){
		 snprintf(screen_mcu_p->reply, 32, "%d.%d.%d.%d", haiwei_info_p->hw_ver[0], haiwei_info_p->hw_ver[1], haiwei_info_p->hw_ver[2], haiwei_info_p->hw_ver[3]);
	}else if(!strcmp(screen_mcu_p->request, "lcd get supplier blver")){
		 snprintf(screen_mcu_p->reply, 32, "%d.%d.%d.%d", haiwei_info_p->bl_ver[0], haiwei_info_p->bl_ver[1], haiwei_info_p->bl_ver[2], haiwei_info_p->bl_ver[3]);
	}else if(!strcmp(screen_mcu_p->request, "lcd get supplier tpver")){
		 snprintf(screen_mcu_p->reply, 32, "%s", haiwei_info_p->tp_ver);
	}else if(!strncmp(screen_mcu_p->request, "lcd update -p ",  strlen("lcd update -p "))){
	    start_update(screen_mcu_p);
	}else if(!strncmp(screen_mcu_p->request, "lcd update tp -p ",  strlen("lcd update -p "))){
	    start_update(screen_mcu_p);
	}else{
	}
	return 0;
}

#include <fcntl.h>
static int tcon_erro_count = 0;
static void may_dump_dsi_reg(struct screen_mcu_drv *screen_mcu_p){
    int fd = -1;
    if(tcon_erro_count%60 == 0){
	    fd = open("/dev/log_collector/cmd", O_RDWR);
	    if(fd>=0){
		    lseek(fd, 0, SEEK_SET);
		    write(fd, "0", 2);
		    close(fd);
		    info_out("%s, busnum:%d, i2caddr:0x%02x, success tcon_erro_count:%d\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, tcon_erro_count);
	    }else{
	        warn_out("%s, busnum:%d, i2caddr:0x%02x, failed \n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
	        tcon_erro_count--;
	    }
    }
    if(tcon_erro_count<241){
        tcon_erro_count++;
    }
}

static int repo_mcu_erro(struct screen_mcu_drv *screen_mcu_p){
    static int try = 2;
	try--;
	if(try){
		return -1;
	}
    uint8_t buf[4] = {0};
    if(!i2c_read_bytes(screen_mcu_p, screen_mcu_p->i2caddr, 0x10, buf, 4)){
        if(buf[1] != 0x00){
            warn_out("%s, busnum:%d, i2caddr:0x%02x, erro:0x%02x, tcon_erro:0x%02x\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, buf[1], buf[3]);
            if(buf[1]&0x08){ //tcon erro
                uint8_t tcon_buf[2] = {0};
                if(!i2c_read_bytes(screen_mcu_p, screen_mcu_p->i2caddr, 0x96, tcon_buf, 2)){
                    warn_out("%s, busnum:%d, i2caddr:0x%02x, tcon_erro:0x%02x\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, tcon_buf[1]);
                }
                uint8_t tddi_buf[8] = {0};
                if(!i2c_read_bytes(screen_mcu_p, screen_mcu_p->i2caddr, 0x98, tddi_buf, 8)){
                    warn_out("%s, busnum:%d, i2caddr:0x%02x, tddi_erro:0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, tddi_buf[1], tddi_buf[2], tddi_buf[3], tddi_buf[4], tddi_buf[5], tddi_buf[6], tddi_buf[7]);
                }
                if(screen_mcu_p->busnum==1 || screen_mcu_p->busnum==5){
                    may_dump_dsi_reg(screen_mcu_p);
                }
            }else{
                tcon_erro_count = 0;
            }
        }
    }
    try = 2;
    return 0;
}

static void *request_handler_thread(void *thread_param){
	int wret;
	struct screen_mcu_drv *screen_mcu_p = thread_param;
	struct haiwei_info *haiwei_info_p =  screen_mcu_p->priv_data;
	pthread_mutex_lock(&screen_mcu_p->lock);
	do{
		if(screen_mcu_p->status){
		    if(screen_mcu_p->attach_chg){
		        usleep(100000);
		        int j=5;
		        while(get_did_info(screen_mcu_p, DID_NUMBER_SUPPLIER_SOFTWARE_VERSION, haiwei_info_p->sw_ver) && j--);
		        j=5;
		        while(get_did_info(screen_mcu_p, DID_NUMBER_SUPPLIER_HARDWARE_VERSION, haiwei_info_p->hw_ver) && j--);
		        j=5;
		        while(get_did_info(screen_mcu_p, DID_NUMBER_BOOTLOADER_VERSION, haiwei_info_p->bl_ver) && j--);
				if(!strcmp(screen_mcu_p->name, "panel0-0")){
					j=5;
		        	while(get_did_info(screen_mcu_p, DID_NUMBER_TP_VERSION, haiwei_info_p->tp_ver) && j--);				
				}
			    screen_mcu_p->attach_chg = 0;
		    }
		    if(screen_mcu_p->pwr_chg){
		        if(!setbacklightonoff(screen_mcu_p, screen_mcu_p->bl_on)){
		            screen_mcu_p->pwr_chg = 0;
		        }
		    }
		    if(screen_mcu_p->bkl_chg){
		        if(!setbacklight(screen_mcu_p, screen_mcu_p->bl)){
		            screen_mcu_p->bkl_chg = 0;
		        }
		    }
		    if(screen_mcu_p->request_chg){
			    if(!handle_request(screen_mcu_p)){
				    screen_mcu_p->request_chg = 0;	
			    }
		    }
		    
		    repo_mcu_erro(screen_mcu_p);
		}
		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);
		now.tv_sec=now.tv_sec+1;
		wret = pthread_cond_timedwait(&screen_mcu_p->request_ready_cond, &screen_mcu_p->lock, &now);
		//info_out("%s, busnum:%d, i2caddr:0x%02x, screen_mcu_p->status:%d screen_mcu_p->bl:%d, screen_mcu_p->bkl_chg:%d, new request\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, screen_mcu_p->status, screen_mcu_p->bl, screen_mcu_p->bkl_chg);
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

static unsigned screen_mcu_haiwei_attach(struct screen_mcu_drv *screen_mcu_p){
	info_out("%s, busnum:%d, i2caddr:0x%02x\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
	pthread_mutex_lock(&screen_mcu_p->lock);
	info_out("%s, busnum:%d, i2caddr:0x%02x\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
	
	screen_mcu_p->attach_chg = -1;
	screen_mcu_p->pwr_chg = 0;
	screen_mcu_p->bkl_chg = 0;
    if(!dtc_irq_init(screen_mcu_p)){
		pthread_create(&screen_mcu_p->dtc_irq_thread, NULL, dtc_irq_thread, screen_mcu_p);	
	}
	pthread_cond_signal(&screen_mcu_p->request_ready_cond);
	pthread_mutex_unlock(&screen_mcu_p->lock);
	
	return 0;
}

static unsigned screen_mcu_haiwei_detach(struct screen_mcu_drv *screen_mcu_p){
	info_out("%s, busnum:%d, i2caddr:0x%02x\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
	//pthread_mutex_lock(&screen_mcu_p->lock);
	if(screen_mcu_p->dtc_irq_thread){
		pthread_cancel(screen_mcu_p->dtc_irq_thread);
		pthread_join(screen_mcu_p->dtc_irq_thread, NULL);
		screen_mcu_p->dtc_irq_thread=0;
	}
	//pthread_mutex_unlock(&screen_mcu_p->lock);
	return 0;
}

unsigned screen_mcu_haiwei_init(struct screen_mcu_drv *screen_mcu_p){
    screen_mcu_p->attach_handler = &screen_mcu_haiwei_attach;
    screen_mcu_p->set_request_handler = &set_request_haiwei;
	screen_mcu_p->get_reply_handler = &get_reply_haiwei;
	screen_mcu_p->detach_handler = &screen_mcu_haiwei_detach;
	screen_mcu_p->bl_on = 1;
	screen_mcu_p->bl = 50;
	info_out("%s, busnum:%d, i2caddr:0x%02x\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
	if(screen_mcu_p->i2cdev <= 0){
	    screen_mcu_p->i2cdev = get_i2cdev_with_bus(screen_mcu_p->busnum);
	    if(screen_mcu_p->i2cdev <= 0){
		    err_out("%s, busnum:%d, i2caddr:0x%02x, can't open i2cdev:%d\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, screen_mcu_p->i2cdev);
		    return -1;	
	    }
	}
	if(!screen_mcu_p->priv_data){
	    struct haiwei_info *haiwei_info_p = (struct haiwei_info *)malloc(sizeof(struct haiwei_info));
	    memset(haiwei_info_p, 0x00, sizeof(struct haiwei_info));
	    screen_mcu_p->priv_data = haiwei_info_p;
	}
	if(!screen_mcu_p->request_handler_thread){
		pthread_create(&screen_mcu_p->request_handler_thread, NULL, request_handler_thread, screen_mcu_p);
	}
	
	return 0;
}



