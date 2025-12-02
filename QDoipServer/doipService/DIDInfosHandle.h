#ifndef DIDINFOSHANDLE_H
#define DIDINFOSHANDLE_H

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <cstdint>
#include <string>
#include <map>
#include "../thirdparty/CommonDef.h"
#include "../thirdparty/doiplogger.h"

namespace DOIP {
namespace APP {

#define SYSTEM_VERSION_PATH_FILE ("/version.txt")
typedef struct _DIDInfo
{
	char m_DIDName[5];
	uint8_t m_mode;
	int8_t m_updated;
	CByteArry m_data;
	std::string m_printdata;
	std::string m_des;
	/* data */
	inline void printDIDInfo() {
		LOG_I("[%s: %d] \n--------\nDID:%s\nmode:%s\ndata:%s\ndes:%s\n-------\n",
		__FUNCTION__, __LINE__, m_DIDName, m_mode==3?"R/W":(m_mode==2?"W":(m_mode==1?"R":"N")), m_printdata.c_str(), m_des.c_str());
	}
}DIDInfo;

typedef enum _DIDMode {
	NO_MODE = 0x0,
	READ_MODE = 0x1,
	WRITE_MODE = 0x1 << 1
} DIDMode;

typedef enum _DIDSTATE {
	NEED_READ = 0x0,
	UPDATED = 0x1,
	NEED_WRITE = 0x2
} DIDSTATE;


class DIDInfosHandle {
protected:
	DIDInfosHandle();
	DIDInfosHandle(const DIDInfosHandle&) = delete;
	DIDInfosHandle& operator = (const DIDInfosHandle&) = delete;
public:
	~DIDInfosHandle();

	static DIDInfosHandle* getInstance();

	void updateDIDdata();

	void readDID(const uint16_t* DIDS, const int size);

	int readDID2(const uint16_t& DID);

	inline std::map<uint16_t, DIDInfo>& getDataSource(){
		return m_datasource;
	}

	CByteArry getDIDdata(const uint16_t& DID);

	int writeDIDdata(const uint16_t& DID, const uint8_t* data, const int& len);

	void writeDIDdata2(const uint16_t& DID, const uint8_t* data, const int& len);

	void onDIDdataCallback(const uint16_t& DID, const uint8_t* data, const int& len, int8_t update);

	void onDIDdataWritedErro(const uint8_t& NRC);

	void onDIDdataWrited(const uint16_t& DID);

	void onDIDdataReadErro(const uint8_t& NRC);

	void onDIDdataReaded(const uint16_t& DID);

	void onVDCUpdated(const uint16_t& vdc_DID);

	static bool getMCUVersion(std::string& version);
	static bool getMPUVersion(std::string& version);
private:
	std::map<uint16_t, DIDInfo> m_datasource;
	std::timed_mutex m_mtx;
	uint16_t m_writedDID;
	uint8_t m_NRC;
	static DIDInfosHandle* ins;
};//DIDInfosHandle


}//APP
}//DOIP

#endif //DIDINFOSHANDLE_H
