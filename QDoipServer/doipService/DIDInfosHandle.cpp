#include "DIDInfosHandle.h"
#include "../thirdparty/doiplogger.h"
#include "McuClient.h"
#include "SessionCtrl.h"

namespace DOIP {
namespace APP {

static CTimer tm;

DIDInfosHandle::DIDInfosHandle():
	m_writedDID(0),
	m_NRC(0)
{
	ins = this;
	m_datasource.insert(std::pair<uint16_t, DIDInfo>(0xF187,
	DIDInfo {
		.m_DIDName = "F187",
		.m_mode = READ_MODE,
		.m_updated = UPDATED,
		.m_data = "18100001AG",
		.m_printdata = "18100001AG",
		.m_des = "partnum零部件编号"
	}
	));//1

	m_datasource.insert(std::pair<uint16_t, DIDInfo>(0xF18A,
	DIDInfo {
		.m_DIDName = "F18A",
		.m_mode = READ_MODE,
		.m_updated = UPDATED,
		.m_data = "P10192",
		.m_printdata = "P10192",
		.m_des = "provider系统供应商公司名称代码"
	}
	));//2

	m_datasource.insert(std::pair<uint16_t, DIDInfo>(0xF197,
	DIDInfo {
		.m_DIDName = "F197",
		.m_mode = READ_MODE,
		.m_updated = UPDATED,
		.m_data = "IDCU      ",
		.m_printdata = "IDCU",
		.m_des = "ecu_name"
	}
	));//3

	m_datasource.insert(std::pair<uint16_t, DIDInfo>(0xF193,
	DIDInfo {
		.m_DIDName = "F193",
		.m_mode = READ_MODE,
		.m_updated = UPDATED,
		.m_data = {0x32, 0x31},
		.m_printdata = "V32.31",
		.m_des = "hw_version硬件版本号"
	}
	));//4

	m_datasource.insert(std::pair<uint16_t, DIDInfo>(0xF195,
	DIDInfo {
		.m_DIDName = "F195",
		.m_mode = READ_MODE,
		.m_updated = UPDATED,
		.m_data = {'V', '4', '_', 0x0, 0x2, '_', 0x0, 0x9},
		.m_printdata = "V4_0002_0009",
		.m_des = "sw_version软件版本号"
	}
	));//5

	m_datasource.insert(std::pair<uint16_t, DIDInfo>(0xF18C,
	DIDInfo {
		.m_DIDName = "F18C",
		.m_mode = READ_MODE,
		.m_updated = UPDATED,
		.m_data = "R11IDCUSN000000235",
		.m_printdata = "R11IDCUSN000000235",
		.m_des = "serial_numECU流水编号"
	}
	));//6

	m_datasource.insert(std::pair<uint16_t, DIDInfo>(0xF190,
	DIDInfo {
		.m_DIDName = "F190",
		.m_mode = READ_MODE | WRITE_MODE,
		.m_updated = UPDATED,
		.m_data = "R11VIN18800000235",
		.m_printdata = "R11VIN18800000235",
		.m_des = "vin"
	}
	));//7

	m_datasource.insert(std::pair<uint16_t, DIDInfo>(0xF188,
	DIDInfo {
		.m_DIDName = "F188",
		.m_mode = READ_MODE,
		.m_updated = UPDATED,
		.m_data = "S1810022AU",
		.m_printdata = "S1810022AU",
		.m_des = "sw_num软件零件号"
	}
	));//8

	m_datasource.insert(std::pair<uint16_t, DIDInfo>(0xF191,
	DIDInfo {
		.m_DIDName = "F191",
		.m_mode = READ_MODE,
		.m_updated = UPDATED,
		.m_data = "18100050AA",
		.m_printdata = "18100050AA",
		.m_des = "hw_num硬件零件号"
	}
	));//9

	m_datasource.insert(std::pair<uint16_t, DIDInfo>(0xF18B,
	DIDInfo {
		.m_DIDName = "F18B",
		.m_mode = READ_MODE,
		.m_updated = UPDATED,
		.m_data = {0x24, 0x01, 0x11},
		.m_printdata = "2024.01.11",
		.m_des = "控制器制造日期 年/月/日"
	}
	));//10


	m_datasource.insert(std::pair<uint16_t, DIDInfo>(0xF15A,
	DIDInfo {
		.m_DIDName = "F15A",
		.m_mode = READ_MODE | WRITE_MODE,
		.m_updated = UPDATED,
		.m_data = {0x24, 0x01, 0x11, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00},
		.m_printdata = "2024.01.11 00",
		.m_des = "Finger print 指纹 年/月/日/类型/序号"
	}
	));//11


	m_datasource.insert(std::pair<uint16_t, DIDInfo>(0xF103,
	DIDInfo {
		.m_DIDName = "F103",
		.m_mode = READ_MODE | WRITE_MODE,
		.m_updated = UPDATED,
		.m_data = {0x24, 0x01, 0x11, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00},
		.m_printdata = "2024.01.11 00",
		.m_des = "Equipment maintenance fingerprint information 设备维修指纹信息 年/月/日/类型/序号"
	}
	));//12

	m_datasource.insert(std::pair<uint16_t, DIDInfo>(0xF101,
	DIDInfo {
		.m_DIDName = "F101",
		.m_mode = READ_MODE | WRITE_MODE,
		.m_updated = UPDATED,
		.m_data = CByteArry::fromHexstr("10A0AFFEBFDEABBFFEF3F1080000000000000000000000000000000000000000", ""),
		.m_printdata = "10A0AFFEBFDEABBFFEF3F1080000000000000000000000000000000000000000",
		.m_des = "config_word车辆配置"
	}
	));//13

	m_datasource.insert(std::pair<uint16_t, DIDInfo>(0xF186,
	DIDInfo {
		.m_DIDName = "F186",
		.m_mode = READ_MODE,
		.m_updated = UPDATED,
		.m_data = {0x01}, //session
		.m_printdata = "01",
		.m_des = "Active diagnostic session 激活的诊断会话"
	}
	));//14

	m_datasource.insert(std::pair<uint16_t, DIDInfo>(0xF1F1,
	DIDInfo {
		.m_DIDName = "F1F1",
		.m_mode = READ_MODE,
		.m_updated = UPDATED,
		.m_data = {'R','1','1','_','U','A','O','1','_','2','5','0','1','2','3','_','4','3','6','4',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '},
		.m_printdata = "R11_UAO1_250123_4364",
		.m_des = "MPU1软件版本号"
	}
	));//15

	m_datasource.insert(std::pair<uint16_t, DIDInfo>(0xF1FA,
	DIDInfo {
		.m_DIDName = "F1FA",
		.m_mode = READ_MODE,
		.m_updated = UPDATED,
		.m_data = {'R','1','1','_','D','D','O','1','_','2','4','0','8','1','4','_','0','4','7','0','_','5','8','2','0',' ',' ',' ',' ',' '},
		.m_printdata = "R11_DDO1_240814_0470_5820",
		.m_des = "MCU1软件版本号"
	}
	));//16

	m_datasource.insert(std::pair<uint16_t, DIDInfo>(0xA621,
	DIDInfo {
		.m_DIDName = "A621",
		.m_mode = READ_MODE,
		.m_updated = UPDATED,
		.m_data = {0x00},//unsigned 
		.m_printdata = "0",
		.m_des = "以太网端口数量"
	}
	));//17

	m_datasource.insert(std::pair<uint16_t, DIDInfo>(0xA622,
	DIDInfo {
		.m_DIDName = "A622",
		.m_mode = READ_MODE,
		.m_updated = UPDATED,
		.m_data = {0x00},
		.m_printdata = "0",//0~7
		.m_des = "以太网信号质量0~7"
	}
	));//18

	m_datasource.insert(std::pair<uint16_t, DIDInfo>(0xA624,
	DIDInfo {
		.m_DIDName = "A624",
		.m_mode = READ_MODE,
		.m_updated = UPDATED,
		.m_data = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		.m_printdata = "00-00-00-00-00-00",
		.m_des = "MAC地址"
	}
	));//19

	m_datasource.insert(std::pair<uint16_t, DIDInfo>(0xDF01,
	DIDInfo {
		.m_DIDName = "DF01",
		.m_mode = READ_MODE,
		.m_updated = UPDATED,
		.m_data = CByteArry::fromHexstr("0000000000000000000000000000000000000000000000000000000000000000", ""),
		.m_printdata = "0000000000000000000000000000000000000000000000000000000000000000",
		.m_des = "目标路径下升级包的HASH"
	}
	));//20

	m_datasource.insert(std::pair<uint16_t, DIDInfo>(0xD43D,
	DIDInfo {
		.m_DIDName = "D43D",
		.m_mode = READ_MODE,
		.m_updated = UPDATED,
		.m_data = {0x00},
		.m_printdata = "0",
		.m_des = "IDCU通信密钥申请状态 0x0~0x4"
	}
	));//21

	m_datasource.insert(std::pair<uint16_t, DIDInfo>(0xD43E,
	DIDInfo {
		.m_DIDName = "D43E",
		.m_mode = READ_MODE,
		.m_updated = UPDATED,
		.m_data = {0x00},
		.m_printdata = "0",
		.m_des = "A2B通路状态 0x0~0x6"
	}
	));//22

	m_datasource.insert(std::pair<uint16_t, DIDInfo>(0xD001,
	DIDInfo {
		.m_DIDName = "D001",
		.m_mode = READ_MODE,
		.m_updated = UPDATED,
		.m_data = {0x00, 0x00, 0x00},
		.m_printdata = "0.0",
		.m_des = "ODO仪表总里程 0~999999.9Km"
	}
	));//23

	m_datasource.insert(std::pair<uint16_t, DIDInfo>(0xD0D3,
	DIDInfo {
		.m_DIDName = "D0D3",
		.m_mode = READ_MODE,
		.m_updated = UPDATED,
		.m_data = {0x00},
		.m_printdata = "0",
		.m_des = "ECUBackupStatus备份分区状态 0~1"
	}
	));//24

	m_datasource.insert(std::pair<uint16_t, DIDInfo>(0xF1A0,
	DIDInfo {
		.m_DIDName = "F1A0",
		.m_mode = READ_MODE,
		.m_updated = UPDATED,
		.m_data = {'V', '1', '_', 0, 1, '_', 0, 1},
		.m_printdata = "V1_01_01",
		.m_des = "ECUBackupSoftwareVersionNumber备份区软件版本号"
	}
	));//25

	m_datasource.insert(std::pair<uint16_t, DIDInfo>(0xF1A1,
	DIDInfo {
		.m_DIDName = "F1A1",
		.m_mode = READ_MODE,
		.m_updated = UPDATED,
		.m_data = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},
		.m_printdata = "          ",
		.m_des = "ECUBackupSoftwareNumber备份区软件零件号"
	}
	));//26
//MCE==========================================================
	m_datasource.insert(std::pair<uint16_t, DIDInfo>(0xF111,
	DIDInfo {
		.m_DIDName = "F111",
		.m_mode = WRITE_MODE,
		.m_updated = UPDATED,
		.m_data = "22222222222222222222222222222222",
		.m_printdata = "22222222222222222222222222222222",
		.m_des = "VDC WIFI SSID"
	}
	));//27

	m_datasource.insert(std::pair<uint16_t, DIDInfo>(0xF112,
	DIDInfo {
		.m_DIDName = "F112",
		.m_mode = WRITE_MODE,
		.m_updated = 1,
		.m_data = "                                                                ",
		.m_printdata = "                                                                ",
		.m_des = "VDC Wifi 密码"
	}
	));//28

	uint8_t macs[335] = {0};
	m_datasource.insert(std::pair<uint16_t, DIDInfo>(0xF113,
	DIDInfo {
		.m_DIDName = "F113",
		.m_mode = WRITE_MODE,
		.m_updated = UPDATED,
		.m_data = CByteArry(macs, sizeof(macs)),
		.m_printdata = "      _      _      _      _      _      _      _      _      _      ",
		.m_des = "VDC WIFI AP MAC白名单"
	}
	));//29

	m_datasource.insert(std::pair<uint16_t, DIDInfo>(0xF114,
	DIDInfo {
		.m_DIDName = "F114",
		.m_mode = READ_MODE,
		.m_updated = UPDATED,
		.m_data = {0},
		.m_printdata = "0",
		.m_des = "VDC WIFI 信息写入状态"
	}
	));//30

	m_datasource.insert(std::pair<uint16_t, DIDInfo>(0xF102,
	DIDInfo {
		.m_DIDName = "F102",
		.m_mode = READ_MODE,
		.m_updated = UPDATED,
		.m_data = CByteArry::fromHexstr("00000000000000000000000000000000", ""),
		.m_printdata = "00000000000000000000000000000000",
		.m_des = "IDCU配置字"
	}
	));//31

	m_datasource.insert(std::pair<uint16_t, DIDInfo>(0xF110,
	DIDInfo {
		.m_DIDName = "F110",
		.m_mode = READ_MODE | WRITE_MODE,
		.m_updated = UPDATED,
		.m_data = "                 ",
		.m_printdata = "                 ",
		.m_des = "IDCU VID"
	}
	));//32

	m_datasource.insert(std::pair<uint16_t, DIDInfo>(0xF12A,
	DIDInfo {
		.m_DIDName = "F12A",
		.m_mode = READ_MODE,
		.m_updated = UPDATED,
		.m_data = {'V', '1', '_', '0', '0', '_', '0', '0'},
		.m_printdata = "V1_00_00",
		.m_des = "DBC 版本"
	}
	));//33
	
	m_datasource.insert(std::pair<uint16_t, DIDInfo>(0xF12B,
	DIDInfo {
		.m_DIDName = "F12B",
		.m_mode = READ_MODE,
		.m_updated = UPDATED,
		.m_data = {'V', '1', '_', '0', '0', '_', '0', '0'},
		.m_printdata = "V1_00_00",
		.m_des = "DDS 版本"
	}
	));//34

	LOG_D("[%s: %d] DID num=%d", __FUNCTION__, __LINE__, m_datasource.size());
	// for(auto it = m_datasource.begin(); it!= m_datasource.end(); ++it){
	// 	it->second.printDIDInfo();
	// }
	tm.setRepeat(1);
	tm.connectTimer([=]{
		for(auto it = m_datasource.begin(); it!= m_datasource.end(); ++it){
			if(NEED_READ == it->second.m_updated) {
				LOG_I("[%s: %d] timer read DID=%04X....", __FUNCTION__, __LINE__,it->first);
				McuClient::getInstance()->requestDID2(it->first);
				break;
			}
			if(NEED_WRITE == it->second.m_updated) {
				if(it->first != DID_Session && it->first != 0xDF01) {
					LOG_I("[%s: %d] timer write DID=%04X....", __FUNCTION__, __LINE__,it->first);
					McuClient::getInstance()->writeDID2(it->first, it->second.m_data.data(), it->second.m_data.length());
				}
				break;
			}
		}
	});
}

DIDInfosHandle::~DIDInfosHandle()
{
	ins = nullptr;
}

DIDInfosHandle* DIDInfosHandle::ins = nullptr;

DIDInfosHandle* DIDInfosHandle::getInstance()
{
	if(!ins) {
		return ins = new DIDInfosHandle();
	}
	return ins;
}

void DIDInfosHandle::updateDIDdata()
{
	if(!tm.isRunning()) {
		tm.start(100);
		LOG_D("[%s: %d] start update DID timer....", __FUNCTION__, __LINE__);
	}

}

void DIDInfosHandle::readDID(const uint16_t* DIDS, const int size)
{
	if(!DIDS || size < 1) return;
	for(int i = 0; i<size; ++i) {
		m_datasource[DIDS[i]].m_updated = NEED_READ;
	}
}

int DIDInfosHandle::readDID2(const uint16_t& DID)//0x22
{
	LOG_D("[%s: %d] DID =%04X", __FUNCTION__, __LINE__, DID);
	int mode = m_datasource[DID].m_mode;
	if((mode & READ_MODE) == 0) return 0x12;
	if(m_datasource[DID].m_updated == NEED_WRITE) return 0;
	m_NRC = 0xFF;
	int res = McuClient::getInstance()->requestDID2(DID);
	if(res != 0) return -1;
	if(!m_mtx.try_lock_for(std::chrono::milliseconds(50))) m_NRC = 0x78;
	if(m_NRC == 0xFF && !m_mtx.try_lock_for(std::chrono::milliseconds(50))) m_NRC = 0x78;
	for(int i = 0; i<1; ++i){
		if(m_NRC == 0x78) McuClient::getInstance()->getNotifyError78()(0x22);
		else if(m_NRC != 0xFF) return m_NRC;
		if(!m_mtx.try_lock_for(std::chrono::milliseconds(2000)) && m_NRC == 0xFF) return -1;
	}
	LOG_D("[%s: %d] 3333 m_NRC =%02X", __FUNCTION__, __LINE__, m_NRC);
	m_NRC = 0xFF;
	return m_NRC;
}

CByteArry DIDInfosHandle::getDIDdata(const uint16_t& DID)
{
	if(m_datasource.find(DID) == m_datasource.end()){
		LOG_D("[%s: %d] undefined DID =%04X", __FUNCTION__, __LINE__, DID);
		return nullptr;
	}
	return m_datasource[DID].m_data;
}

int DIDInfosHandle::writeDIDdata(const uint16_t& DID, const uint8_t* data, const int& len)//0x2E
{
	LOG_D("[%s: %d] DID =%04X, len=%d", __FUNCTION__, __LINE__, DID, len);
	if(data == nullptr || len <= 0) return -1;

	if(m_datasource.find(DID) == m_datasource.end()){
		LOG_D("[%s: %d] undefined DID =%04X", __FUNCTION__, __LINE__, DID);
		return -1;
	}

	DIDInfo& info = m_datasource[DID];

	if(info.m_data.length() != len) {
		LOG_D("[%s: %d] DID =%04X write len incorrect", __FUNCTION__, __LINE__, DID);
		return 0x13;
	}
	if((info.m_mode & WRITE_MODE) == 0) {
		LOG_D("[%s: %d] DID =%04X no write access", __FUNCTION__, __LINE__, DID);
		return 0x12;
	}

	m_NRC = 0xFF;
	int res = McuClient::getInstance()->writeDID2(DID, data, len);
	if(res != 0) return -1;
	m_writedDID = DID;
	if(!m_mtx.try_lock_for(std::chrono::milliseconds(50))) m_NRC = 0x78;
	if(m_NRC == 0xFF && !m_mtx.try_lock_for(std::chrono::milliseconds(50))) m_NRC = 0x78;
	for(int i = 0; i<1; ++i){
		if(m_NRC == 0x78) McuClient::getInstance()->getNotifyError78()(0x2E);
		else if(m_NRC != 0xFF) {
			m_writedDID = 0;
			return m_NRC;
		}
		if(!m_mtx.try_lock_for(std::chrono::milliseconds(2000)) && m_NRC == 0xFF) return -1;
	}
	m_writedDID = 0;
	m_NRC = 0xFF;
	return m_NRC;
}


void DIDInfosHandle::onDIDdataCallback(const uint16_t& DID, const uint8_t* data, const int& len, int8_t update)
{
	LOG_D("[%s: %d] DID =%04X, len=%d update=%d", __FUNCTION__, __LINE__, DID, len, update);
	if(len <= 0) return;

	CByteArry did_data(data, len);
	DIDInfo& info = m_datasource[DID];
	if(0xF186 == DID && info.m_updated == NEED_READ) SessionCtrl::getInstance()->setSession(data[0]);
	info.m_data = did_data;
	info.m_updated = update;
	char buf[512] = {};
	if(0xF18B == DID) {
		snprintf(buf, sizeof(buf), "20%02X.%02X.%02X ", data[0], data[1], data[2]);
	}
	else if(0xF15A == DID || 0xF103 == DID) {
		snprintf(buf, sizeof(buf), "20%02X.%02X.%02X %02X %c%c%c%c%c", data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7], data[8]);
	}
	else if(0xF193 == DID) {
		snprintf(buf, sizeof(buf), "V%02X.V%02X", data[0], data[1]);
	}
	else if(0xF101 == DID || 0xF102 == DID) {
		for(int i = 0; i < len; ++i)
			snprintf(buf+2*i, 3, "%02X", data[i]);
	}
	else if(0xF195 == DID || 0xF1A0 == DID) {
		if(data[1] < ' ')
			snprintf(buf, sizeof(buf), "%c%d%c%02X%02X%c%02X%02X", data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
		else
			snprintf(buf, sizeof(buf), "%c%c%c%02X%02X%c%02X%02X", data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
	}
	else if(0xF186 == DID || 0xD43D == DID || 0xD43E == DID || 0xD0D3 == DID || 0xF114 == DID) {
		snprintf(buf, sizeof(buf), "%02X", data[0]);
	}
	else if(0xA621 == DID || 0xA622 == DID) {
		snprintf(buf, sizeof(buf), "%d", data[0]);
	}
	else if(0xA624 == DID) {//MAC addr
		snprintf(buf, sizeof(buf), "%02X-%02X-%02X-%02X-%02X-%02X", data[0], data[1], data[2], data[3], data[4], data[5]);
	}
	else if(0xDF01 == DID) {//hash
		for(int i = 0; i < len; ++i)
			snprintf(buf+2*i, 3, "%02x", data[i]);
	}
	else if(0xD001 == DID) {//ODO
		snprintf(buf, sizeof(buf), "%.1f", ((data[0]<<16) | (data[1]<<8) | data[2])*0.1f);
	}
	else if(DID == 0xF111 || DID == 0xF112) {
		for(int i = 0; i < len; ++i)
			snprintf(buf+i, 2, "%c", data[i]);
		onVDCUpdated(DID);
	}
	else if(DID == 0xF113) {
		int offset = 0;
		for(int i = 0; i < len; ++i) {
			if(i%7 == 6) {
				buf[offset-1] = data[i];
			}
			else {
				snprintf(buf+offset, 4, "%02x:", data[i]);
				offset += 3;
			}
		}
		buf[offset-1] = '\0';
		onVDCUpdated(DID);
	}
	else if(DID == 0xF114) {
		sprintf(buf, "%d", data[0]);
	}
	else {
		for(int i = 0; i < len; ++i){
			if(data[i] == ' ' || data[i] == 0xFF) buf[i] = '\0';
			else buf[i] = data[i];
		}
	}
	info.m_printdata = std::string(buf);
	info.printDIDInfo();
}

void DIDInfosHandle::writeDIDdata2(const uint16_t& DID, const uint8_t* data, const int& len)
{
	auto& DIDinfo = m_datasource[DID];
	DIDinfo.m_data = CByteArry(data, len);
	DIDinfo.m_updated = NEED_WRITE;
}

void DIDInfosHandle::onDIDdataWritedErro(const uint8_t& NRC)
{
	if(0 == m_writedDID) return;
	m_NRC = NRC;
	m_mtx.unlock();
}

void DIDInfosHandle::onDIDdataWrited(const uint16_t& DID)
{
	LOG_D("[%s: %d] DID =%04X writed success", __FUNCTION__, __LINE__, DID);
	if(m_datasource[DID].m_updated == NEED_WRITE) {
		m_datasource[DID].m_updated = UPDATED;
		return;
	}
	m_NRC = 0;
	auto did = DID;
	m_mtx.unlock();
	readDID(&did, 1);
}

void DIDInfosHandle::onDIDdataReadErro(const uint8_t& NRC)
{
	m_NRC = NRC;
	m_mtx.unlock();
}

void DIDInfosHandle::onDIDdataReaded(const uint16_t& DID)
{
	m_NRC = 0;
	m_mtx.unlock();
}

void DIDInfosHandle::onVDCUpdated(const uint16_t& vdc_DID)
{
	uint8_t state = 2<<(2*(vdc_DID-0xF111));
	uint8_t writeState =  m_datasource[0xF114].m_data.data()[0];
	writeState &= ~(3<<(2*(vdc_DID-0xF111)));
	writeState |= state;
	LOG_D("[%s: %d] writestate=%02X", __FUNCTION__, __LINE__, writeState);
	m_datasource[0xF114].m_data.data()[0] = writeState;
	m_datasource[0xF114].m_updated = NEED_WRITE;
}

bool DIDInfosHandle::getMCUVersion(std::string& version)
{
    FILE *fp = fopen(SYSTEM_VERSION_PATH_FILE, "r");
    if (fp == NULL) {
        LOG_E("open system version file(/version.txt) fail");
        return false;
    }
    char buff[256];
    memset(buff, 0, 256);
    char *versionBuff = NULL;
    while (fgets(buff, 256,  fp) != NULL) {
        buff[strcspn(buff, "\n")] = '\0';
        versionBuff = strstr(buff, "MCU_Version=");
        if (versionBuff != NULL) {
            version = versionBuff + 12;
            LOG_I("MCU version: %s", version.c_str());
            break;
        }
    }
    fclose(fp);

    if (versionBuff == NULL) {
        LOG_E("MCU version not found");
        return false;
    }
    return true;
}

bool DIDInfosHandle::getMPUVersion(std::string& version)
{
    FILE *fp = fopen(SYSTEM_VERSION_PATH_FILE, "r");
    if (fp == NULL) {
        LOG_E("open system version file(/version.txt) fail");
        return false;
    }
    char buff[256];
    memset(buff, 0, 256);
    char *versionBuff = NULL;
    while (fgets(buff, 256,  fp) != NULL) {
        buff[strcspn(buff, "\n")] = '\0';
        versionBuff = strstr(buff, "MPU_Version=");
        if (versionBuff != NULL) {
            version = versionBuff + 12;
            LOG_I("MPU version: %s", version.c_str());
            break;
        }
    }
    fclose(fp);

    if (versionBuff == NULL) {
        LOG_E("MPU version not found");
        return false;
    }
    return true;
}

}//APP
}//DOIP
