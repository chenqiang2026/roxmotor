/*===========================================================================*\
* Copyright 2025 Aptiv, Inc., All Rights Reserved.
* Aptiv Confidential
\*===========================================================================*/
#include "I2cProcessHandler.h"

int main(void)
{
    const std::string i2cDevice = "/dev/i2c2";
    int i2cFd = -1;
    uint32_t i2cAddr = 0x30;

    i2cDriverInfo I2CInfo = {
        .i2cDevPath = i2cDevice, 
        .i2cFd = i2cFd, 
        .i2cDevAddr = i2cAddr
    };

    //init
    I2CInfo.i2cFd = i2cProcessHandler::i2cInitHandler(I2CInfo.i2cDevPath);
    if (-1 == I2CInfo.i2cFd)
    {
        printf("i2cInit failed\n");
        return -1;
    }

    //i2c write
    uint8_t writeData[] = {0x04,0x3,0x00,0x00,0x64};    //byte0 is register addr
    if (i2cProcessHandler::i2cWriteHandler(I2CInfo,writeData,sizeof(writeData)/sizeof(uint8_t)))
        printf("i2cWrite i2caddr:%#x regaddr:%#x succeeded\n",I2CInfo.i2cDevAddr,writeData[0]);
    else
        printf("i2cWrite failed\n");

    //i2c read
    uint8_t readData[2] = {0x0};
    if (i2cProcessHandler::i2cReadHandler(I2CInfo,writeData[0],sizeof(writeData[0]),readData,sizeof(readData)/sizeof(uint8_t)))
        printf("i2cread succeeded,readData[0]=%#x,readData[1]=%#x\n",readData[0],readData[1]);
    else
        printf("i2cread failed\n");

    //deinit
    i2cProcessHandler::i2cDeinitHandler(I2CInfo.i2cFd);

    return 0;
}
