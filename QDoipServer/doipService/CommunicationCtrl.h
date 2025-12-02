#ifndef COMMUNICATIONCTRL_H
#define COMMUNICATIONCTRL_H

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <cstdint>
#include <string>
#include <map>
#include <mutex>
#include "CommonDef.h"


namespace DOIP {
namespace APP {

class CommunicationCtrl {
protected:
	CommunicationCtrl();
	CommunicationCtrl(const CommunicationCtrl&) = delete;
	CommunicationCtrl& operator = (const CommunicationCtrl&) = delete;
public:
	~CommunicationCtrl();

	static CommunicationCtrl* getInstance();

    int communicationCtrlHandle(const uint8_t& sub_sid, const uint8_t& commutype);
    void onCommunicationCtrlCallback(const uint8_t& sub_sid);
    void onCommunicationCtrlError(const uint8_t& NRC);
private:
	std::timed_mutex m_mtx;
	uint8_t m_NRC;
    static CommunicationCtrl* ins;
};//CommunicationCtrl



}//APP
}//DOIP

#endif //COMMUNICATIONCTRL_H
