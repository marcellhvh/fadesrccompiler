#include "includes.h"

#define IS_IN_RANGE( value, max, min ) ( value >= max && value <= min )
#define GET_BITS( value ) ( IS_IN_RANGE( value, '0', '9' ) ? ( value - '0' ) : ( ( value & ( ~0x20 ) ) - 'A' + 0xA ) )
#define GET_BYTE( value ) ( GET_BITS( value[0] ) << 4 | GET_BITS( value[1] ) )


std::uintptr_t Scan2(const std::uintptr_t image, const std::string& signature, bool LOL) {
	if (!image) {
		return 0u;
	}

	auto image_base = (std::uintptr_t)(image);
	auto image_dos_hdr = (IMAGE_DOS_HEADER*)(image_base);

	if (image_dos_hdr->e_magic != IMAGE_DOS_SIGNATURE) {
		return 0u;
	}

	auto image_nt_hdrs = (IMAGE_NT_HEADERS*)(image_base + image_dos_hdr->e_lfanew);

	if (image_nt_hdrs->Signature != IMAGE_NT_SIGNATURE) {
		return 0u;
	}

	auto scan_begin = (std::uint8_t*)(image_base);
	auto scan_end = (std::uint8_t*)(image_base + image_nt_hdrs->OptionalHeader.SizeOfImage);

	std::uint8_t* scan_result = nullptr;
	std::uint8_t* scan_data = (std::uint8_t*)(signature.c_str());

	for (auto current = scan_begin; current < scan_end; current++) {
		if (*(std::uint8_t*)scan_data == '\?' || *current == GET_BYTE(scan_data)) {
			if (!scan_result)
				scan_result = current;

			if (!scan_data[2])
				return (std::uintptr_t)(scan_result);

			scan_data += (*(std::uint16_t*)scan_data == '\?\?' || *(std::uint8_t*)scan_data != '\?') ? 3 : 2;

			if (!*scan_data)
				return (std::uintptr_t)(scan_result);
		}
		else if (scan_result) {
			current = scan_result;
			scan_data = (std::uint8_t*)(signature.c_str());
			scan_result = nullptr;
		}
	}

	return 0u;
}

std::uintptr_t Scan(const std::string& image_name, const std::string& signature, bool LOL) {
	auto image = GetModuleHandleA(image_name.c_str());
	return Scan2((std::uintptr_t)image, signature, LOL);
}

void C_VoiceCommunication::SendDataMsg(VoiceDataCustom* pData)
{
	if ( !g_menu.main.misc.send_data.get( ) )
		return;

	// Creating message
	CCLCMsg_VoiceData_Legacy msg;
	memset(&msg, 0, sizeof(msg));

	static DWORD m_construct_voice_message = (DWORD)Scan("engine.dll", "56 57 8B F9 8D 4F 08 C7 07 ? ? ? ? E8 ? ? ? ? C7", true);

	auto func = (uint32_t(__fastcall*)(void*, void*))m_construct_voice_message;
	func((void*)&msg, nullptr);

	// Setup custom data
	msg.set_data(pData);

	// :D
	lame_string_t CommunicationString{ };

	// Setup voice message
	msg.data = &CommunicationString; // Its mad code
	msg.format = 0; // VoiceFormat_Steam
	msg.flags = 63; // All flags

	g_csgo.m_engine->GetNetChannelInfo()->SendNetMsg((INetMessage*)&msg, false, true);
}

int lastsent = 0;
void C_VoiceCommunication::RunUserData()
{
	if ( !g_menu.main.misc.send_data.get( ) )
		return;

	/* skip local servers */
	INetChannel* m_NetChannel = g_csgo.m_engine->GetNetChannelInfo();
	if (!m_NetChannel || !g_cl.m_local)
		return;

	/* build message */
	Voice_Vader packet;
	strcpy(packet.cheat_name, XOR("fade.tapping"));
	packet.make_sure = 1;
	packet.username = "admin";
	VoiceDataCustom data;
	memcpy(data.get_raw_data(), &packet, sizeof(packet));
	
	constexpr int EXPIRE_DURATION = 0.5f; // miliseconds-ish?
	bool should_send = g_csgo.m_globals->m_realtime - lastsent > EXPIRE_DURATION;

	if (!should_send)
		return;

	// Filling packet
	SendDataMsg(&data);

	lastsent = g_csgo.m_globals->m_realtime;
}