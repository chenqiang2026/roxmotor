#include "CommunicationCtrl.h"
#include "../thirdparty/doiplogger.h"
#include "McuClient.h"

namespace DOIP {
namespace APP {

CommunicationCtrl::CommunicationCtrl():
	m_NRC(0)
{
	ins = this;
}

CommunicationCtrl* CommunicationCtrl::ins = nullptr;

CommunicationCtrl:: ~CommunicationCtrl()
{
	ins = nullptr;
}

CommunicationCtrl* CommunicationCtrl::getInstance()
{
	if(!ins) {
		return ins = new CommunicationCtrl();
	}
	return ins;
}

int CommunicationCtrl::communicationCtrlHandle(const uint8_t& sub_sid, const uint8_t& commutype)//0x28
{
    m_NRC = 0xFF;
    int res = McuClient::getInstance()->requestCommunicationCtrl(sub_sid, commutype);
    if(res != 0) return -1;
	LOG_D("[%s: %d] ctrltype=0x%02X, commutype=0x%02X", __FUNCTION__, __LINE__, sub_sid, commutype);
	if(!m_mtx.try_lock_for(std::chrono::milliseconds(50))) m_NRC = 0x78;
	if(m_NRC == 0xFF && !m_mtx.try_lock_for(std::chrono::milliseconds(50))) m_NRC = 0x78;
	for(int i = 0; i<3; ++i){
		if(m_NRC == 0x78) McuClient::getInstance()->getNotifyError78()(0x28);
		else if(m_NRC != 0xFF) return m_NRC;
		if(!m_mtx.try_lock_for(std::chrono::milliseconds(2000)) && m_NRC == 0xFF) return -1;
	}
	m_NRC = 0xFF;
    return m_NRC;
}

void CommunicationCtrl::onCommunicationCtrlCallback(const uint8_t& sub_sid)
{
	LOG_D("[%s: %d]", __FUNCTION__, __LINE__);
	m_NRC = 0x0;
	m_mtx.unlock();
}

void CommunicationCtrl::onCommunicationCtrlError(const uint8_t& NRC)
{
	m_NRC = NRC;
	m_mtx.unlock();
}


}//APP
}//DOIP
