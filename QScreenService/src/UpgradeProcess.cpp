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

constexpr uint8_t BOOT_MODE_RESP_SIZE = 6;
constexpr uint8_t TRANSFER_CHUNK = 128;
constexpr uint8_t MAX_WRITE_RETRIES = 3;
constexpr uint8_t MAX_READ_RETRIES = 20;
constexpr uint8_t Firmware_ADDR_LEN = 4;
constexpr uint8_t Firmware_SIZE_LEN = 4;
constexpr uint8_t CRC16_LEN = 2;
constexpr uint8_t TRANSFER_HEAD_LEN= 4;

constexpr uint8_t BOOT_MODE_REGADDR = 0x60;
constexpr uint8_t BOOT_MODE_RESP_REGADDR = 0x61;

constexpr uint8_t SERVICE_ID_ERASE_RANGE_COMMAND = 0x81;
constexpr uint8_t SERVICE_ID_REQUEST_DOWNLOAD_COMMAND = 0x82;
constexpr uint8_t SERVICE_ID_TRANSFER_DATA_COMMAND = 0x83;
constexpr uint8_t SERVICE_ID_CHECKSUM_COMMAND = 0x84;
constexpr uint8_t SERVICE_ID_RESET_COMMAND = 0x85;
constexpr uint8_t SERVICE_ID_SLAVE_RESPONS = 0x40;

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
    std::cout << "upgrade方法 [I2C初始化] 开始初始化I2C,设备路径: " << i2cInfo.i2cDevPath << std::endl;
    int i2cFd = i2cProcessHandler::i2cInitHandler(i2cInfo.i2cDevPath);
    i2cInfo.i2cFd = i2cFd;
    if(i2cFd < 0){
        errmsg = " UpgradeProcess::upgrade ,i2cProcessHandler::i2cInitHandler ERR : i2c init fd failed \n";
        std::cout<<errmsg.c_str()<<std::endl;
        ::write(replyFd, errmsg.c_str(), errmsg.size());
        return false;
    }
    std::cout << "[I2C初始化成功] I2C文件描述符: " << i2cFd << std::endl;
    std::cout<<"binFile name:" <<binFile.c_str()<<std::endl;
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
            {0x00, 0x01, 0x00, 0x00},  //TCON物理地址
            0x32,
            binHeader.blocks[1].startAddrDecValue,
            binHeader.blocks[1].lengthDecValue,
            true
        });    
    } else if (binFileDecode_->getBlockTouchValid()) {
        modules.emplace_back(FirmwareInfo{
            {0x00, 0x00, 0x80, 0x00}, //Touch物理地址
            0x33,
            binHeader.blocks[0].startAddrDecValue,
            binHeader.blocks[0].lengthDecValue,
            true
        });    
    }
    
    // if (modules.empty()) {
    //     errmsg = "没有需要升级的模块\n";
    //     std::cout << errmsg << std::endl;
    //     ::write(replyFd, errmsg.c_str(), errmsg.size());
    //     i2cProcessHandler::i2cDeinitHandler(i2cFd);
    //     return false;
    // }

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
    std::cout << "[模块升级开始] upgradeModule 模块: " << moduleName << std::endl;
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
    //std::cout << "进入checkSum这个方法之前,downloadCRC: 0x" << std::hex << downloadCRC << std::dec << std::endl;
    // 校验和
    if(!checksum(i2cInfo, module.firmwareAddr, lengthArray, downloadCRC)){
        std::string errmsg = "Checksum " + moduleName + " failed\n";
        ::write(replyFd, errmsg.c_str(), errmsg.size());
        return false;
    }

    std::cout << "[模块升级完成] 模块: " << moduleName << std::endl;
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
    std::cout << "assignI2cInfo方法,ScreenType:" << static_cast<int>(screenType) 
              << ",i2cDevPath: " << i2cInfo.i2cDevPath
              << ",i2cDevAddr: 0x" << std::hex << static_cast<int>(i2cInfo.i2cDevAddr) << std::dec << std::endl;
}

//step8
//主机发送Reset Command(0x60, 0x85) 给display
bool UpgradeProcess::reset(const i2cDriverInfo &info)
{
    std::cout << "[RESET] 开始发送Reset命令" << std::endl;
    std::vector<uint8_t> resetReqParam = {BOOT_MODE_REGADDR, 0x04, 0x00, SERVICE_ID_RESET_COMMAND, 0x01};
    uint16_t resetCRC = CRC16XMODEM::calculate(resetReqParam.data() + 1, resetReqParam.size() - 1);
    resetReqParam.emplace_back(resetCRC & 0xFF);
    resetReqParam.emplace_back((resetCRC >> 8) & 0xFF);

    uint8_t resetRespData[BOOT_MODE_RESP_SIZE] = {0x00};
     // 打印要发送的Reset命令
    std::cout << "[RESET-I2C发送] 准备发送Reset命令:0x";
    for (size_t i = 0; i < resetReqParam.size(); i++) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') 
                  << static_cast<int>(resetReqParam[i]);
    }
    std::cout << std::dec << std::endl;
    bool result = serviceIdCommandWithRetry(
        info,
        resetReqParam,
        BOOT_MODE_RESP_REGADDR,              
        resetRespData,
        sizeof(resetRespData),
        "Reset Command"
    );
    std::cout<<"Reset Command 结果: "<<result<<std::endl;
    return result; 
}

//step7
//主机通过Checksum Command(0x60, 0x84)发送主机计算的该block的checksum值 给display
bool UpgradeProcess::checksum(const i2cDriverInfo &info,const uint8_t screenFirmwareAddr[4], const uint8_t screenFirmwareLen[4],uint16_t downfileCRC)
{
    std::cout << "[CHECKSUM] 开始发送Checksum命令" << std::endl;
    uint8_t  checksumRespData[BOOT_MODE_RESP_SIZE];

    std::cout << "checksum方法,downfileCRC: 0x" << std::hex << downfileCRC << std::dec << std::endl;

    std::vector<uint8_t> checksumReqParam = {BOOT_MODE_REGADDR, 0x0D,0x00,SERVICE_ID_CHECKSUM_COMMAND};

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

     // 打印要发送的Checksum命令
    std::cout << "[CHECKSUM-I2C发送] 准备发送Checksum命令:0x";
    for (size_t i = 0; i < checksumReqParam.size(); i++) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') 
                  << static_cast<int>(checksumReqParam[i]) << " ";
    }
    std::cout << std::dec << std::endl;
    bool result = serviceIdCommandWithRetry(
        info,
        checksumReqParam,
        BOOT_MODE_RESP_REGADDR,              
        checksumRespData,
        sizeof(checksumRespData),
        "ChecksumCommand"
    );
    std::cout<<"Checksum Command 结果: "<<result<<std::endl;
    return result; 
}

std::vector<uint8_t> UpgradeProcess::buildTransferPacket(const std::vector<uint8_t>& firmwareData, uint32_t seqNo) {
    std::vector<uint8_t> packet;
    uint16_t transferDataLen = firmwareData.size() + TRANSFER_HEAD_LEN;
    packet.reserve(1 + TRANSFER_HEAD_LEN + firmwareData.size() + CRC16_LEN);

    packet.emplace_back(BOOT_MODE_REGADDR);
    packet.emplace_back(transferDataLen & 0xFF);
    packet.emplace_back((transferDataLen >> 8) & 0xFF);
    packet.emplace_back(SERVICE_ID_TRANSFER_DATA_COMMAND);
    packet.emplace_back((seqNo % 0xFF)+1);
    packet.insert(packet.end(), firmwareData.begin(), firmwareData.end());

    uint16_t crc = CRC16XMODEM::calculate(packet.data() + 1, transferDataLen);
    packet.push_back(static_cast<uint8_t>(crc & 0xFF));
    packet.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));

    return packet;
}
//step6
//主机通过Transfer Data Command(0x60, 0x83) 发送当前block升级包数据给display（每次发128Byte数据）
bool UpgradeProcess::transferData(i2cDriverInfo &info,uint32_t addrInBinFile,uint32_t screenFirmwareDecLen,const std::string &module,uint16_t &outChecksum)
{
    std::cout << "[TRANSFER DATA] 开始传输数据，模块: " << module << std::endl;
    uint64_t transferCount = screenFirmwareDecLen % TRANSFER_CHUNK==0?screenFirmwareDecLen/TRANSFER_CHUNK:screenFirmwareDecLen/TRANSFER_CHUNK +1;

    std::cout << "首地址:addrInBinFile:0x" <<std::hex << addrInBinFile << "总长度:"<< std::dec << screenFirmwareDecLen<<std::endl;
    std::cout <<"首地址+总长度:0x"<< std::hex<<(addrInBinFile + screenFirmwareDecLen) <<std::endl;
    uint32_t currentFrameIndex = 0;
    //sequence frame:1-255，
    std::vector<uint8_t> transferDataReqParam(1 + TRANSFER_HEAD_LEN + TRANSFER_CHUNK + CRC16_LEN);
    uint8_t transferDataResp[BOOT_MODE_RESP_SIZE]={0x00};
    std::vector<uint8_t> firmwareData(TRANSFER_CHUNK);
    
    std::vector<uint8_t> frameCRC(128);
    uint64_t frameCheckSum = 0;

    int writeRetryCount = 0;
    int readRetryCount = 0;
    uint32_t currentOffset = 0;
    std::string errmsg;

    while(currentFrameIndex < transferCount) {

        uint32_t bytesToRead = TRANSFER_CHUNK;
        if (currentFrameIndex == transferCount - 1 && screenFirmwareDecLen % TRANSFER_CHUNK != 0) {
            bytesToRead = screenFirmwareDecLen % TRANSFER_CHUNK;
            std::cout << "transferData 这是最后一包,数据长度: " << bytesToRead << " 字节" << std::endl;
        }

        firmwareData.clear();
        if (!binFileDecode_->readBlockData(addrInBinFile + currentOffset, 
                                           bytesToRead, 
                                           firmwareData)) {
            SCREEN_LOGE("读取固件%s数据失败,地址: 0x%x, 长度: %u", 
                       module.c_str(),
                       addrInBinFile + currentOffset, 
                       bytesToRead);
            return 0;
        }
        
        //更新偏移量
        currentOffset += bytesToRead;
        std::cout<<"currentOffset is "<< currentOffset << (currentOffset/128) <<std::endl;
        
        transferDataReqParam.clear();
        transferDataReqParam = buildTransferPacket(firmwareData, currentFrameIndex);
        std::cout <<"buildTransferPacket的写方法入参,显示16个字节:"<<std::endl;
        for (int i = 0; i < 16; i++) { // 只显示前16个字节，避免输出过多  
            std::cout <<std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(firmwareData[i]) ;
        }
        std::cout<<std::dec<<std::endl;
          
        writeRetryCount = 0;
        bool frameTransferred = false;
        while (writeRetryCount < MAX_WRITE_RETRIES) {

            if(i2cProcessHandler::i2cWriteHandler(info, transferDataReqParam.data(), transferDataReqParam.size())) {
                //SCREEN_LOGD("i2cWrite i2caddr:%2x regaddr:%2x succeeded\n", info.i2cDevAddr);
                // 添加I2C写入成功的调试输出，包含设备路径和设备地址
                std::cout << "[TRANSFER DATA-I2C 写屏幕成功] i2cDevPath:" << info.i2cDevPath 
                          << ",i2cDevAddr: 0x" << std::hex << static_cast<int>(info.i2cDevAddr) 
                          << std::dec << std::endl;             
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                readRetryCount = 0;
               
                while (readRetryCount < MAX_READ_RETRIES) {
                    if(i2cProcessHandler::i2cReadHandler(info, BOOT_MODE_RESP_REGADDR, sizeof(uint8_t), transferDataResp, sizeof(transferDataResp))) {
                        std::cout << "[TRANSFER DATA-I2C 读屏幕成功] 响应数据: ";
                        for (int i = 0; i < sizeof(transferDataResp); i++) {
                            std::cout << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(transferDataResp[i]) << " ";
                        }
                        std::cout << std::dec << std::endl;

                        uint16_t calc_crc = CRC16XMODEM::calculate(transferDataResp, 4);
                        uint16_t recv_crc = (transferDataResp[5] << 8) | transferDataResp[4];
                        if (calc_crc != recv_crc) {
                            //SCREEN_LOGE("响应CRC验证失败,帧: %u", currentFrameIndex);
                            std::cout << "calc_crc: 0x" << std::hex << calc_crc << ", recv_crc: 0x" << recv_crc << std::dec << std::endl;
                            std::cout<<"响应CRC验证失败,帧: "<<currentFrameIndex<<std::endl;
                           return false;
                        }

                        if(transferDataResp[3] == 0x00) { //OK
                            float progress=(static_cast<float>(currentFrameIndex+1) / transferCount) * 100;
                            std::stringstream ss;
                            ss << std::fixed << std::setprecision(4) << progress;
                            errmsg = module + "通过[0x60 0x83]传输数据成功,当前进度是:" + ss.str() + "%";
                            std::cout << errmsg.c_str() << std::endl;
                            //SCREEN_LOGD("%s\n", errmsg.c_str());
                            //校验和
                            frameCRC.clear();
                            //frameCRC.emplace_back((bytesToRead + 3) & 0xFF);
                            //frameCRC.emplace_back(((bytesToRead + 3) >> 8) & 0xFF);
                            //frameCRC.emplace_back(SERVICE_ID_TRANSFER_DATA_COMMAND);
                            //frameCRC.insert(frameCRC.end(),firmwareData.begin(),firmwareData.end());
                            //frameCRC.insert(firmwareData.begin(),firmwareData.end());
                            frameCRC = firmwareData;
                            std::cout<<"frameCRC.size()="<<frameCRC.size()<<std::endl;
                            std::cout<<"bytesToRead="<<bytesToRead<<std::endl;
                            uint16_t framecrc = CRC16XMODEM::calculate(frameCRC.data(), 0x80);
                            frameCheckSum += framecrc;
                            //准备下一包
                            currentFrameIndex++;
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
                //SCREEN_LOGD("通过TransferDataCommand(0x60, 0x83)发送当前mcu升级包数据给display失败\n");
                writeRetryCount++;
                if (writeRetryCount >= MAX_WRITE_RETRIES) {
                    SCREEN_LOGE("重试写操作 %d 次后仍然失败，当前帧索引: %d\n", MAX_WRITE_RETRIES, currentFrameIndex);
                    return false;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }
    std::cout <<"总包数:"<< transferCount <<std::endl;
    std::cout << "当前帧索引:" << currentFrameIndex << std::endl;

    std::cout << "帧累加和:frameCheckSum "<< static_cast<uint64_t>(frameCheckSum) << std::endl;
    outChecksum=static_cast<uint16_t>(frameCheckSum & 0xFFFF);
    std::cout << "[TRANSFER DATA完成] 模块: " << module << ", 最终Checksum: 0x" 
    << std::hex << outChecksum << std::dec << std::endl;
    return true; 
}

//step5
//已进入bootloader 模式
//主机通过Request Download Command (0x60, 0x82)请求display下载升级包(给显示器MCU/TCON/TOUCH 发送数据包)
bool UpgradeProcess::requestDownload(const i2cDriverInfo &info,const uint8_t screenFirmwareAddr[4], const uint8_t screenFirmwareLen[4])
{
    std::cout << "[REQUEST DOWNLOAD] 开始请求下载" << std::endl;
    std::vector<uint8_t> requestDownloadReqParam = {BOOT_MODE_REGADDR,0x0B,0x00,SERVICE_ID_REQUEST_DOWNLOAD_COMMAND};
    for (int i = Firmware_ADDR_LEN - 1; i >= 0; --i) {
        requestDownloadReqParam.emplace_back(screenFirmwareAddr[i]);
    }
    for (int j = Firmware_SIZE_LEN - 1; j >= 0; --j) {
        requestDownloadReqParam.emplace_back(screenFirmwareLen[j]);
    }

    uint8_t requestDownloadRespData[BOOT_MODE_RESP_SIZE]={0x0};
    uint8_t crcBytes[2];
    uint16_t crc = CRC16XMODEM::calculate(requestDownloadReqParam.data() + 1, requestDownloadReqParam.size() - 1);    
    crcBytes[0] = crc & 0xFF;
    crcBytes[1] = (crc >> 8) & 0xFF;
    requestDownloadReqParam.emplace_back(crcBytes[0]);
    requestDownloadReqParam.emplace_back(crcBytes[1]);
     // 打印要发送的Request Download命令
    //std::cout << "[REQUEST DOWNLOAD-I2C发送] 准备发送Request Download命令:0x";
    // for (size_t i = 0; i < requestDownloadReqParam.size(); i++) {
    //     std::cout << std::hex << std::setw(2) << std::setfill('0') 
    //               << static_cast<int>(requestDownloadReqParam[i]) << " ";
    // }
    // std::cout << std::dec << std::endl;

    bool result = serviceIdCommandWithRetry(
        info,
        requestDownloadReqParam,
        BOOT_MODE_RESP_REGADDR,              
        requestDownloadRespData,
        sizeof(requestDownloadRespData),
        "RequestDownloadCommand"
    );
    std::cout<<"[REQUEST DOWNLOAD] 结果: "<<result<<std::endl;
    return result; 
}

bool UpgradeProcess::serviceIdCommandWithRetry(const i2cDriverInfo& info, 
                                            std::vector<uint8_t>& writeData, 
                                            uint8_t readReg, uint8_t* readData, 
                                            size_t readDataSize,
                                            const std::string& commandName) 
{
    int writeRetryCount = 0, readRetryCount = 0; 
    while (writeRetryCount < MAX_WRITE_RETRIES) {
        if (i2cProcessHandler::i2cWriteHandler(info, writeData.data(), writeData.size())) {
            std::cout << "[" << commandName << "-I2C写操作成功] 设备路径: " << info.i2cDevPath 
                      << ", 设备地址: 0x" << std::hex << static_cast<int>(info.i2cDevAddr) << std::dec << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));    
            while (readRetryCount < MAX_READ_RETRIES) {
                if (i2cProcessHandler::i2cReadHandler(info, readReg, sizeof(uint8_t), readData, readDataSize)) {
                    std::cout << "[" << commandName << "-I2C读操作成功] 读取到的数据:0x";
                    for (size_t i = 0; i < readDataSize; i++) {
                        std::cout << std::hex << std::setw(2) << std::setfill('0') 
                                  << static_cast<int>(readData[i]);
                    }
                    std::cout << std::dec << std::endl;

                    uint16_t calc_crc = CRC16XMODEM::calculate(readData, 4);
                    uint16_t recv_crc = (readData[5] << 8) | readData[4];
                        
                    if (calc_crc != recv_crc) {
                        std::cout << "calc_crc: 0x" << std::hex << calc_crc << ", recv_crc: 0x" << recv_crc << std::dec << std::endl;
                        std::cout<< commandName <<"响应CRC验证失败,"<<std::endl;
                       return false;
                    }

                    if (readDataSize < 5) {
                        std::cout << "[" << commandName << "] 响应数据长度不足" << std::endl;
                        return false;
                    }
                    std::cout << commandName << "命令收到响应码: 0x" << std::hex << std::setw(2) 
                              << std::setfill('0') << static_cast<int>(readData[3]) << std::dec << std::endl;

                    if (readData[3] == 0x00) { //OK
                        std::cout << "[" << commandName << "] 操作成功" << std::endl;
                        return true;
                    } else if (readData[3] == 0x01) { //NOT_OK
                        std::cout << "[" << commandName << "] 操作失败" << std::endl;
                        return false;
                    } else if (readData[3] == 0x02) { //BUSY 
                        readRetryCount++;
                        std::cout << "[" << commandName << "] 设备忙(BUSY)，正在重试... (" 
                                  << readRetryCount << "/" << MAX_READ_RETRIES << ")" << std::endl;
                        
                        if (readRetryCount >= MAX_READ_RETRIES) {
                            std::cout << "[" << commandName << "] 重试读操作 " << MAX_READ_RETRIES 
                                      << " 次后仍然 BUSY" << std::endl;
                            return false;
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        continue;
                    } else if (readData[3] == 0x03) { //RETRANSMISSION
                        writeRetryCount++;
                        std::cout << "[" << commandName << "] 需要重传，正在重试写操作... (" 
                                  << writeRetryCount << "/" << MAX_WRITE_RETRIES << ")" << std::endl;
                        
                        if (writeRetryCount >= MAX_WRITE_RETRIES) {
                            std::cout << "[" << commandName << "] 重试写操作 " << MAX_WRITE_RETRIES 
                                      << " 次后仍然需要重传" << std::endl;
                            return false;
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        break; //跳出读循环，重试写操作
                    } else { // 未知响应码
                        std::cout << "[" << commandName << "] 未知响应码: 0x" << std::hex 
                                  << static_cast<int>(readData[4]) << std::dec << std::endl;
                        return false;
                    }
                } else {
                    readRetryCount++;
                    std::cout << "[" << commandName << "-I2C读操作失败] 尝试次数: (" 
                              << readRetryCount << "/" << MAX_READ_RETRIES << ")" << std::endl;
                    
                    if (readRetryCount >= MAX_READ_RETRIES) {
                        std::cout << "[" << commandName << "] 读操作重试耗尽" << std::endl;
                        return false;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            } 
            if(readRetryCount >= MAX_READ_RETRIES){
                // 读循环正常结束（非RETRANSMISSION情况）
                std::cout << "[" << commandName << "] 读重试耗尽" << std::endl;
                return false;    
            }                                   
        } else {
            writeRetryCount++;
            std::cout << "[" << commandName << "-I2C写操作失败] 尝试次数: (" 
                      << writeRetryCount << "/" << MAX_WRITE_RETRIES << ")" << std::endl;
            
            if (writeRetryCount >= MAX_WRITE_RETRIES) {
                std::cout << "[" << commandName << "] 写操作重试耗尽" << std::endl;
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }  
    std::cout << "[" << commandName << "] 写重试耗尽" << std::endl;
    return false;
}
//step4
//主机通过Erase Range Command (0x60, 0x81) 命令擦除display的对应固件存储区域
bool UpgradeProcess::eraseCommand(const i2cDriverInfo &info, const uint8_t screenFirmwareAddr[4], const uint8_t screenFirmwareLen[4])
{
    std::cout << "[ERASE] 开始擦除命令" << std::endl;
    std::vector<uint8_t> eraseReqParam = {BOOT_MODE_REGADDR, 0x0B, 0x00, SERVICE_ID_ERASE_RANGE_COMMAND};
    for (int i = Firmware_ADDR_LEN - 1; i >= 0; --i) {
        eraseReqParam.emplace_back(screenFirmwareAddr[i]);
        std::cout << std::hex << std::setw(2) << std::setfill('0') 
                  << static_cast<int>(screenFirmwareAddr[i]) << std::dec << std::endl;
    }

    for (int j = Firmware_SIZE_LEN - 1; j >= 0; --j) {
        eraseReqParam.emplace_back(screenFirmwareLen[j]);
    }

    uint16_t crc = CRC16XMODEM::calculate(eraseReqParam.data() + 1, eraseReqParam.size() - 1);
    eraseReqParam.emplace_back(crc & 0xFF);
    eraseReqParam.emplace_back((crc >> 8) & 0xFF);
    
    // 打印要发送的Erase命令
    // std::cout << "[ERASE-I2C发送] 准备发送Erase命令:0x";
    // for (size_t i = 0; i < eraseReqParam.size(); i++) {
    //     std::cout << std::hex << std::setw(2) << std::setfill('0') 
    //               << static_cast<int>(eraseReqParam[i]) << " ";
    // }
    // std::cout << std::dec << std::endl;

    uint8_t eraseRespData[BOOT_MODE_RESP_SIZE] = {0x00};
    bool result = serviceIdCommandWithRetry(
        info,
        eraseReqParam,
        BOOT_MODE_RESP_REGADDR,              
        eraseRespData,
        sizeof(eraseRespData),
        "EraseCommand" 
    );
    std::cout << "[ERASE] 结果: " <<  std::boolalpha <<result << std::endl;
    return result; 
}
//step3
//主机发Read Session(0x40 0x22 0xFEF7)给 display，读取当前session
int UpgradeProcess::readSession(const i2cDriverInfo &info)
{
    std::cout << "[READ SESSION] 开始读取Session" << std::endl;
    uint8_t sessionReqParam[] = {0x40,0x03,0x22,0xFE,0xF7}; 
    uint8_t sessionRespData[5]={0x0};

    // 打印要发送的Read Session命令
    // std::cout << "[READ SESSION-I2C发送] 准备发送Read Session命令:0x";
    // for (size_t i = 0; i < sizeof(sessionReqParam)/sizeof(uint8_t); i++) {
    //     std::cout << std::hex << std::setw(2) << std::setfill('0') 
    //               << static_cast<int>(sessionReqParam[i]);
    // }
    // std::cout << std::dec << std::endl;

    if (i2cProcessHandler::i2cWriteHandler(info,sessionReqParam,sizeof(sessionReqParam)/sizeof(uint8_t))) {
            std::cout << "[READ SESSION-I2C写操作成功]" << std::endl;
            //SCREEN_LOGD("i2cWrite i2caddr:%2x succeeded\n", info.i2cDevAddr);
    } else {
        //SCREEN_LOGE("主机通过 [0x40 0x22 0xFEF7] 通知 display 读取当前session失败\n");
        std::cout << "[READ SESSION-I2C写操作失败]" << std::endl;
        return -1;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    //std::cout << "[READ SESSION-I2C读操作] 开始读取响应" << std::endl;

    if (i2cProcessHandler::i2cReadHandler(info,0x41,sizeof(uint8_t),sessionRespData,sizeof(sessionRespData))) {
        // std::cout << "[READ SESSION-I2C读操作成功] 读取到的数据:0x";
        // for (size_t i = 0; i < sizeof(sessionRespData); i++) {
        //     std::cout << std::hex << std::setw(2) << std::setfill('0') 
        //               << static_cast<int>(sessionRespData[i]);
        // }
        // std::cout << std::dec << std::endl;
        int sessionValue = sessionRespData[4];
        std::cout << "[READ SESSION结果] sessionRespData[4]: 0x" << std::hex << sessionValue << std::dec << std::endl;
        //SCREEN_LOGD("i2c read succeeded,sessionRespData[0]=%#x,sessionRespData[1]=%#x\n",sessionRespData[0],sessionRespData[1]);   
        return sessionRespData[4];     
    } else {
        std::cout << "[READ SESSION-I2C读操作失败]" << std::endl;
        //SCREEN_LOGE("主机通过 [0x40 0x22 0xFEF7] 通知 display 读取当前session失败\n");
        return -2;
    }
}

//主机通过Programming Session(0x40 0x10) 命令通知对应的display切到bootloader 刷机模式
bool UpgradeProcess::programmingSession(const i2cDriverInfo &info)
{
    //当前在APP模式,还没进入bootloader,所以参数长度是1
    //主机通过Programming Session(0x40 0x10) 命令通知对应的display切到bootloader 刷机模式
    std::cout << "[PROGRAMMING SESSION] 开始切换到Bootloader模式" << std::endl;
    uint8_t programSessionParam[] = {0x40,0x02,0x10,0x02};
    uint8_t programSessionRespData[4]={0x0};//正反馈和负反馈的返回的字节数不一致

    // 打印要发送的Programming Session命令
    // std::cout << "[PROGRAMMING SESSION-I2C发送] 准备发送Programming Session命令:0x";
    // for (size_t i = 0; i < sizeof(programSessionParam); i++) {
    //     std::cout << std::hex << std::setw(2) << std::setfill('0') 
    //               << static_cast<int>(programSessionParam[i]) << " ";
    // }
    // std::cout << std::dec << std::endl;

    if (i2cProcessHandler::i2cWriteHandler(info,programSessionParam,sizeof(programSessionParam))) {
        //SCREEN_LOGD("i2cWrite i2caddr:%2x  succeeded\n",info.i2cDevAddr);
        std::cout << "[PROGRAMMING SESSION-I2C写操作成功]" << std::endl;
    } else {
        //SCREEN_LOGD("主机通过Programming Session(0x40 0x02 0x10 0x02) 命令通知对应的display切到bootloader 刷机模式失败\n");
         std::cout << "[PROGRAMMING SESSION-I2C写操作失败]" << std::endl;
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    //std::cout << "[PROGRAMMING SESSION-I2C读操作] 开始读取响应" << std::endl;
    std::string errmsg{};
    if (i2cProcessHandler::i2cReadHandler(info,0x41,sizeof(uint8_t),programSessionRespData,sizeof(programSessionRespData))){

        // std::cout << "[PROGRAMMING SESSION-I2C读操作成功] 读取到的数据:0x";
        // for (size_t i = 0; i < sizeof(programSessionRespData); i++) {
        //     std::cout << std::hex << std::setw(2) << std::setfill('0') 
        //               << static_cast<int>(programSessionRespData[i]) << " ";
        // }
        // std::cout << std::dec << std::endl;

        //SCREEN_LOGD("i2c read succeeded,programSessionRespData[0]=%#x,programSessionRespData[1]=%#x\n",programSessionRespData[0],programSessionRespData[1]);
        if(programSessionRespData[1]==0x7F){
            std::string errmsg = "主机通过Programming Session [0x40 0x10] read,display 反馈0x7F,升级失败\n";
             std::cout << "[PROGRAMMING SESSION失败] " << errmsg << std::endl;
            //SCREEN_LOGE("主机通过Programming Session [0x40 0x10] read,display 反馈0x7F,升级失败\n");
            return false;
        }
    } else {
        errmsg = "主机通过Programming Session(0x40 0x10) 命令通知对应的display切到bootloader 刷机模式失败\n";
        //SCREEN_LOGE("主机通过Programming Session(0x40 0x10) 命令通知对应的display切到bootloader 刷机模式失败\n");
        std::cout << "[PROGRAMMING SESSION失败] " << errmsg << std::endl;
        return false;
    }
    std::cout << "[PROGRAMMING SESSION成功] 已切换到Bootloader模式" << std::endl;
    return true;
}

//step2
//主机发Read PN命令(0x40 0x22 0xFEF5)给 display，请求读取显示屏的零件号
bool UpgradeProcess::readPN(const i2cDriverInfo &info)
{
    std::cout << "[READ PN] 开始读取零件号" << std::endl;
    uint8_t pnReqParam[] = {0x40,0x03,0x22,0xFE,0xF5}; 
    uint8_t  pnRespData[24]={0x0};
    std::string errmsg{};
     // 打印要发送的Read PN命令
    // std::cout << "[READ PN-I2C发送] 准备发送Read PN命令:0x";
    // for (size_t i = 0; i < sizeof(pnReqParam)/sizeof(uint8_t); i++) {
    //     std::cout << std::hex << std::setw(2) << std::setfill('0') 
    //               << static_cast<int>(pnReqParam[i]);
    // }
    // std::cout << std::dec << std::endl;

    if (i2cProcessHandler::i2cWriteHandler(info,pnReqParam,sizeof(pnReqParam)/sizeof(uint8_t))) {
        //SCREEN_LOGD("i2cWrite i2caddr:%2x regaddr:%2x succeeded\n",info.i2cDevAddr);
        std::cout << "[READ PN-I2C写操作成功]" << std::endl;
    } else {
        errmsg = "主机通过(0x40 0x22 0xFEF5)通知读取显示屏的零件号失败\n";
        //SCREEN_LOGE("%s\n", errmsg.c_str());
        std::cout << "[READ PN-I2C写操作失败] " << errmsg << std::endl;
        return false;
    }

    //50ms后主机通过IIC协议0x41查询display的PN
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    //std::cout << "[READ PN-I2C读操作] 开始读取响应" << std::endl;

    if (!i2cProcessHandler::i2cReadHandler(info,0x41,sizeof(uint8_t),pnRespData,sizeof(pnRespData))) {
        errmsg = "主机通过0x41命令读取display的PN响应失败";
        //SCREEN_LOGE("%s\n", errmsg.c_str());
        std::cout << "[READ PN-I2C读操作失败] " << errmsg << std::endl;
        return false;
    } else {
        // std::cout << "[READ PN-I2C读操作成功] 读取到的数据:0x";
        // for (size_t i = 0; i < sizeof(pnRespData); i++) {
        //     std::cout << std::hex << std::setw(2) << std::setfill('0') 
        //               << static_cast<int>(pnRespData[i]);
        // }
        // std::cout << std::dec << std::endl;

        if (pnRespData[1]==0x7F) {
                errmsg = "主机通过Programming Session [0x40 0x10] read, display反馈0x7F,升级失败";
                //SCREEN_LOGE("%s\n", errmsg.c_str());
                std::cout << "[READ PN失败] " << errmsg << std::endl;
                return false;
        }else{
                std::cout << "[READ PN结果] 从设备读取的PN: ";
                char displayPN[20]={0};
                memcpy(displayPN, &pnRespData[4], 20);
                if (memcmp(displayPN, binFileDecode_->getHeader().productPN, 20) != 0) {
                    errmsg = "显示屏读取的PN与bin文件解析的PN不匹配\n";
                    //SCREEN_LOGE("%s\n", errmsg.c_str());
                    std::cout << "[READ PN失败] " << errmsg << std::endl;
                    return false;
                } 
                std::cout << "[READ PN成功] PN匹配成功" << std::endl;
                return true;
        }
    }
    return false;
}

//step1
//主机通过(0x40 0x22 0xFEF3)通知读取显示屏的I2C通信协议版本
bool UpgradeProcess::readI2CVersion(const i2cDriverInfo &info)
{
    std::cout << "[READ I2C VERSION] 开始读取I2C协议版本" << std::endl;
    uint8_t i2cVerReqParam[] = {0x40,0x03,0x22,0xFE,0xF3};  
    uint8_t i2cVerRespData[14] = {0x0};
    std::string errmsg{};

    // 打印要发送的Read I2C Version命令
    // std::cout << "[READ I2C VERSION-I2C发送] 准备发送Read I2C Version命令: ";
    // for (size_t i = 0; i < sizeof(i2cVerReqParam)/sizeof(uint8_t); i++) {
    //     std::cout << "0x" << std::hex << std::setw(2) << std::setfill('0') 
    //               << static_cast<int>(i2cVerReqParam[i]);
    // }
    // std::cout << std::dec << std::endl;

    if (i2cProcessHandler::i2cWriteHandler(info,i2cVerReqParam,sizeof(i2cVerReqParam)/sizeof(uint8_t))) {
        std::cout << "[READ I2C VERSION-I2C写操作成功] i2caddr:0x" << std::hex << (int)info.i2cDevAddr 
                  << " ,regAddr:0x" << std::hex << (int)i2cVerReqParam[0] << std::dec << std::endl;
        //SCREEN_LOGD("i2cWrite i2caddr:%#x ,regAddr:%#x succeeded\n",info.i2cDevAddr,i2cVerReqParam[0]);
    } else {
        std::cout << "[READ I2C VERSION-I2C写操作失败] i2caddr:0x" << std::hex << (int)info.i2cDevAddr 
                  << " ,regAddr:0x" << std::hex << (int)i2cVerReqParam[0] << std::dec << std::endl;
        errmsg = "主机通过(0x40 0x22 0xFEF3)通知读取显示屏的I2C通信协议版本失败\n";
        //SCREEN_LOGE("%s\n", errmsg.c_str());
        std::cout << "[READ I2C VERSION失败] " << errmsg << std::endl;
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    //std::cout << "[READ I2C VERSION-I2C读操作] 开始读取响应" << std::endl;

    if (i2cProcessHandler::i2cReadHandler(info,0x41,sizeof(uint8_t),i2cVerRespData,sizeof(i2cVerRespData))) {
 
            // std::cout << "[READ I2C VERSION-I2C读操作成功] 读取到的数据: ";
            // for (size_t i = 0; i < sizeof(i2cVerRespData); i++) {
            //     std::cout << "0x" << std::hex << std::setw(2) << std::setfill('0') 
            //               << static_cast<int>(i2cVerRespData[i]);
            // }
            // std::cout << std::dec << std::endl;
            //SCREEN_LOGD("i2cread succeeded,i2cVerRespData[0]=%#x,i2cVerRespData[1]=%#x\n",i2cVerRespData[0],i2cVerRespData[1]);
            std::string i2cVersion;
            for (int i = 4; i < 14; i++) {
                i2cVersion += i2cVerRespData[i];
            }
            //SCREEN_LOGD("I2C协议版本: %s", i2cVersion.c_str());
            std::cout << "[READ I2C VERSION结果] I2C协议版本: " << i2cVersion.c_str() << std::endl;
            return true;
    } else {
            //SCREEN_LOGE("%s\n", errmsg.c_str());
            errmsg = "i2c version read failed\n";
            std::cout << "[READ I2C VERSION失败] " << errmsg << std::endl;
            return false;
    }
}
