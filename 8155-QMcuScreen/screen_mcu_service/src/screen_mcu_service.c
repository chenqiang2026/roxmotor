#include <amss/core/pdbg.h>
#include <ctype.h>
#include "screen_mcu_service.h"
#include "screen_mcu_yftech.h"
#include "screen_mcu_haiwei.h"
#include "screen_mcu_huayang.h"
#include "screen_mcu_hangsheng.h"
#include "screen_mcu_zheyi.h"
#include "screen_mcu_oufei.h"
#include "screen_mcu_chery.h"
#include "screen_mcu_yfmotor.h"

#define MAX_FILE_NAME_LEN	24

int get_i2cdev_with_bus(int busNum){
	char temp[24];
	int i2cdev;
	snprintf(temp, sizeof(temp), "/dev/i2c%d", busNum);
	i2cdev = i2c_open(temp);
    if (i2cdev >= 0) {
		i2c_set_bus_speed(i2cdev, I2C_SPEED_STANDARD, NULL);	// 100 kbit/s
		return i2cdev;
	}
	return i2cdev;
}

int i2c_write_bat(struct screen_mcu_drv *screen_mcu_p, uint8_t dev, uint8_t *buf, uint8_t len)
{
	static char dbg_buf[1024];
	int dbg_pn;
	int i;
	int retry;
	int ret;

	do {
		if (i2c_set_slave_addr(screen_mcu_p->i2cdev, dev>>1, I2C_ADDRFMT_7BIT) < 0) {
			ret = -EPROTO;
			break;
		}

		dbg_pn = sprintf(dbg_buf, "I2CW[%02x.%02x] ", dev, buf[0]);
		for (i=1; i<len; i++)
			dbg_pn += sprintf(dbg_buf+dbg_pn, "%02x ", buf[i]);
		info_out("%s\n", dbg_buf);

		retry = 3;
		while (i2c_write(screen_mcu_p->i2cdev, buf, len) < 0 && retry-- > 0)
			usleep(10000);

		if (retry < 0) {
			err_out("I2CW[%02x.%02x] %d bytes fail!\n", dev, buf[0], len-1);
			ret = -EIO;
			break;
		}

		ret = 0;
	} while (0);


	return ret;
}

int i2c_read_bytes(struct screen_mcu_drv *screen_mcu_p, uint8_t dev, uint8_t reg, uint8_t *buf, uint8_t len)
{
    static char dbg_read_buf[1024];
    int dbg_pn;
    int i;
	unsigned char wbuf[1];
	int retry;
	int ret;

	do {
		if (i2c_set_slave_addr(screen_mcu_p->i2cdev, dev>>1, I2C_ADDRFMT_7BIT) < 0) {
			ret = -EPROTO;
			break;
		}

		retry = 3;
		wbuf[0] = reg;
		while (i2c_combined_writeread(screen_mcu_p->i2cdev, wbuf, 1, buf, len) < 0 && retry-- > 0)
			usleep(10000);

		if (retry < 0) {
			err_out("I2CR[%02x.%02x] %d bytes fail!\n", dev, reg, len);
			ret = -EIO;
			break;
		}else{
		    dbg_pn = sprintf(dbg_read_buf, "I2CR[%02x.%02x] ", dev, reg);
		    for (i=0; i<len; i++)
			    dbg_pn += sprintf(dbg_read_buf+dbg_pn, "%02x ", buf[i]);
		    info_out("%s\n", dbg_read_buf);
		    ret = 0;
		}
	} while (0);

	return ret;
}

int i2c_read_bytes_direct(struct screen_mcu_drv *screen_mcu_p, uint8_t dev, uint8_t *buf, uint8_t len)
{
    static char dbg_read_buf[1024];
    int dbg_pn;
    int i;
	int retry;
	int ret;

	do {
		if (i2c_set_slave_addr(screen_mcu_p->i2cdev, dev>>1, I2C_ADDRFMT_7BIT) < 0) {
			ret = -EPROTO;
			break;
		}

		retry = 3;
		while (i2c_read(screen_mcu_p->i2cdev, buf, len) < 0 && retry-- > 0)
			usleep(10000);

		if (retry < 0) {
			err_out("I2CR[%02x.direct] %d bytes fail!\n", dev, len);
			ret = -EIO;
			break;
		}else{
		    dbg_pn = sprintf(dbg_read_buf, "I2CR[%02x.%02x] ", dev, buf[0]);
		    for (i=1; i<len; i++)
			    dbg_pn += sprintf(dbg_read_buf+dbg_pn, "%02x ", buf[i]);
		    info_out("%s\n", dbg_read_buf);
		    ret = 0;
		}
	} while (0);

	return ret;
}

static unsigned screen_mcu_attach(struct screen_mcu_drv *screen_mcu_p){
	return screen_mcu_p->attach_handler(screen_mcu_p);
}

static unsigned screen_mcu_detach(struct screen_mcu_drv *screen_mcu_p){
	return screen_mcu_p->detach_handler(screen_mcu_p);
}

static unsigned screen_mcu_status_set(char *recv_msg, size_t size, off64_t *offset, void *file_ctx){
	struct screen_mcu_drv *screen_mcu_p = file_ctx;
	unsigned nbytes = strlen(recv_msg);
	int le = nbytes - 1;
	while (le > 0 && isspace(recv_msg[le]))
		recv_msg[le--] = 0;

	info_out("%s, busnum:%d, i2caddr:0x%02x, recv_msg:%s\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, recv_msg);
	if (!strcmp(recv_msg, "start")) {
	    pthread_mutex_lock(&screen_mcu_p->lock);
		screen_mcu_p->status = 1;
		pthread_mutex_unlock(&screen_mcu_p->lock);
		if(screen_mcu_attach(screen_mcu_p)){
			err_out("%s, busnum:%d, i2caddr:0x%02x, start failed\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
			pthread_mutex_lock(&screen_mcu_p->lock);
		    screen_mcu_p->status = 0;
		    pthread_mutex_unlock(&screen_mcu_p->lock);
		}
	}else if(!strcmp(recv_msg, "stop")){
		pthread_mutex_lock(&screen_mcu_p->lock);
		screen_mcu_p->status = 0;
		pthread_mutex_unlock(&screen_mcu_p->lock);
		screen_mcu_detach(screen_mcu_p);
	}

	return nbytes;
}

static unsigned screen_mcu_status_get(char *reply_msg, size_t size, off64_t *offset, void *file_ctx){
	unsigned nbytes=0;

	if (*offset != 0)
		return 0;

	struct screen_mcu_drv *screen_mcu_p = file_ctx;
//	pthread_mutex_lock(&screen_mcu_p->lock);
	if(screen_mcu_p->status){
		nbytes = snprintf(reply_msg, size, "%s\n", "start");
		*offset += nbytes;
	}else{
		nbytes = snprintf(reply_msg, size, "%s\n", "stop");
		*offset += nbytes;
	}
//	pthread_mutex_unlock(&screen_mcu_p->lock);
	info_out("%s\n", __func__);
	return min(nbytes, size);
}

static unsigned screen_mcu_addr_set(char *recv_msg, size_t size, off64_t *offset, void *file_ctx){
	struct screen_mcu_drv *screen_mcu_p = file_ctx;
	info_out("%s, busnum:%d, i2caddr:0x%02x, recv_msg:%s\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, recv_msg);
	pthread_mutex_lock(&screen_mcu_p->lock);
	screen_mcu_p->i2caddr = strtol(recv_msg, NULL, 0);
	pthread_mutex_unlock(&screen_mcu_p->lock);
	return strlen(recv_msg);
}

static unsigned screen_mcu_addr_get(char *reply_msg, size_t size, off64_t *offset, void *file_ctx){
	unsigned nbytes=0;

	if (*offset != 0)
		return 0;	

	struct screen_mcu_drv *screen_mcu_p = file_ctx;
	pthread_mutex_lock(&screen_mcu_p->lock);
	nbytes = snprintf(reply_msg, size, "0x%02x\n", screen_mcu_p->i2caddr);
	*offset += nbytes;
	pthread_mutex_unlock(&screen_mcu_p->lock);
	info_out("%s\n", __func__);
	return min(nbytes, size);
}

static unsigned screen_mcu_bus_set(char *recv_msg, size_t size, off64_t *offset, void *file_ctx){
	struct screen_mcu_drv *screen_mcu_p = file_ctx;
	info_out("%s, busnum:%d, i2caddr:0x%02x, recv_msg:%s\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, recv_msg);
	pthread_mutex_lock(&screen_mcu_p->lock);
	screen_mcu_p->busnum = strtol(recv_msg, NULL, 0);
	pthread_mutex_unlock(&screen_mcu_p->lock);
	return strlen(recv_msg);
}

static unsigned screen_mcu_bus_get(char *reply_msg, size_t size, off64_t *offset, void *file_ctx){
	unsigned nbytes=0;

	if (*offset != 0)
		return 0;	

	struct screen_mcu_drv *screen_mcu_p = file_ctx;
	pthread_mutex_lock(&screen_mcu_p->lock);

	nbytes = snprintf(reply_msg, size, "%d\n", screen_mcu_p->busnum);
	*offset += nbytes;
	
	pthread_mutex_unlock(&screen_mcu_p->lock);
	info_out("%s\n", __func__);
	return min(nbytes, size);
}


static unsigned screen_mcu_protocol_set(char *recv_msg, size_t size, off64_t *offset, void *file_ctx){
	struct screen_mcu_drv *screen_mcu_p = file_ctx;
	info_out("%s, busnum:%d, i2caddr:0x%02x, recv_msg:%s\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, recv_msg);
	pthread_mutex_lock(&screen_mcu_p->lock);
	strcpy(screen_mcu_p->protocol, recv_msg);
	pthread_mutex_unlock(&screen_mcu_p->lock);
	return strlen(recv_msg);
}

static unsigned screen_mcu_protocol_get(char *reply_msg, size_t size, off64_t *offset, void *file_ctx){
	unsigned nbytes=0;
	if (*offset != 0)
		return 0;	

	struct screen_mcu_drv *screen_mcu_p = file_ctx;
	pthread_mutex_lock(&screen_mcu_p->lock);	
	nbytes = snprintf(reply_msg, size, "%s\n", screen_mcu_p->protocol);
	*offset += nbytes;
	info_out("%s, nbytes:%d, size:%d, reply_msg:%s\n", __func__, nbytes,(int)size, reply_msg);
	pthread_mutex_unlock(&screen_mcu_p->lock);

	return min(nbytes, size);
}

static unsigned screen_mcu_info_set(char *recv_msg, size_t size, off64_t *offset, void *file_ctx){
	struct screen_mcu_drv *screen_mcu_p = file_ctx;
	info_out("%s, busnum:%d, i2caddr:0x%02x, size:%d, *offset:%d,recv_msg:%s\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr, (int)size,(int)(*offset), recv_msg);
	if(screen_mcu_p->set_request_handler!=NULL){
		return screen_mcu_p->set_request_handler(screen_mcu_p, recv_msg);
	}else{
		err_out("%s, busnum:%d, i2caddr:0x%02x, set_request_handler not availiable\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
		return strlen(recv_msg);
	}
}

static unsigned screen_mcu_info_get(char *reply_msg, size_t size, off64_t *offset, void *file_ctx){
	unsigned nbytes=0;
	if (*offset != 0)
		return 0;
	
	usleep(10000);
	struct screen_mcu_drv *screen_mcu_p = file_ctx;
	if(screen_mcu_p->get_reply_handler!=NULL){
		nbytes = screen_mcu_p->get_reply_handler(screen_mcu_p, reply_msg);
		*offset += nbytes;
		return min(nbytes, size);
	}else{
		err_out("%s, busnum:%d, i2caddr:0x%02x, get_reply_handler not availiable\n", __func__, screen_mcu_p->busnum, screen_mcu_p->i2caddr);
		return 0;	
	}
	
}

static char *strip_space(char *str, int *plen)
{
	char *pstart = NULL;

	while (*str && isspace(*str))
		str++;

	if (*str) {
		pstart = str;
		do {
			str++;
		} while (*str && !isspace(*str));
		if (*str)
			*str = 0;
		if (plen)
			*plen = str - pstart;
	}

	return pstart;
}

int main(int argc, char *argv[])
{
	struct stat st;
	struct screen_mcu_drv* screen_mcu_drvs[6];
	int count = 0;
	char file_name[MAX_FILE_NAME_LEN];
	signal(SIGPIPE, SIG_IGN);	// don't catch SIGPIPE
	pthread_condattr_t condattr;
	pthread_condattr_init(&condattr);
	pthread_condattr_setclock(&condattr, CLOCK_MONOTONIC);

	FILE *fp = fopen("/usr/bin/screen_mcu.conf", "r");
	char temp[1024];
	char buf[512];
	char *ps;
	if(!fp){
		warn_out("%s, no config:%s\n", __func__, "/usr/bin/screen_mcu.conf");
		return 0;	
	}

	while (fgets(buf, sizeof(buf), fp)) {
		info_out("%s, fgets(buf:%s \n", __func__, buf);
		if((ps = strstr(buf, "begin"))){
			screen_mcu_drvs[count] = (struct screen_mcu_drv*)malloc(sizeof(struct screen_mcu_drv));	
			memset(screen_mcu_drvs[count], 0x00, sizeof(struct screen_mcu_drv));
			screen_mcu_drvs[count]->power_gpio = -1;
			count++;
		}else if((ps = strstr(buf, "name"))){
			ps = strchr(ps+4, '=');
			if (ps) {
				ps = strip_space(ps+1, NULL);
				if (ps) {
					strcpy(screen_mcu_drvs[count-1]->name, ps);
				}
			}
		}else if((ps = strstr(buf, "i2cbus"))){
			ps = strchr(ps+6, '=');
			if (ps) {
				ps = strip_space(ps+1, NULL);
				if (ps) {
					screen_mcu_drvs[count-1]->busnum = strtol(ps, NULL, 0);
				}
			}
		}else if((ps = strstr(buf, "i2caddr"))){
			ps = strchr(ps+7, '=');
			if (ps) {
				ps = strip_space(ps+1, NULL);
				if (ps) {
					screen_mcu_drvs[count-1]->i2caddr = strtol(ps, NULL, 0);
				}
			}
		}else if((ps = strstr(buf, "protocol"))){
			ps = strchr(ps+8, '=');
			if (ps) {
				ps = strip_space(ps+1, NULL);
				if (ps) {
					strcpy(screen_mcu_drvs[count-1]->protocol, ps);
				}
			}
		}else if((ps = strstr(buf, "tpirqgpio"))){
			ps = strchr(ps+8, '=');
			if (ps) {
				ps = strip_space(ps+1, NULL);
				if (ps) {
					screen_mcu_drvs[count-1]->tp_irq_gpio = strtol(ps, NULL, 0);
				}
			}
		}else if((ps = strstr(buf, "dtcirqgpio"))){
			ps = strchr(ps+8, '=');
			if (ps) {
				ps = strip_space(ps+1, NULL);
				if (ps) {
					screen_mcu_drvs[count-1]->dtc_irq_gpio = strtol(ps, NULL, 0);
				}
			}
		}else if((ps = strstr(buf, "motor"))){
			ps = strchr(ps+5, '=');
			if (ps) {
				ps = strip_space(ps+1, NULL);
				if (ps) {
					screen_mcu_drvs[count-1]->motor_type = strtol(ps, NULL, 0);
				}
			}
		}else if((ps = strstr(buf, "tfterro"))){
			ps = strchr(ps+5, '=');
			if (ps) {
				ps = strip_space(ps+1, NULL);
				if (ps) {
					screen_mcu_drvs[count-1]->show_tft_erro = strtol(ps, NULL, 0);
				}
			}
		}else if((ps = strstr(buf, "illuminance"))){
			ps = strchr(ps+5, '=');
			if (ps) {
				ps = strip_space(ps+1, NULL);
				if (ps) {
					screen_mcu_drvs[count-1]->illuminance_sensor = strtol(ps, NULL, 0);
				}
			}
		}else if((ps = strstr(buf, "powergpio"))){
			ps = strchr(ps+8, '=');
			if (ps) {
				ps = strip_space(ps+1, NULL);
				if (ps) {
				    snprintf(temp, sizeof(temp), "/dev/gpio/%s/value", ps);
					screen_mcu_drvs[count-1]->power_gpio = open(temp, O_RDWR);
					info_out("%s, fgets(buf:%s temp:%s screen_mcu_drvs[count-1]->power_gpio:%d\n", __func__, buf, temp, screen_mcu_drvs[count-1]->power_gpio);
				}
			}
		}else if((ps = strstr(buf, "end"))){
		    pthread_mutex_init(&(screen_mcu_drvs[count-1]->lock), NULL);
		    pthread_cond_init(&(screen_mcu_drvs[count-1]->request_ready_cond), &condattr);
		    if (!strncmp(screen_mcu_drvs[count-1]->protocol, "yftech", 6)) {
                screen_mcu_yftech_init(screen_mcu_drvs[count-1]);
            }else if (!strncmp(screen_mcu_drvs[count-1]->protocol, "haiwei", 6)) {
                screen_mcu_haiwei_init(screen_mcu_drvs[count-1]);
            }else if (!strncmp(screen_mcu_drvs[count-1]->protocol, "huayang", 7)) {
                screen_mcu_huayang_init(screen_mcu_drvs[count-1]);
            }else if (!strncmp(screen_mcu_drvs[count-1]->protocol, "hangsheng", 9)) {
                screen_mcu_hangsheng_init(screen_mcu_drvs[count-1]);
            }else if (!strncmp(screen_mcu_drvs[count-1]->protocol, "zheyi", 5)) {
                screen_mcu_zheyi_init(screen_mcu_drvs[count-1]);
            }else if (!strncmp(screen_mcu_drvs[count-1]->protocol, "oufei", 5)) {
                screen_mcu_oufei_init(screen_mcu_drvs[count-1]);
            }else if (!strncmp(screen_mcu_drvs[count-1]->protocol, "chery", 5)) {
                screen_mcu_chery_init(screen_mcu_drvs[count-1]);
            }else if (!strncmp(screen_mcu_drvs[count-1]->protocol, "cms022", 6)) {
                screen_mcu_cms022_init(screen_mcu_drvs[count-1]);
            }else if (!strncmp(screen_mcu_drvs[count-1]->protocol, "yfmotor", 7)) {
                screen_mcu_yfmotor_init(screen_mcu_drvs[count-1]);
            }else{
                screen_mcu_yftech_init(screen_mcu_drvs[count-1]);
            }
		}else{
			
		}
	}
	fclose(fp);


	if (stat("/proc/mount/dev/screen_mcu", &st)) {
		pdbg_init("/dev/screen_mcu"); //create a node for user function
		usleep(20000);
	}
	
	struct pdbg_ops_s ops_status;
	memset(&ops_status, 0x0, sizeof(struct pdbg_ops_s));
	ops_status.read = screen_mcu_status_get;
	ops_status.write  = screen_mcu_status_set;


	struct pdbg_ops_s ops_addr;
	memset(&ops_addr, 0x0, sizeof(struct pdbg_ops_s));
	ops_addr.read = screen_mcu_addr_get;
	ops_addr.write  = screen_mcu_addr_set;

	struct pdbg_ops_s ops_bus;
	memset(&ops_bus, 0x0, sizeof(struct pdbg_ops_s));
	ops_bus.read = screen_mcu_bus_get;
	ops_bus.write  = screen_mcu_bus_set;
	
	struct pdbg_ops_s ops_protocol;
	memset(&ops_protocol, 0x0, sizeof(struct pdbg_ops_s));
	ops_protocol.read = screen_mcu_protocol_get;
	ops_protocol.write = screen_mcu_protocol_set;

	struct pdbg_ops_s ops_info;
	memset(&ops_info, 0x0, sizeof(struct pdbg_ops_s));
	ops_info.read = screen_mcu_info_get;
	ops_info.write = screen_mcu_info_set;

	info_out("%s, threadid:%d \n", __func__, pthread_self());

	for(int c=0; c<count; c++){	
		snprintf(file_name, MAX_FILE_NAME_LEN, "/%s/status", screen_mcu_drvs[c]->name);
		pdbg_create_file(file_name, 0666, ops_status, screen_mcu_drvs[c]); 	

		snprintf(file_name, MAX_FILE_NAME_LEN, "/%s/addr", screen_mcu_drvs[c]->name);
		pdbg_create_file(file_name, 0666, ops_addr, screen_mcu_drvs[c]); 
			
		snprintf(file_name, MAX_FILE_NAME_LEN, "/%s/bus", screen_mcu_drvs[c]->name);
		pdbg_create_file(file_name, 0666, ops_bus, screen_mcu_drvs[c]);

		snprintf(file_name, MAX_FILE_NAME_LEN, "/%s/protocol", screen_mcu_drvs[c]->name);
		pdbg_create_file(file_name, 0666, ops_protocol,screen_mcu_drvs[c]); 

		snprintf(file_name, MAX_FILE_NAME_LEN, "/%s/info", screen_mcu_drvs[c]->name);
		pdbg_create_file(file_name, 0666, ops_info, screen_mcu_drvs[c]); 
	}
    info_out("version:251125\n");
	pause();
	return 0;
}

