#pragma once

class C_VoiceCommunication
{
public:
	virtual void SendDataMsg(VoiceDataCustom* pData);
	virtual void RunUserData();
};

inline C_VoiceCommunication* g_VoiceCommunication = new C_VoiceCommunication();