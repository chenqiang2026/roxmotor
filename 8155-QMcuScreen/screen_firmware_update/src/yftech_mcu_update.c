#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <unix.h>
#include <libgen.h>
#include <sys/stat.h>

#include <amss/i2c_client.h>

#include "yftech_mcu_update.h"
#include "log_utils.h"

#define MCU_REG_TP_ADDRESS		0x20	//TP Report
#define MCU_REG_BL_ADDRESS		0x04	//Backlight on/off
#define MCU_REG_DI_ADDRESS		0x03	//Dimming info
#define MCU_REG_LB_DI_LVL_ADDRESS	0x07	//Lightbar dimming level info
#define MCU_REG_LB_ADDRESS	        0x05    //Lightbar backlight on/off
#define MCU_REG_DIAG_WRITE_ADDRESS			0x40	//diagnosis wirte
#define MCU_REG_DIAG_READ_ADDRESS			0x41	//diagnosis	read
#define MCU_REG_BOOTLOADER_WRITE_ADDRESS	0x60	//bootloader write
#define MCU_REG_BOOTLOADER_READ_ADDRESS		0x61	//bootloader response

#define MCU_SERVICE_ID_MCU_READ			0x22	//mcu read
#define MCU_SERVICE_ID_0X2E				0x2E	//service id 0x2E
#define MCU_SERVICE_ID_MCU_UPDATE		0x10	//mcu update request

#define MCU_SERVICE_ID_ERASE_RANGE		0x81	//Erase Range Command
#define MCU_SERVICE_ID_REQUEST_DOWNLOAD	0x82	//Request Download  Command
#define MCU_SERVICE_ID_TRANSFER_DATA	0x83	//Transfer Data  Command
#define MCU_SERVICE_ID_CHECKSUM			0x84	//Checksum  Command
#define MCU_SERVICE_ID_RESET			0x85	//Reset Command

#define DID_NUMBER_I2C_VERSION	0xFEF3	//DID number i2c version
#define DID_NUMBER_SOFTWARE_VERSION		0xFEF0 // software version
#define DID_NUMBER_DISPLAY_PN	0xFEF5	//display pn
#define DID_NUMBER_SESSION		0xFEF7	//session
#define DID_NUMBER_UNIQUE_ID	0xFEFC	//unique id

#define MCU_APP_MODE			0x01
#define MCU_BOOTLOADER_MODE		0x02
#define MCU_TP_UPDAT_MODE       0x03
#define MCU_BRIDGE_UPDAT_MODE   0x04

#define MCU_RESPONSE_FAIL		-0x7F
#define MCU_BOOTLOADER_STATUS_OK				0
#define MCU_BOOTLOADER_STATUS_NO_OK				1
#define MCU_BOOTLOADER_STATUS_BUSY				2
#define MCU_BOOTLOADER_STATUS_RETRANSMISSION	3

#define A88_DISPLAY_PN	"8560010CMV0000"
#define A58_DISPLAY_PN	"8560010DSV0000"
#define A79_DISPLAY_PN	"8560010CRV0000"
#define A13_DISPLAY_PN	"8560010ACN0200"
#define A29_01_DISPLAY_PN	"8560010ARD0500"
#define A29_02_DISPLAY_PN	"8560013ARD0100"

#define A88_UNIQUE_ID	"0501"
#define A58_UNIQUE_ID	"0401"
#define A79_UNIQUE_ID	"0402"

#define MCU_SINGLE_DATA 128
#define MCU_PROTOCOL_HEAD_LEN 4
#define MCU_PROTOCOL_CRC 2

struct screen_firmware cur_firmware;
uint8_t MCU_I2C_ADDRESS;

static int mts_i2c_write_mcu(struct screen_firmware * screen_firmware_p, uint8_t i2caddr, uint8_t regaddr, uint8_t *txdata, uint8_t length){
    static char dbg_buf[1024];
    int dbg_pn;
	uint8_t wbuf[length+1];
	int retry;
	int ret;

	wbuf[0] = regaddr;
	memcpy(wbuf+1, txdata, length);

	do {
		if (i2c_set_slave_addr(screen_firmware_p->i2c_dev, i2caddr>>1, I2C_ADDRFMT_7BIT) < 0) {
			err_out("i2c_set_slave_addr erro\n");
			ret = -EPROTO;
			break;
		}
		
		dbg_pn = sprintf(dbg_buf, "I2CW[%02x.%02x] ", i2caddr, wbuf[0]);
		for (int i=1; i<length+1; i++)
			dbg_pn += sprintf(dbg_buf+dbg_pn, "%02x ", wbuf[i]);
		info_out("%s\n", dbg_buf);

		retry = 3;
		while (i2c_write(screen_firmware_p->i2c_dev, wbuf, length+1) < 0 && retry-- > 0)
			usleep(10000);

		if (retry < 0) {
			err_out("I2CW[0x%02x.0x%02x] %d bytes fail!\n", i2caddr, regaddr, length);
			ret = -EIO;
			break;
		}

		ret = 0;
	} while (0);

	return ret;
}

static int mts_i2c_read(struct screen_firmware * screen_firmware_p, uint8_t i2caddr, uint8_t regaddr, uint8_t *rxdata, uint8_t length){
    static char dbg_read_buf[1024];
    int dbg_pn;
	unsigned char wbuf[1];
	int retry;
	int ret;

	do {
		if (i2c_set_slave_addr(screen_firmware_p->i2c_dev, i2caddr>>1, I2C_ADDRFMT_7BIT) < 0) {
			err_out("i2c_set_slave_addr erro\n");
			ret = -EPROTO;
			break;
		}

		retry = 3;
		wbuf[0] = regaddr;
		while (i2c_combined_writeread(screen_firmware_p->i2c_dev, wbuf, 1, rxdata, length) < 0 && retry-- > 0)
			usleep(10000);

		if (retry < 0) {
			err_out("I2CR[0x%02x.0x%04x] %d bytes fail!\n", i2caddr, regaddr, length);
			ret = -EIO;
			break;
		}else{
		    dbg_pn = sprintf(dbg_read_buf, "I2CR[%02x.%02x] ", i2caddr, regaddr);
		    for (int i=0; i<length; i++)
			    dbg_pn += sprintf(dbg_read_buf+dbg_pn, "%02x ", rxdata[i]);
		    info_out("%s\n", dbg_read_buf);
		}

		ret = 0;
	} while (0);

	return ret;
}

static int mts_request_i2c_version(struct screen_firmware * screen_firmware_p)
{
	uint8_t write_byte[6] = {0};

	write_byte[0] = MCU_REG_DIAG_WRITE_ADDRESS;
	write_byte[1] = 3;
	write_byte[2] = MCU_SERVICE_ID_MCU_READ;
	write_byte[3] = DID_NUMBER_I2C_VERSION >> 8;
	write_byte[4] = DID_NUMBER_I2C_VERSION & 0x00ff;

	return mts_i2c_write_mcu(screen_firmware_p, screen_firmware_p->slave_addr, MCU_REG_DIAG_WRITE_ADDRESS, &write_byte[1], 4);
}

static void mts_get_i2c_version(struct screen_firmware * screen_firmware_p, char *rxdata)
{
	uint8_t buf[15] = {0};

	mts_i2c_read(screen_firmware_p, screen_firmware_p->slave_addr, MCU_REG_DIAG_READ_ADDRESS, buf, 14);

	if(buf[2] == (DID_NUMBER_I2C_VERSION >> 8) && buf[3] == (DID_NUMBER_I2C_VERSION & 0x00ff)) {
		strcpy(rxdata, (char *)&buf[4]);
	}
}

static int mts_request_software_version(struct screen_firmware * screen_firmware_p)
{
	char write_byte[6] = {0};

	write_byte[0] = MCU_REG_DIAG_WRITE_ADDRESS;
	write_byte[1] = 3;
	write_byte[2] = MCU_SERVICE_ID_MCU_READ;
	write_byte[3] = DID_NUMBER_SOFTWARE_VERSION >> 8;
	write_byte[4] = DID_NUMBER_SOFTWARE_VERSION & 0x00ff;

	return mts_i2c_write_mcu(screen_firmware_p, screen_firmware_p->slave_addr, MCU_REG_DIAG_WRITE_ADDRESS, &write_byte[1], 4);
}

static void mts_get_software_version(struct screen_firmware * screen_firmware_p, char *rxdata)
{
	uint8_t buf[25] = {0};

	mts_i2c_read(screen_firmware_p, screen_firmware_p->slave_addr, MCU_REG_DIAG_READ_ADDRESS, buf, 24);

	if(buf[2] == (DID_NUMBER_SOFTWARE_VERSION >> 8) && buf[3] == (DID_NUMBER_SOFTWARE_VERSION & 0x00ff)) {
		strcpy(rxdata, (char *)&buf[4]);
	}
}

static int mts_request_display_pn(struct screen_firmware * screen_firmware_p)
{
	char write_byte[6] = {0};

	write_byte[0] = MCU_REG_DIAG_WRITE_ADDRESS;
	write_byte[1] = 3;
	write_byte[2] = MCU_SERVICE_ID_MCU_READ;
	write_byte[3] = DID_NUMBER_DISPLAY_PN >> 8;
	write_byte[4] = DID_NUMBER_DISPLAY_PN & 0x00ff;

	return mts_i2c_write_mcu(screen_firmware_p, screen_firmware_p->slave_addr, MCU_REG_DIAG_WRITE_ADDRESS, &write_byte[1], 4);
}

static void mts_get_display_pn(struct screen_firmware * screen_firmware_p, char *rxdata)
{
	uint8_t buf[25] = {0};

	mts_i2c_read(screen_firmware_p, screen_firmware_p->slave_addr, MCU_REG_DIAG_READ_ADDRESS, buf, 24);

	if(buf[2] == (DID_NUMBER_DISPLAY_PN >> 8) && buf[3] == (DID_NUMBER_DISPLAY_PN & 0x00ff)) {
		strcpy(rxdata, (char *)&buf[4]);
	}
}

static int mts_get_session(struct screen_firmware * screen_firmware_p)
{
	uint8_t rxdata[6] = {0};

	memset(rxdata, 0xff, sizeof(rxdata));
	mts_i2c_read(screen_firmware_p, screen_firmware_p->slave_addr, MCU_REG_DIAG_READ_ADDRESS, rxdata, 5);

	info_out("DID number rxdata[2]: %#x\n", rxdata[2]);
	info_out("DID number rxdata[3]: %#x\n", rxdata[3]);
	if(rxdata[2] == (DID_NUMBER_SESSION >> 8) && rxdata[3] == (DID_NUMBER_SESSION & 0x00ff)) {
		info_out("The status of the command rxdata[4]: %#x\n", rxdata[4] & 0xff);
		return (rxdata[4] & 0xff);
	}

	return -1;
}

static int mts_request_session(struct screen_firmware * screen_firmware_p)
{
	char write_byte[6] = {0};

	write_byte[0] = MCU_REG_DIAG_WRITE_ADDRESS;
	write_byte[1] = 3;
	write_byte[2] = MCU_SERVICE_ID_MCU_READ;
	write_byte[3] = DID_NUMBER_SESSION >> 8;
	write_byte[4] = DID_NUMBER_SESSION & 0x00ff;

	return mts_i2c_write_mcu(screen_firmware_p, screen_firmware_p->slave_addr, MCU_REG_DIAG_WRITE_ADDRESS, &write_byte[1], 4);
}

static int mts_get_mcu_mode(struct screen_firmware * screen_firmware_p)
{
    int ret = -1;
    int retry=3;
    do{
	    mts_request_session(screen_firmware_p);
	    usleep(100000);
	    ret = mts_get_session(screen_firmware_p);
	    info_out("mts_get_mcu_mode:%d\n", ret);
	}while(retry-- && ret == -1);
	
	return ret;
}

static int mts_request_mcu_bootloader_mode(struct screen_firmware * screen_firmware_p, int is_for_tp)
{
	char write_byte[5] = {0};

	write_byte[0] = MCU_REG_DIAG_WRITE_ADDRESS;
	write_byte[1] = 2;
	write_byte[2] = MCU_SERVICE_ID_MCU_UPDATE;
	if(!is_for_tp){
		write_byte[3] = MCU_BOOTLOADER_MODE;
	}else if(is_for_tp == 1){
		write_byte[3] = MCU_TP_UPDAT_MODE;
	}else if(is_for_tp == 2){
	    write_byte[3] = MCU_BRIDGE_UPDAT_MODE;
	}else{
	    warn_out("mts_request_mcu_bootloader_mode unsupport mode %d\n", is_for_tp);
	}

	return mts_i2c_write_mcu(screen_firmware_p, screen_firmware_p->slave_addr, MCU_REG_DIAG_WRITE_ADDRESS, &write_byte[1], 3);
}

/****************************Info********************************************** 
 * Name:	CRC-16/XMODEM	   x16+x12+x5+1 
 * Width:	16 
 * Poly:	0x1021 
 * Init:	0x0000 
 * Refin:   False 
 * Refout:  False 
 * Xorout:  0x0000 
 * Alias:   CRC-16/ZMODEM,CRC-16/ACORN 
 *****************************************************************************/
static unsigned short mts_crc16Clac(unsigned char* data, unsigned int len)
{
	unsigned short wCRCin = 0x0000;
	unsigned short wCPoly = 0x1021;
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

static int mts_set_mcu_bootloader_erase(struct screen_firmware * screen_firmware_p, const unsigned char *data)
{
	unsigned short tmp_crc16 = 0;
	char write_byte[20] = {0};
	memset(write_byte, 0x0, 20);

	write_byte[0] = MCU_REG_BOOTLOADER_WRITE_ADDRESS;
	write_byte[1] = 11;	//data length LSB
	write_byte[2] = 0;	//data length MSB
	write_byte[3] = MCU_SERVICE_ID_ERASE_RANGE;	//service id
	write_byte[4] = data[60];//start addr
	write_byte[5] = data[61];
	write_byte[6] = data[62];
	write_byte[7] = data[63];
	write_byte[8] = data[64];//block len
	write_byte[9] = data[65];
	write_byte[10] = data[66];
	write_byte[11] = data[67];
	tmp_crc16 = mts_crc16Clac(&write_byte[1], 11);
	write_byte[12] = tmp_crc16 & 0x00ff;			//data length LSB
	write_byte[13] = (tmp_crc16 & 0xff00) >> 8;		//data length MSB
	return mts_i2c_write_mcu(screen_firmware_p, screen_firmware_p->slave_addr, MCU_REG_BOOTLOADER_WRITE_ADDRESS, &write_byte[1], 13);
}

static int mts_get_mcu_bootloader_response(struct screen_firmware * screen_firmware_p)
{
#define I2C_ERROR_RETRY_TIMES 40
	uint8_t rxdata[10] = {0xff};
	int ret = 0, cnt = 0;
	unsigned short tmp_crc16 = 0, i = 0;
	info_out("%#x\n", MCU_REG_BOOTLOADER_READ_ADDRESS);
	memset(rxdata, 0xff, sizeof(rxdata));
	do {
		if ((++cnt) < I2C_ERROR_RETRY_TIMES) {
			usleep(5000);
		} else {
			warn_out("I tried %d times and still failed\n", I2C_ERROR_RETRY_TIMES);
			break;
		}
		ret = mts_i2c_read(screen_firmware_p, screen_firmware_p->slave_addr, MCU_REG_BOOTLOADER_READ_ADDRESS, rxdata, 6);
	} while (ret < 0);
	tmp_crc16 = mts_crc16Clac(rxdata, 4);

	if ((tmp_crc16 & 0x00ff) == rxdata[4] && ((tmp_crc16 & 0xff00) >> 8) == rxdata[5]) {
		info_out("Crc16 success, The status of the command rxdata[3]: %#x\n", rxdata[3] & 0xff);
		return (rxdata[3] & 0xff);
	} else {
		info_out("Calc crc16 LSB: %#x\n", tmp_crc16 & 0x00ff);
		info_out("Calc crc16 MSB: %#x\n", (tmp_crc16 & 0xff00) >> 8);
		info_out("Recieve data:");
		for (i = 0; i < 6; i++) {
			info_out("%#x \n", rxdata[i]);
		}
		return MCU_BOOTLOADER_STATUS_NO_OK;
	}
}

static int mts_set_mcu_bootloader_download(struct screen_firmware * screen_firmware_p, const unsigned char *data)
{
	unsigned short tmp_crc16 = 0;
	char write_byte[20] = {0};
	memset(write_byte, 0x0, 20);

	write_byte[0] = MCU_REG_BOOTLOADER_WRITE_ADDRESS;
	write_byte[1] = 11;	//data length LSB
	write_byte[2] = 0;	//data length MSB
	write_byte[3] = MCU_SERVICE_ID_REQUEST_DOWNLOAD; //service id
	write_byte[4] = data[60];//start addr
	write_byte[5] = data[61];
	write_byte[6] = data[62];
	write_byte[7] = data[63];
	write_byte[8] = data[64];//block len
	write_byte[9] = data[65];
	write_byte[10] = data[66];
	write_byte[11] = data[67];
	tmp_crc16 = mts_crc16Clac(&write_byte[1], 11);
	write_byte[12] = tmp_crc16 & 0x00ff;			//data length LSB
	write_byte[13] = (tmp_crc16 & 0xff00) >> 8;		//data length MSB
	return mts_i2c_write_mcu(screen_firmware_p, screen_firmware_p->slave_addr, MCU_REG_BOOTLOADER_WRITE_ADDRESS, &write_byte[1], 13);
}

static char * mts_load_mcu_update_file(const char * filename, int *len, int *head_len)
{
	struct stat sStat;
    int fileDes;
	char *buf = NULL;	
	
    memset((char *) &sStat, 0x00, sizeof(struct stat));
	
	 if (0 != stat(filename, &sStat)){
       if (ENOENT == errno) // The path doesn't exist, or path is an empty string
       {
            info_out("The config path doesn't exist, or path is an empty string\n");
       }else{
			err_out("The config path stat failed\n");
       }
       return NULL;
     }
	
	info_out("config file filesize:%uB\n", sStat.st_size);
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
	
	*head_len = buf[12] | buf[13]<<8 | buf[14]<<16 | buf[15]<<24;

	//printf("mts_load_mcu_update_file , buf[60]:%02x, buf[61]:%02x, buf[62]:%02x,buf[63]:%02x\n", buf[60], buf[61], buf[62], buf[63]);
	//printf("mts_load_mcu_update_file , buf[64]:%02x, buf[65]:%02x, buf[66]:%02x,buf[67]:%02x\n", buf[64], buf[65], buf[66], buf[67]);
	return buf;
}

static int mts_set_mcu_bootloader_transfer_data(struct screen_firmware * screen_firmware_p, const char *txdata, int datalength, int current_index)
{
	int ret = 0;
	unsigned short tmp_crc16 = 0;
	char *write_byte;
	write_byte = malloc(datalength + MCU_PROTOCOL_HEAD_LEN + MCU_PROTOCOL_CRC + 1);
	if (write_byte == NULL) {
		err_out("kzalloc failed!\n");
		return -ENOMEM;
	}
	write_byte[0] = MCU_REG_BOOTLOADER_WRITE_ADDRESS;
	write_byte[1] = (datalength + MCU_PROTOCOL_HEAD_LEN) & 0x00ff;		//data length LSB
	write_byte[2] = ((datalength + MCU_PROTOCOL_HEAD_LEN) & 0xff00) >> 8;	//data length MSB
	write_byte[3] = MCU_SERVICE_ID_TRANSFER_DATA;	//service id
	write_byte[4] = (current_index&0xff);
	memcpy(write_byte + 5, txdata, datalength);
	for(int i=5; i<16; i++){
		info_out("write_byte[%d]:0x%x\n", i, write_byte[i]);
	}
   
	tmp_crc16 = mts_crc16Clac(&write_byte[1], datalength + MCU_PROTOCOL_HEAD_LEN);
	write_byte[datalength + 5] = tmp_crc16 & 0x00ff;		//data length LSB
	write_byte[datalength + 6] = (tmp_crc16 & 0xff00) >> 8;	//data length MSB
	info_out("write_byte[datalength + 5]:0x%x\n", write_byte[datalength + 5]);
	info_out("write_byte[datalength + 6]:0x%x\n", write_byte[datalength + 6]);
	
	ret = mts_i2c_write_mcu(screen_firmware_p, screen_firmware_p->slave_addr, MCU_REG_BOOTLOADER_WRITE_ADDRESS, &write_byte[1], datalength + MCU_PROTOCOL_HEAD_LEN + MCU_PROTOCOL_CRC);
	free(write_byte);
	return ret;
}

static int mts_set_mcu_bootloader_transfer_data_ex(struct screen_firmware * screen_firmware_p, const unsigned char *data, unsigned int cnt, unsigned int head_len)
{
	int err = MCU_UPDATE_FAIL;
	unsigned int total_index = 0, current_index = 0, valid_cnt = 0, for1st_index = 1, hexLength;
	int mcu_boot_status = -1;

	/* Ready read data and write */
	hexLength = cnt - head_len;
	if(hexLength%MCU_SINGLE_DATA){
	    total_index = hexLength/MCU_SINGLE_DATA + 1;
	}else{
	    total_index = hexLength/MCU_SINGLE_DATA;
	}
	info_out("hexLength = %u, total_index = %d\n", hexLength, total_index);
	for(current_index = 0; current_index < total_index; current_index++) {
		for1st_index = (current_index%0xff) + 1;
		if(current_index == (total_index - 1)) {
			valid_cnt = hexLength - (MCU_SINGLE_DATA * current_index);
		} else {
			valid_cnt = MCU_SINGLE_DATA;
		}

		do {
			info_out("uploading ... valid_cnt=%d,current_index=%#x,for1st_index=%#x, total_index=%d\n", valid_cnt, current_index, for1st_index, total_index);
			if(screen_firmware_p->callback){
				screen_firmware_p->callback(screen_firmware_p, current_index*100/total_index);	
			}
			err = mts_set_mcu_bootloader_transfer_data(screen_firmware_p, data + head_len + MCU_SINGLE_DATA * current_index, valid_cnt, for1st_index);
			if(err < 0) {
				warn_out("Send update data faild, Retry...\n");
			    usleep(200000);
			} else {
			    usleep(50000);
			}
			do {
				mcu_boot_status = mts_get_mcu_bootloader_response(screen_firmware_p);
			} while (mcu_boot_status == MCU_BOOTLOADER_STATUS_BUSY);
			if (mcu_boot_status == MCU_BOOTLOADER_STATUS_NO_OK) {
				err_out("mts_set_mcu_bootloader_transfer_data_ex MCU_BOOTLOADER_STATUS_NO_OK\n");
				return MCU_UPDATE_FAIL;
			} else if (mcu_boot_status == MCU_BOOTLOADER_STATUS_RETRANSMISSION) {
				err_out("MCU_BOOTLOADER_STATUS_RETRANSMISSION, Retry...\n");
				continue;
			}else if(mcu_boot_status != MCU_BOOTLOADER_STATUS_OK){
			    err_out("mts_set_mcu_bootloader_transfer_data_ex mcu_boot_status:0x%02x\n", mcu_boot_status);
			    return MCU_UPDATE_FAIL;
			}
		} while (mcu_boot_status != MCU_BOOTLOADER_STATUS_OK);
	}

	return MCU_UPDATE_SUCCESSFUL;
}

static int mts_set_mcu_bootloader_checksum(struct screen_firmware * screen_firmware_p, const unsigned char *data)
{
	unsigned short tmp_crc16 = 0;
	char write_byte[16] = {0};
	memset(write_byte, 0x0, 16);

	write_byte[0] = MCU_REG_BOOTLOADER_WRITE_ADDRESS;
	write_byte[1] = 13;	//data length LSB
	write_byte[2] = 0;	//data length MSB
	write_byte[3] = MCU_SERVICE_ID_CHECKSUM;	//service id
	write_byte[4] = data[60];//start addr
	write_byte[5] = data[61];
	write_byte[6] = data[62];
	write_byte[7] = data[63];
	write_byte[8] = data[64];//block len
	write_byte[9] = data[65];
	write_byte[10] = data[66];
	write_byte[11] = data[67];
	write_byte[12] = data[4];
	write_byte[13] = data[5];
	tmp_crc16 = mts_crc16Clac(&write_byte[1], 13);
	write_byte[14] = tmp_crc16 & 0x00ff;		//data length LSB
	write_byte[15] = (tmp_crc16 & 0xff00) >> 8;	//data length MSB
	return mts_i2c_write_mcu(screen_firmware_p, screen_firmware_p->slave_addr, MCU_REG_BOOTLOADER_WRITE_ADDRESS, &write_byte[1], 15);
}

static int mts_set_mcu_bootloader_reset(struct screen_firmware * screen_firmware_p)
{
	unsigned short tmp_crc16 = 0;
	char write_byte[20] = {0};
	memset(write_byte, 0x0, 20);

	write_byte[0] = MCU_REG_BOOTLOADER_WRITE_ADDRESS;
	write_byte[1] = 4;	//data length LSB
	write_byte[2] = 0;	//data length MSB
	write_byte[3] = MCU_SERVICE_ID_RESET;	//service id
	write_byte[4] = 1;	//reset(0x01)
	tmp_crc16 = mts_crc16Clac(&write_byte[1], 4);
	write_byte[5] = tmp_crc16 & 0x00ff;			//data length LSB
	write_byte[6] = (tmp_crc16 & 0xff00) >> 8;	//data length MSB
	return mts_i2c_write_mcu(screen_firmware_p, screen_firmware_p->slave_addr, MCU_REG_BOOTLOADER_WRITE_ADDRESS, &write_byte[1], 6);
}

enum {
	kStateFailed = -1,
	kStateSuccess = 0,
	kStateI2CError,
	kStateDisplayPNError,
	kStateEraseError,
	kStateDownloadError,
	kStateChecksumError,
	kStateMCUResetError,
	kStateModeError,
};

static int repo_screen_update_status(int status){
    char temp[64];
    int fd = -1;
    char buf[1] = {0};
    
    
    if(status){
       buf[0] = 1; 
    }else{
        buf[0] = 0;
    }
    
	snprintf(temp, sizeof(temp), "/tmp/screen_update_stat");
	fd = open(temp, O_RDWR|O_CREAT|O_CLOEXEC);
	if(fd < 0){
	    err_out("%s, failed open %s \n", __func__, temp);
	}else{
	    if (lseek(fd, 0, SEEK_SET) == 0 &&
			write(fd, buf, sizeof(buf)) > 0) {
			close(fd);
			return 0;
		}else{
		    err_out("%s, can't write file %s\n", __func__, temp);
		}
		close(fd);
	}
	
	return -1;
}

int yftech_mts_mcu_firmware_update(struct screen_firmware * screen_firmware_p, int is_for_tp){
	int ret = kStateSuccess;
	char temp[1024];
	char buf[256] = { 0 };
	int mcu_mode = 0, mcu_boot_status = 0;
	const unsigned char *data = NULL;
	unsigned int cnt = 0, head_len = 0;
	repo_screen_update_status(1);
	usleep(1500000);
	do{
		data = mts_load_mcu_update_file(screen_firmware_p->firmware_path, &cnt, &head_len);
		if(data == NULL || cnt <= 0 || cnt <= head_len || cnt <= head_len + MCU_SINGLE_DATA) {
			err_out("mcu update data is NULL! cnt:%d, head_len:%d \n", cnt, head_len);
			ret = kStateDownloadError;
			goto result;
		}

		info_out("Step 1: Get i2c version.\n");
		if (mts_request_i2c_version(screen_firmware_p) != 0) {
			ret = kStateI2CError;
			break;
		}
		usleep(100000);
		memset(&buf, 0, sizeof(buf));
		mts_get_i2c_version(screen_firmware_p, buf);
		info_out("i2c-version: %s\n", buf);
		
		info_out("Get solftware version.\n");
		if (mts_request_software_version(screen_firmware_p) != 0) {
			ret = kStateI2CError;
			break;
		}
		usleep(100000);
		mts_get_software_version(screen_firmware_p, screen_firmware_p->cur_version);
		memcpy(screen_firmware_p->new_version, data+36, 20);
		info_out("software-version: %s, new-version:%s\n", screen_firmware_p->cur_version,screen_firmware_p->new_version);
		
		info_out("Step 2: Get display PN.\n");
		if (mts_request_display_pn(screen_firmware_p) != 0) {
			ret = kStateI2CError;
			break;
		}
		usleep(100000);
		mts_get_display_pn(screen_firmware_p, screen_firmware_p->cur_pn);
		memcpy(screen_firmware_p->new_pn, data+16, 20);
		info_out("display-pn: %s, new-display-pn:%s\n", screen_firmware_p->cur_pn, screen_firmware_p->new_pn);
	
		if(screen_firmware_p->callback){
			if(screen_firmware_p->callback(screen_firmware_p, 0)){
				err_out("call back failed \n");
				ret = kStateI2CError;
				break;
			}
		}
		info_out("Step 3: Query mcu current status.\n");
		mcu_mode = mts_get_mcu_mode(screen_firmware_p);
		if (mcu_mode == MCU_RESPONSE_FAIL) {
			ret = kStateFailed;
			break;
		} else if (mcu_mode == MCU_APP_MODE) {
			/* request mcu enter bootloader mode */
			mts_request_mcu_bootloader_mode(screen_firmware_p, is_for_tp);
			int retry=3;
			do{
			    usleep(1000000);
			    mcu_mode = mts_get_mcu_mode(screen_firmware_p);
			    if (mcu_mode == -1 || mcu_mode == MCU_RESPONSE_FAIL || mcu_mode == MCU_APP_MODE) {
				    ret = kStateFailed;
			    }else{
			        ret = kStateSuccess;
			        break;
			    }
			}while(retry--);
			
			if(ret == kStateFailed){
			    err_out("go to update mode failed\n");
			    break;
			}
		}

		info_out("Step 4: Erase range (0x60, 0x81).\n");
		mts_set_mcu_bootloader_erase(screen_firmware_p, data);
		do {
			usleep(500000);
			mcu_boot_status = mts_get_mcu_bootloader_response(screen_firmware_p);
			if (mcu_boot_status == MCU_BOOTLOADER_STATUS_NO_OK) {
				ret = kStateEraseError;
				goto result;
			} else if (mcu_boot_status == MCU_BOOTLOADER_STATUS_RETRANSMISSION) {
				mts_set_mcu_bootloader_erase(screen_firmware_p, data);
			}
		} while (mcu_boot_status != MCU_BOOTLOADER_STATUS_OK);

		info_out("Step 5: Request download (0x60, 0x82).\n");
		mts_set_mcu_bootloader_download(screen_firmware_p, data);
		do {
			usleep(500000);
			mcu_boot_status = mts_get_mcu_bootloader_response(screen_firmware_p);
			if (mcu_boot_status == MCU_BOOTLOADER_STATUS_NO_OK) {
				err_out("mcu update fail\n");
				ret = kStateDownloadError;
				goto result;
			} else if (mcu_boot_status == MCU_BOOTLOADER_STATUS_RETRANSMISSION) {
				mts_set_mcu_bootloader_download(screen_firmware_p, data);
			}
		} while (mcu_boot_status != MCU_BOOTLOADER_STATUS_OK);
		
		info_out("Step 6: Transfer data (0x60, 0x83).\n");
		if(mts_set_mcu_bootloader_transfer_data_ex(screen_firmware_p, data, cnt, head_len) != MCU_UPDATE_SUCCESSFUL){
			ret = kStateDownloadError;
			goto result;		
		}

		/* checksum check (0x60, 0x84) */
		info_out("Step 7: Checksum check (0x60, 0x84).\n");
		mts_set_mcu_bootloader_checksum(screen_firmware_p, data);
		do {
			usleep(500000);
			mcu_boot_status = mts_get_mcu_bootloader_response(screen_firmware_p);
			if (mcu_boot_status == MCU_BOOTLOADER_STATUS_NO_OK) {
				ret = kStateChecksumError;
				goto result;
			} else if (mcu_boot_status == MCU_BOOTLOADER_STATUS_RETRANSMISSION) {
				mts_set_mcu_bootloader_checksum(screen_firmware_p, data);
			}
		} while (mcu_boot_status != MCU_BOOTLOADER_STATUS_OK);
				
		if(screen_firmware_p->callback){
			screen_firmware_p->callback(screen_firmware_p, 100);	
		}		
		info_out("Step 8: MCU reset (0x60, 0x85).\n");
		mts_set_mcu_bootloader_reset(screen_firmware_p);
		do {
			usleep(1000);
			mcu_boot_status = mts_get_mcu_bootloader_response(screen_firmware_p);
			if (mcu_boot_status == MCU_BOOTLOADER_STATUS_NO_OK) {
				err_out("mcu update fail\n");
				ret = kStateMCUResetError;
				goto result;
			}
		} while (mcu_boot_status != MCU_BOOTLOADER_STATUS_OK);

		info_out("Step 9: Query mcu current status.\n");
		usleep(2000000);
		/*/
		snprintf(temp, sizeof(temp), "/dev/i2c%d", screen_firmware_p->i2c_bus);
		info_out("temp: %s\n", temp);
		if(screen_firmware_p->i2c_dev > 0){
		    i2c_close(screen_firmware_p->i2c_dev);
		    screen_firmware_p->i2c_dev = -1;
		}
		screen_firmware_p->i2c_dev = i2c_open(temp);
		info_out("screen_firmware_p->i2c_dev: %d\n", screen_firmware_p->i2c_dev);
		if (screen_firmware_p->i2c_dev >= 0) {
			i2c_set_bus_speed(screen_firmware_p->i2c_dev, I2C_SPEED_STANDARD, NULL);	// 100 kbit/s
		}else{
			err_out("i2c_open failed %d\n", screen_firmware_p->i2c_bus);
			ret = kStateModeError;
			goto result;			
		}
		/*/
		for(int i=0; i<5; i++){
		    usleep(1000000);
		    mcu_mode = mts_get_mcu_mode(screen_firmware_p);
		    if (mcu_mode != MCU_APP_MODE) {
			    warn_out("reset not in app mode i:%d\n", i);
		    }else{
		        break;
		    }
		}
		if (mcu_mode != MCU_APP_MODE) {
            err_out("reset not in app mode\n");
            ret = kStateModeError;
            goto result;
        }
	}while(0);
result:
	if(data != NULL){
		free(data);
		data = NULL;
	}
	repo_screen_update_status(0);
	if (kStateSuccess != ret) {
		err_out("mcu update fail\n");
		return MCU_UPDATE_FAIL;
	}
	info_out("mcu update successfully\n");
	return MCU_UPDATE_SUCCESSFUL;
}
