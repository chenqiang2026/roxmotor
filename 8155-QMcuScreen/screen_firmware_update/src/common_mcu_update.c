#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include "common_mcu_update.h"
#include "log_utils.h"

int common_mts_mcu_firmware_update(struct screen_firmware * screen_firmware_p, int is_for_tp)
{
    int ret = MCU_UPDATE_FAIL;
    char cfg_path[128] = {'\0'};
    char update_cmd[1024] = {'\0'};
    char read_buf[32] = {'\0'};
    int fd = -1;
    int progress = -1;
    snprintf(update_cmd, sizeof(update_cmd), "lcd update -p %s", screen_firmware_p->firmware_path);
    snprintf(cfg_path, sizeof(cfg_path), "/dev/screen_mcu/panel%d-%d/info", screen_firmware_p->i2c_bus, screen_firmware_p->slave_addr);
    info_out("%s start, cfg_path:%s, update_cmd:%s", __func__, cfg_path, update_cmd);
    fd = open(cfg_path, O_RDWR);
    if(fd >=0 ){
        lseek(fd, 0, SEEK_SET);
        ret = write(fd, update_cmd, strlen(update_cmd)+1);
        info_out("%s, wirte update_cmd ret:%d!", __func__, ret);
        if(ret == strlen(update_cmd)){
            do{
                usleep(1000000);
                lseek(fd, 0, SEEK_SET);
                ret = read(fd, read_buf, 32);
                if(ret<0){
                    info_out("%s, read_buf failed ret:%d", __func__, ret);
                    ret = MCU_UPDATE_FAIL;
                    break;
                }
                info_out("%s progress:%s, ret:%d", __func__, read_buf, ret);
                if(!strncmp(read_buf, "update", 6)){
                    if(!strncmp(&read_buf[7], "success", 7)){
                        if(screen_firmware_p->callback){
                            screen_firmware_p->callback(screen_firmware_p, 100);
                        }
                        ret = MCU_UPDATE_SUCCESSFUL;
                        break;
                    }else if(!strncmp(&read_buf[7], "failed", 6)){
                        ret = MCU_UPDATE_FAIL;
                        break;
                    }else{
                        if(screen_firmware_p->callback){
                            sscanf(&read_buf[7], "%d", &progress);
                            screen_firmware_p->callback(screen_firmware_p, progress);
                        }
                    }
                }
            }while(1);
            
            close(fd);
       }else{
            ret = MCU_UPDATE_FAIL;
            err_out("%s, wirte update_cmd failed!ret:%d", __func__, ret);
       }
    }
    
    return ret;    
}
