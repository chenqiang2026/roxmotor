#include "../inc/UpgradeProcess.h"
#include <cstdint>
#include <cstdio>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h> 
#include <sys/poll.h>
#include <thread>
#include <unistd.h> 
#include <cstring>
#include <iostream>
#include <vector>
#include <iomanip>
#include <sstream>
#include "../inc/CRC16XMODEM.h"
#include "../inc/Config.h"
#include "../inc/Logger.h"

using namespace std;
// constexpr const uint32_t mcuStartAddr = 0x00000000;
// constexpr const uint32_t touchStartAddr = 0x00008000;
// constexpr const uint32_t tconStartAddr = 0x00010000;
constexpr uint8_t BOOT_MODE_SIZE = 6;
constexpr uint8_t TRANSFER_CHUNK = 128;
constexpr uint8_t MAX_WRITE_RETRIES = 3;
constexpr uint8_t MAX_READ_RETRIES = 3;
constexpr uint8_t Firmware_ADDR_LEN = 4;
constexpr uint8_t Firmware_SIZE_LEN = 4;

UpgradeProcess::UpgradeProcess()
{
    binFileDecode_ = std::make_shared<BinFileDecode>();
}
UpgradeProcess::~UpgradeProcess()
{
  
}
bool UpgradeProcess::upgrade(int replyFd,ScreenType screenType,const std::string& binFile)
{    
    i2cDriverInfo i2cInfo;
    assignI2cInfo(screenType,i2cInfo); 
    std::string errmsg{};
    int i2cFd = i2cProcessHandler::i2cInitHandler(i2cInfo.i2cDevPath);
    i2cInfo.i2cFd = i2cFd;
    if(i2cFd < 0){
        errmsg = " UpgradeProcess::upgrade ,i2cProcessHandler::i2cInitHandler ERR : i2c init fd failed \n";
        std::cout<<errmsg.c_str()<<std::endl;
        ::write(replyFd, errmsg.c_str(), errmsg.size());
        return false;
    }
    // 打开bin文件
    if(!binFileDecode_->openFile(binFile)){
        errmsg="UpgradeProcess::upgrade 执行 bin文件打开失败";
        std::cout<<errmsg<<std::endl;
        ::write(replyFd, errmsg.c_str(), errmsg.size());
        i2cProcessHandler::i2cDeinitHandler(i2cFd);
        return false;
    }
    if (!binFileDecode_->decodeBinFileHeadInfo()){
        errmsg = "bin文件解析头文件失败";
        std::cout << errmsg << std::endl;
        ::write(replyFd, errmsg.c_str(), errmsg.size());
        i2cProcessHandler::i2cDeinitHandler(i2cFd);
        return false;
    }

    std::cout<<"UpgradeProcess::upgrade 执行 binFileDecode_->decodeBinFileHeadInfo方法执行完成"<<std::endl;

    if(!binFileDecode_->isHeaderValid()) {
        errmsg="bin文件 HeaderValid 无效";
        std::cout<<errmsg<<std::endl;
        ::write(replyFd, errmsg.c_str(), errmsg.size());
        i2cProcessHandler::i2cDeinitHandler(i2cFd);
        return false;
    }

    //BinFileHeader binHeader = binFileDecode_->getHeader();
    auto  binHeader = binFileDecode_->getHeader();

    if(!readI2CVersion(i2cInfo)){
        errmsg = "upgrade 方法 I2cVersion read failed\n";
        ::write(replyFd, errmsg.c_str(), errmsg.size());
        i2cProcessHandler::i2cDeinitHandler(i2cFd);
        return false;
    }

    if(!readPN(i2cInfo)){
        errmsg = "upgrade 方法 PN read failed\n";
        ::write(replyFd, errmsg.c_str(), errmsg.size());
        i2cProcessHandler::i2cDeinitHandler(i2cFd);
        return false;
    }

    int sessionValue = readSession(i2cInfo);
    if(sessionValue < 0){
        errmsg = "upgrade 方法 readSession failed\n";
        ::write(replyFd, errmsg.c_str(), errmsg.size());
        i2cProcessHandler::i2cDeinitHandler(i2cFd);
        return false;
    }
    if(sessionValue == 0x01){ //APP 模式
        if(!programmingSession(i2cInfo)) {
            errmsg = "upgrade 方法 programmingSession failed\n";
            ::write(replyFd, errmsg.c_str(), errmsg.size());
            i2cProcessHandler::i2cDeinitHandler(i2cFd);
            return false;
        }
    } else if(sessionValue!=0x01 && sessionValue!=0x02) {
        errmsg = "upgrade 方法 sessionValue sessionValue!=0x01 && sessionValue!=0x02 错误\n";
        ::write(replyFd, errmsg.c_str(), errmsg.size());
        SCREEN_LOGE("%s\n",errmsg.c_str());
        i2cProcessHandler::i2cDeinitHandler(i2cFd);
        return false;
    }
    
    std::vector<FirmwareInfo> modules;
    if (binFileDecode_->getBlockMcuValid()) {
        modules.emplace_back(FirmwareInfo{
            {0x00, 0x00, 0x00, 0x00},  //MCU物理地址
            0x31, //
            binHeader.blocks[0].startAddrDecValue,
            binHeader.blocks[0].lengthDecValue,
            true
        });
        modules.emplace_back(FirmwareInfo{
            {0x00, 0x00, 0x01, 0x00},  //TCON物理地址
            0x32,
            binHeader.blocks[1].startAddrDecValue,
            binHeader.blocks[1].lengthDecValue,
            true
        });    
    } else if (binFileDecode_->getBlockTouchValid()) {
        modules.emplace_back(FirmwareInfo{
            {0x00, 0x10, 0x00, 0x00}, //Touch物理地址
            0x33,
            binHeader.blocks[0].startAddrDecValue,
            binHeader.blocks[0].lengthDecValue,
            true
        });    
    }
    
    if (modules.empty()) {
        errmsg = "没有需要升级的模块\n";
        std::cout << errmsg << std::endl;
        ::write(replyFd, errmsg.c_str(), errmsg.size());
        i2cProcessHandler::i2cDeinitHandler(i2cFd);
        return false;
    }

    auto handleUpgradeError = [&](const std::string& msg) {
        std::cout << msg << std::endl;
        ::write(replyFd, msg.c_str(), msg.size());
        i2cProcessHandler::i2cDeinitHandler(i2cFd);
        return false;
    };

    // 封装IVI和Cluster共有的升级循环逻辑
    auto upgradeModulesLoop = [&]() {
        for(auto& module : modules) {
             std::cout << "模块各个字节:0x" << std::hex 
                      << static_cast<int>(module.firmwareAddr[0])
                      << static_cast<int>(module.firmwareAddr[1])
                      << static_cast<int>(module.firmwareAddr[2])
                      << static_cast<int>(module.firmwareAddr[3])
                      << std::dec << std::endl;
            if(module.valid) {
                std::cout << "模块有效,第一个字节是: 0x" << std::hex << static_cast<uint8_t>(module.oneInBlock) << std::dec << std::endl;
                switch(module.oneInBlock) {
                    case 0x31:
                        if(!upgradeModule(i2cInfo, module, binHeader.blocks[0].length, replyFd, "MCU")) {
                            return handleUpgradeError("MCU升级失败");
                        }
                        break;
                    case 0x32:
                        if(!upgradeModule(i2cInfo, module, binHeader.blocks[1].length, replyFd, "TCON")) {
                            return handleUpgradeError("TCON升级失败");
                        }
                        break;
                    case 0x33:
                        if(!upgradeModule(i2cInfo, module, binHeader.blocks[0].length, replyFd, "TOUCH")) {
                            return handleUpgradeError("TOUCH升级失败");
                        }
                        break;
                    default:
                        std::cout << "未知模块物理地址: 0x" << std::hex << static_cast<int>(module.oneInBlock) << std::dec << std::endl;
                        return handleUpgradeError("发现未知模块，升级终止");
                }
            }
        }
        return true;
    };

    if(screenType == ScreenType::IVI || screenType == ScreenType::Cluster) {
        std::cout << "ScreenType::" << (screenType == ScreenType::IVI ? "IVI" : "Cluster") << " 升级开始" << std::endl;
        if (!upgradeModulesLoop()) {
            return false;
        }     
    } else if(screenType == ScreenType::Ceiling) {
        std::cout << "ScreenType::Ceiling 升级开始" << std::endl;
        // 注意：Ceiling类型目前没有实现升级逻辑
    }
    
    if(!reset(i2cInfo)){
        errmsg = "upgrade 方法 reset failed\n";
        ::write(replyFd, errmsg.c_str(), errmsg.size());
        i2cProcessHandler::i2cDeinitHandler(i2cFd);
        return false;
    }
    int sessionValueLast = readSession(i2cInfo);
    if(sessionValueLast==0x01) {
        errmsg="经过bootloader模式后, sessionValueLast==0x01, 升级成功\n";
        std::cout<<errmsg<<std::endl;
        ::write(replyFd, errmsg.c_str(), errmsg.size());
    } else if(sessionValueLast==0x02) {
        errmsg="经过bootloader模式后, sessionValueLast!=0x01, 升级失败\n";
        std::cout<<errmsg<<std::endl;
        ::write(replyFd, errmsg.c_str(), errmsg.size());
        i2cProcessHandler::i2cDeinitHandler(i2cFd);
        return false;
    } else if((sessionValueLast!=0x01) && (sessionValueLast!=0x02)){
        errmsg="经过bootloader模式后, sessionValueLast!=0x01 && sessionValueLast!=0x02, 升级失败\n";
        std::cout<<errmsg<<std::endl;
        ::write(replyFd, errmsg.c_str(), errmsg.size());
        i2cProcessHandler::i2cDeinitHandler(i2cFd);
        return false;
    }
    //deinit
    i2cProcessHandler::i2cDeinitHandler(i2cFd);
    binFileDecode_->closeFile();
    return true;
}

bool UpgradeProcess::upgradeModule(i2cDriverInfo& i2cInfo, const FirmwareInfo& module,const uint8_t* lengthArray,
                                   int replyFd,const std::string& moduleName)
{
    if(!module.valid){
        return true; 
    }  
    // 擦除
    if(!eraseCommand(i2cInfo, module.firmwareAddr, lengthArray)) {
        std::string errmsg = "Erase " + moduleName + " failed\n";
        ::write(replyFd, errmsg.c_str(), errmsg.size());
        return false;
    }
    // 请求下载
    if(!requestDownload(i2cInfo, module.firmwareAddr, lengthArray)){
        std::string errmsg = "Request Download " + moduleName + " failed\n";
        ::write(replyFd, errmsg.c_str(), errmsg.size());
        return false;
    }
    // 传输数据
    uint16_t downloadCRC = 0;
    if (!transferData(i2cInfo, module.startAddr, module.length, moduleName, downloadCRC)) {
        std::string errmsg = "升级中Transfer Data " + moduleName + " failed\n";
        ::write(replyFd, errmsg.c_str(), errmsg.size());
        return false;
    }
    // 校验和
    if(!checksum(i2cInfo, module.firmwareAddr, lengthArray, downloadCRC)){
        std::string errmsg = "Checksum " + moduleName + " failed\n";
        ::write(replyFd, errmsg.c_str(), errmsg.size());
        return false;
    }
    return true;
}
void UpgradeProcess::assignI2cInfo(ScreenType screenType,i2cDriverInfo &i2cInfo)
{
    if(screenType == ScreenType::IVI) {
        i2cInfo.i2cDevPath =  Config::i2cDevIVI();
        i2cInfo.i2cDevAddr = Config::I2C_ADDR_IVI;   
    } else if (screenType == ScreenType::Cluster) {
        i2cInfo.i2cDevPath =  Config::i2cDevCluster();
        i2cInfo.i2cDevAddr = Config::I2C_ADDR_CLUSTER;
    } else if (screenType == ScreenType::Ceiling) {
        i2cInfo.i2cDevPath =  Config::i2cDevCeiling();
        i2cInfo.i2cDevAddr = Config::I2C_ADDR_CEILING;
    }
}

//step8
//主机发送Reset Command(0x60, 0x85) 给display
bool UpgradeProcess::reset(const i2cDriverInfo &info)
{
    std::vector<uint8_t> resetReqParam = {0x60, 0x04, 0x00, 0x85, 0x01};
    uint16_t resetCRC = CRC16XMODEM::calculate(resetReqParam.data() + 1, resetReqParam.size() - 1);
    resetReqParam.emplace_back(resetCRC & 0xFF);
    resetReqParam.emplace_back((resetCRC >> 8) & 0xFF);

    uint8_t resetRespData[6] = {0x00};

    int writeRetryCount = 0;
    int readRetryCount = 0;

    while (writeRetryCount < MAX_WRITE_RETRIES) {
        if(i2cProcessHandler::i2cWriteHandler(info, resetReqParam.data(), resetReqParam.size())) {
            // 写成功，进行读操作
            std::this_thread::sleep_for(std::chrono::milliseconds(5000));
            readRetryCount = 0;
            while (readRetryCount < MAX_READ_RETRIES) {
                if(i2cProcessHandler::i2cReadHandler(info, 0x61, sizeof(uint8_t), resetRespData, sizeof(resetRespData))) {
                    if(resetRespData[4] == 0x00) { // OK 
                        return true;
                    } else if(resetRespData[4] == 0x01) { // NOT_OK
                        SCREEN_LOGE("主机通过Reset Response(0x61)接收display校验结果失败, 校验结果 = 0x%02x\n", resetRespData[4]);
                        return false;
                    } else if (resetRespData[4] == 0x02) { // BUSY
                        readRetryCount++;
                        if (readRetryCount >= MAX_READ_RETRIES) {
                            SCREEN_LOGE("重试读操作 %d 次后仍然 BUSY\n", MAX_READ_RETRIES);
                            return false;
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
                        continue;
                    } else if(resetRespData[4] == 0x03) { // RETRANSMISSION
                        writeRetryCount++;
                        if (writeRetryCount >= MAX_WRITE_RETRIES) {
                            SCREEN_LOGE("重试写操作 %d 次后仍然需要重传\n", MAX_WRITE_RETRIES);
                            return false;
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
                        break; // 跳出读循环，重新进行写操作
                    } else {
                        // 未知响应码
                        SCREEN_LOGE("未知的Reset响应码: 0x%02X\n", resetRespData[4]);
                        return false;
                    }
                } else {
                    readRetryCount++;
                    if (readRetryCount >= MAX_READ_RETRIES) {
                        SCREEN_LOGE("重试读操作 %d 次后读取仍然失败\n", MAX_READ_RETRIES);
                        return false;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(5000));
                    continue;
                }
            }
        } else {
            SCREEN_LOGE("主机发送Reset Command(0x60, 0x85) 给display失败\n");
            writeRetryCount++;
            if (writeRetryCount >= MAX_WRITE_RETRIES) {
                SCREEN_LOGE("重试写操作 %d 次后仍然失败\n", MAX_WRITE_RETRIES);
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5000));
        }
    }   
    return false;
}

//step7
//主机通过Checksum Command(0x60, 0x84)发送主机计算的该block的checksum值 给display
bool UpgradeProcess::checksum(const i2cDriverInfo &info,const uint8_t screenFirmwareAddr[4], const uint8_t screenFirmwareLen[4],uint16_t downfileCRC)
{
    uint8_t  checksumRespData[6];

    std::vector<uint8_t> checksumReqParam = {0x60, 0x0D,0x00,0x84};
    for (int i = Firmware_ADDR_LEN - 1; i >= 0; --i) {
        checksumReqParam.emplace_back(screenFirmwareAddr[i]);
    }
    for (int j = Firmware_SIZE_LEN - 1; j >= 0; --j) {
        checksumReqParam.emplace_back(screenFirmwareLen[j]);
    }
    checksumReqParam.emplace_back(downfileCRC & 0xFF);
    checksumReqParam.emplace_back((downfileCRC >> 8) & 0xFF);

    uint16_t crc = CRC16XMODEM::calculate(checksumReqParam.data() + 1,checksumReqParam.size() - 1);
    checksumReqParam.emplace_back(crc & 0xFF);
    checksumReqParam.emplace_back((crc >> 8) & 0xFF);
    
    int writeRetryCount = 0;
    int readRetryCount = 0;

    while (writeRetryCount < MAX_WRITE_RETRIES) {
        if(i2cProcessHandler::i2cWriteHandler(info, checksumReqParam.data(), checksumReqParam.size())) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            readRetryCount = 0;
            while (readRetryCount < MAX_READ_RETRIES) {
                if(i2cProcessHandler::i2cReadHandler(info, 0x61, sizeof(uint8_t), checksumRespData, sizeof(checksumRespData))) {
                    if(checksumRespData[4] == 0x00) { // OK 
                        SCREEN_LOGD("主机通过Checksum Response(0x61)接收display校验结果成功\n");
                        return true;
                    } else if(checksumRespData[4] == 0x01) { // NOT_OK
                        SCREEN_LOGE("Checksum校验失败\n");
                        return false;
                    } else if (checksumRespData[4] == 0x02) { // BUSY 
                        readRetryCount++;
                        if (readRetryCount >= MAX_READ_RETRIES) {
                            SCREEN_LOGE("重试读操作 %d 次后仍然 BUSY\n", MAX_READ_RETRIES);
                            return false;
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(500));
                        continue; // 继续重试读
                    } else if(checksumRespData[4] == 0x03) { // RETRANSMISSION
                        writeRetryCount++;
                        if (writeRetryCount >= MAX_WRITE_RETRIES) {
                            SCREEN_LOGE("重试写操作 %d 次后仍然需要重传\n", MAX_WRITE_RETRIES);
                            return false;
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(500));
                        break;
                    } else {
                        SCREEN_LOGE("未知的Checksum响应码: 0x%02X\n", checksumRespData[4]);
                        return false;
                    }
                } else {
                    readRetryCount++;
                    if (readRetryCount >= MAX_READ_RETRIES) {
                        SCREEN_LOGE("重试读操作 %d 次后读取仍然失败\n", MAX_READ_RETRIES);
                        return false;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    continue;
                }
            }
        } else {
            SCREEN_LOGE("主机通过Checksum Command(0x60, 0x84)发送主机计算的该block的checksum值给display失败\n");
            writeRetryCount++;
            if (writeRetryCount >= MAX_WRITE_RETRIES) {
                SCREEN_LOGE("重试写操作 %d 次后仍然失败\n", MAX_WRITE_RETRIES);
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }   
    return false;
}

//step6
//主机通过Transfer Data Command(0x60, 0x83) 发送当前block升级包数据给display（每次发128Byte数据）
bool UpgradeProcess::transferData(i2cDriverInfo &info,uint32_t addrInBinFile,uint32_t screenFirmwareDecLen,const std::string &module,uint16_t &outChecksum)
{
    uint64_t transferCount = screenFirmwareDecLen % TRANSFER_CHUNK==0?screenFirmwareDecLen/TRANSFER_CHUNK:screenFirmwareDecLen/TRANSFER_CHUNK +1;

    std::cout<<"升级模块:" << module <<",总帧数:"<< transferCount <<std::endl;
    std::cout << "首地址:addrInBinFile:0x" <<std::hex << addrInBinFile << "总长度:"<< std::dec << screenFirmwareDecLen<<std::endl;
    std::cout <<"首地址+总长度:0x"<< std::hex<<(addrInBinFile + screenFirmwareDecLen) <<std::endl;
    uint32_t currentFrameIndex = 0;
    uint8_t seqNo = 1; //sequence frame:1-255，每包+1, 溢出回到 0;134的十六进制
    std::vector<uint8_t> transferDataReqParam;
    uint8_t transferDataResp[BOOT_MODE_SIZE]={0x00};
    std::vector<uint8_t> firmwareData(TRANSFER_CHUNK);
    //frame.reserve(TRANSFER_CHUNK+6); 
    std::vector<uint8_t> frame;
    std::vector<uint8_t> frameCRC;
    uint64_t frameCheckSum = 0;

    int writeRetryCount = 0;
    int readRetryCount = 0;
    uint32_t currentOffset = 0;
    std::string errmsg;

    while(currentFrameIndex < transferCount) {
        //frame.clear();
        frameCRC.clear();
        transferDataReqParam.clear();
        //uint32_t bytesToRead = TRANSFER_CHUNK;
        // if (currentFrameIndex == transferCount - 1 && screenFirmwareDecLen % TRANSFER_CHUNK != 0) {
        //     bytesToRead = screenFirmwareDecLen % TRANSFER_CHUNK;
        // }
        if (!binFileDecode_->readBlockData(addrInBinFile + currentOffset, 
                                           TRANSFER_CHUNK, 
                                           firmwareData)) {
            SCREEN_LOGE("读取固件%s数据失败,地址: 0x%x, 长度: %u", 
                       module.c_str(),
                       addrInBinFile + currentOffset, 
                       TRANSFER_CHUNK);
            return 0;
        }
        //更新偏移量
        currentOffset += TRANSFER_CHUNK;
        transferDataReqParam.emplace_back(0x60);
        transferDataReqParam.emplace_back(0x86);
        transferDataReqParam.emplace_back(0x00);
        transferDataReqParam.emplace_back(0x83);
        transferDataReqParam.emplace_back(seqNo); 
        transferDataReqParam.insert(transferDataReqParam.end(), firmwareData.begin(), firmwareData.end());
        // if (firmwareData.size() < TRANSFER_CHUNK) {
        //     frame.resize(TRANSFER_CHUNK, 0x00);
        // }
        uint16_t crc = CRC16XMODEM::calculate(transferDataReqParam.data() + 1, transferDataReqParam.size() - 1);
        // 将 crc 以小端放入（低字节先）
        transferDataReqParam.push_back(static_cast<uint8_t>(crc & 0xFF));
        transferDataReqParam.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));
        frameCRC.emplace_back(0x80);
        frameCRC.emplace_back(0x00);
        frameCRC.emplace_back(0x83);
        frameCRC.insert(frameCRC.end(),firmwareData.begin(),firmwareData.end());
        uint16_t framecrc = CRC16XMODEM::calculate(frameCRC.data(), frameCRC.size());
        frameCheckSum += framecrc;

        writeRetryCount = 0;
        bool frameTransferred = false;
        while (writeRetryCount < MAX_WRITE_RETRIES) {
            if(i2cProcessHandler::i2cWriteHandler(info, transferDataReqParam.data(), transferDataReqParam.size())) {
                SCREEN_LOGD("i2cWrite i2caddr:%2x regaddr:%2x succeeded\n", info.i2cDevAddr);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                readRetryCount = 0;
               
                while (readRetryCount < MAX_READ_RETRIES) {
                    if(i2cProcessHandler::i2cReadHandler(info, 0x61, sizeof(uint8_t), transferDataResp, sizeof(transferDataResp))) {
                        if(transferDataResp[3] == 0x00) { //OK
                            float progress=(static_cast<float>(currentFrameIndex+1) / transferCount) * 100;
                            std::stringstream ss;
                            ss << std::fixed << std::setprecision(4) << progress;
                            errmsg = module + "通过[0x60 0x83]传输数据成功,当前进度是:" + ss.str() + "%";
                            std::cout << errmsg.c_str() << std::endl;
                            SCREEN_LOGD("%s\n", errmsg.c_str());
                            //准备下一包
                            currentFrameIndex++;
                            seqNo = (seqNo + 1) % 256;
                            // 跳出所有重试循环，继续处理下一帧
                            frameTransferred = true;
                            break; 
                        } else if(transferDataResp[3] == 0x01) { //NOT_OK
                            SCREEN_LOGE("传输之后接收屏幕响应的结果:NOT_OK,重试次数: %d,当前帧索引: %d\n", writeRetryCount, currentFrameIndex);
                            return false; // 返回0表示失败
                        } else if(transferDataResp[3] == 0x02) {//BUSY 
                            readRetryCount++;
                            if (readRetryCount >= MAX_READ_RETRIES) {
                                SCREEN_LOGE("重试读操作 %d 次后仍然 BUSY,当前帧索引: %d\n", MAX_READ_RETRIES, currentFrameIndex);
                                return false;
                            }
                            std::this_thread::sleep_for(std::chrono::milliseconds(100));
                            continue;
                        } else if(transferDataResp[3] == 0x03) { //RETRANSMISSION
                            writeRetryCount++;
                            if (writeRetryCount >= MAX_WRITE_RETRIES) {
                                SCREEN_LOGE("重试写操作 %d 次后仍然需要重传，当前帧索引: %d\n", MAX_WRITE_RETRIES, currentFrameIndex);
                                return false;
                            }
                            std::this_thread::sleep_for(std::chrono::milliseconds(100));
                            break;
                        }
                    } else {
                        readRetryCount++;
                        if (readRetryCount >= MAX_READ_RETRIES) {
                            SCREEN_LOGE("重试读操作 %d 次后读取仍然失败，当前帧索引: %d\n", MAX_READ_RETRIES, currentFrameIndex);
                            return false;
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        continue;
                    }
                }
                if (frameTransferred) {
                    break; // 当前帧传输成功，处理下一帧
                }
                
            } else {
                SCREEN_LOGD("通过TransferDataCommand(0x60, 0x83)发送当前mcu升级包数据给display失败\n");
                writeRetryCount++;
                if (writeRetryCount >= MAX_WRITE_RETRIES) {
                    SCREEN_LOGE("重试写操作 %d 次后仍然失败，当前帧索引: %d\n", MAX_WRITE_RETRIES, currentFrameIndex);
                    return false;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }
    outChecksum=static_cast<uint16_t>(frameCheckSum & 0xFFFF);
    return true; 
}

//step5
//已进入bootloader 模式
//主机通过Request Download Command (0x60, 0x82)请求display下载升级包(给显示器MCU/TCON/TOUCH 发送数据包)
bool UpgradeProcess::requestDownload(const i2cDriverInfo &info,const uint8_t screenFirmwareAddr[4], const uint8_t screenFirmwareLen[4])
{
    std::vector<uint8_t> requestDownloadReqParam = {0x60,0x0B,0x00,0x82};
    for (int i = Firmware_ADDR_LEN - 1; i >= 0; --i) {
        requestDownloadReqParam.emplace_back(screenFirmwareAddr[i]);
    }
    for (int j = Firmware_SIZE_LEN - 1; j >= 0; --j) {
        requestDownloadReqParam.emplace_back(screenFirmwareLen[j]);
    }

    uint8_t requestDownloadRespData[BOOT_MODE_SIZE]={0x0};
    uint8_t crcBytes[2];
    uint16_t crc = CRC16XMODEM::calculate(requestDownloadReqParam.data() + 1, requestDownloadReqParam.size() - 1);    
    crcBytes[0] = crc & 0xFF;
    crcBytes[1] = (crc >> 8) & 0xFF;
    requestDownloadReqParam.emplace_back(crcBytes[0]);
    requestDownloadReqParam.emplace_back(crcBytes[1]);

    int writeRetryCount = 0;
    int readRetryCount = 0;

    while (writeRetryCount < MAX_WRITE_RETRIES) {
        if(i2cProcessHandler::i2cWriteHandler(info, requestDownloadReqParam.data(), requestDownloadReqParam.size())) {
            SCREEN_LOGD("i2cWrite i2caddr:%2x regaddr:%2x succeeded\n", info.i2cDevAddr);
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            readRetryCount = 0;
            while (readRetryCount < MAX_READ_RETRIES) {
                if(i2cProcessHandler::i2cReadHandler(info, 0x61, sizeof(uint8_t), requestDownloadRespData, sizeof(requestDownloadRespData))) {
                    if(requestDownloadRespData[3] == 0x00) { // OK
                        SCREEN_LOGD("主机通过 [0x60 0x82] 通知 display 开始传输MCU数据成功\n")
                        return true;
                    } else if(requestDownloadRespData[3] == 0x01) { // NO_OK
                        SCREEN_LOGE("主机通过 [0x60 0x60] 通知 display 开始传MCU输数据失败\n");
                        return false;
                    } else if(requestDownloadRespData[3] == 0x02) { // BUSY 
                        readRetryCount++;
                        if (readRetryCount >= MAX_READ_RETRIES) {
                            SCREEN_LOGE("重试读操作 %d 次后仍然 BUSY\n", MAX_READ_RETRIES);
                            return false;
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        continue;
                    } else if(requestDownloadRespData[3] == 0x03) { // RETRANSMISSION
                        writeRetryCount++;
                        if (writeRetryCount >= MAX_WRITE_RETRIES) {
                            SCREEN_LOGE("重试写操作 %d 次后仍然需要重传\n", MAX_WRITE_RETRIES);
                            return false;
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        break;
                    }
                } else {
                    readRetryCount++;
                    if (readRetryCount >= MAX_READ_RETRIES) {
                        SCREEN_LOGE("重试读操作 %d 次后读取仍然失败\n", MAX_READ_RETRIES);
                        return false;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    continue;
                }
            }
            
        } else {
            SCREEN_LOGD("主机通过 [0x60 0x60] 通知 display 开始传输MCU数据失败\n");
            writeRetryCount++;
            if (writeRetryCount >= MAX_WRITE_RETRIES) {
                SCREEN_LOGE("重试写操作 %d 次后仍然失败\n", MAX_WRITE_RETRIES);
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    return false;
}

//step4
//主机通过Erase Range Command (0x60, 0x81)通知display擦除待升级模块的数据,MCU ,TCON ,TOUCH
//此时已进入bootloader模式
bool UpgradeProcess::eraseCommand(const i2cDriverInfo &info,const uint8_t screenFirmwareAddr[4], const uint8_t screenFirmwareLen[4])
{ 
    //从设备屏幕中的 MCU模块地址 0x0000 touch地址 0x8000 tcon地址 0x10000 
    //0x60 :register addr ; 0x0B,0x00 length(11),0x81以后就是excel中的数据
    std::vector<uint8_t> eraseRangeReqParam = {0x60, 0x0B,0x00,0x81};
    uint8_t  eraseRespData[6]={0x0};
    //eraseRangeReqParamTemp.reserve(100)
    for (int i = Firmware_ADDR_LEN - 1; i >= 0; --i) {
        eraseRangeReqParam.emplace_back(screenFirmwareAddr[i]);
    }
    for (int j = Firmware_SIZE_LEN - 1; j >= 0; --j) {
        eraseRangeReqParam.emplace_back(screenFirmwareLen[j]);
    }

    uint8_t crcBytes[2];
    uint16_t crc = CRC16XMODEM::calculate(eraseRangeReqParam.data() + 1, eraseRangeReqParam.size() - 1);
    crcBytes[0] = crc & 0xFF;
    crcBytes[1] = (crc >> 8) & 0xFF;
    //eraseRangeReqParam.reserve(eraseRangeReqParamTemp.size() + 2);
    eraseRangeReqParam.emplace_back(static_cast<uint8_t>(crcBytes[0]));
    eraseRangeReqParam.emplace_back(static_cast<uint8_t>(crcBytes[1]));

    uint8_t writeRetryCount = 0;
    uint8_t readRetryCount = 0;

    while (writeRetryCount < MAX_WRITE_RETRIES) {
        if (i2cProcessHandler::i2cWriteHandler(info, eraseRangeReqParam.data(), eraseRangeReqParam.size())) {
            SCREEN_LOGD("i2cWrite i2caddr:%2x regaddr:%2x succeeded\n", info.i2cDevAddr);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            readRetryCount = 0;
            while (readRetryCount < MAX_READ_RETRIES) {
                if (i2cProcessHandler::i2cReadHandler(info, 0x61, sizeof(uint8_t), eraseRespData, sizeof(eraseRespData))) {
                    if (eraseRespData[3] == 0x00) { // OK
                        return true;
                    } else if (eraseRespData[3] == 0x01) { // NO_OK
                        SCREEN_LOGE("擦除操作失败,设备返回NO_OK");
                        return false;
                    } else if (eraseRespData[3] == 0x02) { // BUSY 
                        readRetryCount++;
                        SCREEN_LOGD("设备忙，正在重试... (%d/%d)", readRetryCount, MAX_READ_RETRIES);
                        if (readRetryCount >= MAX_READ_RETRIES) {
                            SCREEN_LOGE("重试读操作 %d 次后仍然 BUSY\n", MAX_READ_RETRIES);
                            return false;
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        continue;
                    } else if (eraseRespData[3] == 0x03) { // RETRANSMISSION
                        writeRetryCount++;
                        SCREEN_LOGD("需要重传，正在重试写操作... (%d/%d)", writeRetryCount, MAX_WRITE_RETRIES);
                        if (writeRetryCount >= MAX_WRITE_RETRIES) {
                            SCREEN_LOGE("重试写操作 %d 次后仍然需要重传\n", MAX_WRITE_RETRIES);
                            return false;
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        break; 
                    }
                } else {
                    return false;
                }
            }
            
            if (readRetryCount < MAX_READ_RETRIES) {
                return true;
            }
            
        } else {
            SCREEN_LOGE("主机通过 [0x60 0x81] 通知display擦除指定范围失败\n");
            writeRetryCount++;
            if (writeRetryCount >= MAX_WRITE_RETRIES) {
                SCREEN_LOGE("重试写操作 %d 次后仍然失败\n", MAX_WRITE_RETRIES);
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    return false; 
}

//step3
//主机发Read Session(0x40 0x22 0xFEF7)给 display，读取当前session
int UpgradeProcess::readSession(const i2cDriverInfo &info)
{
    uint8_t sessionReqParam[] = {0x40,0x03,0x22,0xFE,0xF7}; 
    uint8_t sessionRespData[5]={0x0};
    if (i2cProcessHandler::i2cWriteHandler(info,sessionReqParam,sizeof(sessionReqParam)/sizeof(uint8_t))) {
        SCREEN_LOGD("i2cWrite i2caddr:%2x succeeded\n", info.i2cDevAddr);
    } else {
        SCREEN_LOGE("主机通过 [0x40 0x22 0xFEF7] 通知 display 读取当前session失败\n");
        return -1;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    if (i2cProcessHandler::i2cReadHandler(info,0x41,sizeof(uint8_t),sessionRespData,sizeof(sessionRespData))) {
        SCREEN_LOGD("i2c read succeeded,sessionRespData[0]=%#x,sessionRespData[1]=%#x\n",sessionRespData[0],sessionRespData[1]);   
        return sessionRespData[4];     
    } else {
        SCREEN_LOGE("主机通过 [0x40 0x22 0xFEF7] 通知 display 读取当前session失败\n");
        return -2;
    }
}

//主机通过Programming Session(0x40 0x10) 命令通知对应的display切到bootloader 刷机模式
bool UpgradeProcess::programmingSession(const i2cDriverInfo &info)
{
    //当前在APP模式,还没进入bootloader,所以参数长度是1
    //主机通过Programming Session(0x40 0x10) 命令通知对应的display切到bootloader 刷机模式
    uint8_t programSessionParam[] = {0x40,0x02,0x10,0x02};
    uint8_t programSessionRespData[4]={0x0};//正反馈和负反馈的返回的字节数不一致
    if (i2cProcessHandler::i2cWriteHandler(info,programSessionParam,sizeof(programSessionParam))) {
        SCREEN_LOGD("i2cWrite i2caddr:%2x  succeeded\n",info.i2cDevAddr);
    } else {
        SCREEN_LOGD("主机通过Programming Session(0x40 0x02 0x10 0x02) 命令通知对应的display切到bootloader 刷机模式失败\n");
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    std::string errmsg{};
    if (i2cProcessHandler::i2cReadHandler(info,0x41,sizeof(uint8_t),programSessionRespData,sizeof(programSessionRespData))){
        SCREEN_LOGD("i2c read succeeded,programSessionRespData[0]=%#x,programSessionRespData[1]=%#x\n",programSessionRespData[0],programSessionRespData[1]);
        if(programSessionRespData[1]==0x7F){
            std::string errmsg = "主机通过Programming Session [0x40 0x10] read,display 反馈0x7F,升级失败\n";
            SCREEN_LOGE("主机通过Programming Session [0x40 0x10] read,display 反馈0x7F,升级失败\n");
            return false;
        }
    } else {
        errmsg = "主机通过Programming Session(0x40 0x10) 命令通知对应的display切到bootloader 刷机模式失败\n";
        SCREEN_LOGE("主机通过Programming Session(0x40 0x10) 命令通知对应的display切到bootloader 刷机模式失败\n");
        return false;
    }
    return true;
}

//step2
//主机发Read PN命令(0x40 0x22 0xFEF5)给 display，请求读取显示屏的零件号
bool UpgradeProcess::readPN(const i2cDriverInfo &info)
{
    uint8_t pnReqParam[] = {0x40,0x03,0x22,0xFE,0xF5}; 
    uint8_t  pnRespData[24]={0x0};
    std::string errmsg{};
    if (i2cProcessHandler::i2cWriteHandler(info,pnReqParam,sizeof(pnReqParam)/sizeof(uint8_t))) {
        SCREEN_LOGD("i2cWrite i2caddr:%2x regaddr:%2x succeeded\n",info.i2cDevAddr);
    } else {
        errmsg = "主机通过(0x40 0x22 0xFEF5)通知读取显示屏的零件号失败\n";
        SCREEN_LOGE("%s\n", errmsg.c_str());
        return false;
    }

    //50ms后主机通过IIC协议0x41查询display的PN
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    if (!i2cProcessHandler::i2cReadHandler(info,0x41,sizeof(uint8_t),pnRespData,sizeof(pnRespData))) {
        errmsg = "主机通过0x41命令读取display的PN响应失败";
        SCREEN_LOGE("%s\n", errmsg.c_str());
        return false;
    } else {
        if (pnRespData[1]==0x7F) {
                errmsg = "主机通过Programming Session [0x40 0x10] read, display反馈0x7F,升级失败";
                SCREEN_LOGE("%s\n", errmsg.c_str());
                return false;
        }else{
                char displayPN[20]={0};
                memcpy(displayPN, &pnRespData[4], 20);
                if (memcmp(displayPN, binFileDecode_->getHeader().productPN, 20) != 0) {
                    errmsg = "显示屏读取的PN与bin文件解析的PN不匹配\n";
                    SCREEN_LOGE("%s\n", errmsg.c_str());
                    return false;
                } 
                return true;
        }
    }
    return false;
}

//step1
//主机通过(0x40 0x22 0xFEF3)通知读取显示屏的I2C通信协议版本
bool UpgradeProcess::readI2CVersion(const i2cDriverInfo &info)
{
    uint8_t i2cVerReqParam[] = {0x40,0x03,0x22,0xFE,0xF3};  
    uint8_t i2cVerRespData[14] = {0x0};
    std::string errmsg{};
    if (i2cProcessHandler::i2cWriteHandler(info,i2cVerReqParam,sizeof(i2cVerReqParam)/sizeof(uint8_t))) {
        std::cout << "i2cWrite i2caddr:0x" << std::hex << (int)info.i2cDevAddr << " ,regAddr:0x" << std::hex << (int)i2cVerReqParam[0] << std::endl;
        SCREEN_LOGD("i2cWrite i2caddr:%#x ,regAddr:%#x succeeded\n",info.i2cDevAddr,i2cVerReqParam[0]);
    } else {
        std::cout << "i2cWrite i2caddr:0x" << std::hex << (int)info.i2cDevAddr << " ,regAddr:0x" << std::hex << (int)i2cVerReqParam[0] << std::endl;
        errmsg = "主机通过(0x40 0x22 0xFEF3)通知读取显示屏的I2C通信协议版本失败\n";
        SCREEN_LOGE("%s\n", errmsg.c_str());
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    if (i2cProcessHandler::i2cReadHandler(info,0x41,sizeof(uint8_t),i2cVerRespData,sizeof(i2cVerRespData))) {
            SCREEN_LOGD("i2cread succeeded,i2cVerRespData[0]=%#x,i2cVerRespData[1]=%#x\n",i2cVerRespData[0],i2cVerRespData[1]);
            std::string i2cVersion;
            for (int i = 4; i < 14; i++) {
                i2cVersion += i2cVerRespData[i];
            }
            SCREEN_LOGD("I2C协议版本: %s", i2cVersion.c_str());
            std::cout << "I2C协议版本: " << i2cVersion.c_str() << std::endl;
            return true;
    } else {
            errmsg = "i2c version read failed\n";
            SCREEN_LOGE("%s\n", errmsg.c_str());
            return false;
    }
}
