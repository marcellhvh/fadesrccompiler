#include "includes.h"

bool Hooks::IsConnected( )
{

	/*
 - string: "IsLoadoutAllowed"
 - follow up v8::FunctionTemplate::New function
 - inside it go to second function that is being called after "if" statement.
 - after that u need to open first function that is inside it.[ before( *( int ( ** )( void ) )( *( _DWORD* ) dword_152350E4 + 516 ) )( ); ]
 */

	if ( !this || !g_csgo.m_engine )
		return false;

	Stack stack;

	static Address IsLoadoutAllowed{pattern::find( g_csgo.m_client_dll, XOR( "84 C0 75 04 B0 01 5F" ) )};
	if ( IsLoadoutAllowed )
	{
		if ( g_menu.main.misc.unlock.get( ) && g_csgo.m_engine->IsInGame() && stack.ReturnAddress( ) == IsLoadoutAllowed )
			return false;
	}

	return g_hooks.m_engine.GetOldMethod< IsConnected_t >( IVEngineClient::ISCONNECTED )(this);
}

bool Hooks::IsPaused( )
{
	if ( !this || !g_csgo.m_engine || !g_csgo.m_engine->IsInGame( ) )
		return g_hooks.m_engine.GetOldMethod< IsPaused_t >( IVEngineClient::ISPAUSED )(this);

	static DWORD* return_to_extrapolation = (DWORD*) (pattern::find( g_csgo.m_client_dll,
																	 XOR( "FF D0 A1 ?? ?? ?? ?? B9 ?? ?? ?? ?? D9 1D ?? ?? ?? ?? FF 50 34 85 C0 74 22 8B 0D ?? ?? ?? ??" ) ) + 0x29);

	if ( _ReturnAddress( ) == (void*) return_to_extrapolation )
		return true;

	return g_hooks.m_engine.GetOldMethod< IsPaused_t >( IVEngineClient::ISPAUSED )(this);
}

bool Hooks::IsHLTV( )
{
	if ( !this || !g_csgo.m_engine )
		return false;

	Stack stack;

	static const Address return_to_setup_velocity{pattern::find( g_csgo.m_client_dll, XOR( "84 C0 75 38 8B 0D ? ? ? ? 8B 01 8B 80" ) )};
	static const Address return_to_accumulate_layers = pattern::find( g_csgo.m_client_dll, XOR( "84 C0 75 0D F6 87" ) );

	if ( stack.ReturnAddress( ) == return_to_setup_velocity )
		return true;

	if ( stack.ReturnAddress( ) == return_to_accumulate_layers )
		return true;

	return g_hooks.m_engine.GetOldMethod< IsHLTV_t >( IVEngineClient::ISHLTV )(this);
}

void Hooks::EmitSound( IRecipientFilter& filter, int iEntIndex, int iChannel, const char* pSoundEntry, unsigned int nSoundEntryHash, const char* pSample, float flVolume, float flAttenuation, int nSeed, int iFlags, int iPitch, const vec3_t* pOrigin, const vec3_t* pDirection, void* pUtlVecOrigins, bool bUpdatePositions, float soundtime, int speakerentity )
{
	if ( strstr( pSample, "null" ) )
	{
		iFlags = (1 << 2) | (1 << 5);
	}

	g_hooks.m_engine_sound.GetOldMethod<EmitSound_t>( IEngineSound::EMITSOUND )(this, filter, iEntIndex, iChannel, pSoundEntry, nSoundEntryHash, pSample, flVolume, flAttenuation, nSeed, iFlags, iPitch, pOrigin, pDirection, pUtlVecOrigins, bUpdatePositions, soundtime, speakerentity);
}