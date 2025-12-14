#include "includes.h"

void Hooks::DoExtraBoneProcessing(int a2, int a3, int a4, int a5, int a6, int a7) {

	// the server doesn't use this so let's not use it aswell.
	return;
}

void Hooks::StandardBlendingRules(int a2, int a3, int a4, int a5, int a6)
{
	// cast thisptr to player ptr.
	Player* player = (Player*) this;

	if ( !player || (player->index( ) - 1) > 63 )
		return g_hooks.m_StandardBlendingRules( this, a2, a3, a4, a5, a6 );

	// disable interpolation.
	if ( !(player->m_fEffects( ) & EF_NOINTERP) )
		player->m_fEffects( ) |= EF_NOINTERP;

	g_hooks.m_StandardBlendingRules( this, a2, a3, a4, a5, a6 );

	// restore interpolation.
	player->m_fEffects( ) &= ~EF_NOINTERP;

}

void Hooks::BuildTransformations(int a2, int a3, int a4, int a5, int a6, int a7) {
	// cast thisptr to player ptr.
	Player* player = (Player*) this;

	// get bone jiggle.
	int bone_jiggle = *reinterpret_cast<int*>(uintptr_t( player ) + 0x292C);

	// null bone jiggle to prevent attachments from jiggling around.
	*reinterpret_cast<int*>(uintptr_t( player ) + 0x292C) = 0;

	// call og.
	g_hooks.m_BuildTransformations( this, a2, a3, a4, a5, a6, a7 );

	// restore bone jiggle.
	*reinterpret_cast<int*>(uintptr_t( player ) + 0x292C) = bone_jiggle;
}

void Hooks::CalcView(vec3_t& eye_origin, vec3_t& eye_angles, float& z_near, float& z_far, float& fov)
{
	// cast thisptr to player ptr.
	Player* player = (Player*)this;

	const auto use_new_anim_state = *(int*)((uintptr_t)player + 0x39E1);

	*(bool*)((uintptr_t)player + 0x39E1) = false;

	g_hooks.m_1CalcView(this, eye_origin, eye_angles, z_near, z_far, fov);

	*(bool*)((uintptr_t)player + 0x39E1) = use_new_anim_state;
}

void Hooks::UpdateClientSideAnimation() {
	if (!g_csgo.m_engine->IsInGame() || !g_csgo.m_engine->IsConnected())
		return g_hooks.m_UpdateClientSideAnimation(this);

	if (!g_cl.m_update_anims)
		return;

	g_hooks.m_UpdateClientSideAnimation(this);
}

Weapon* Hooks::GetActiveWeapon() {
	Stack stack;

	static Address ret_1 = pattern::find(g_csgo.m_client_dll, XOR("85 C0 74 1D 8B 88 ? ? ? ? 85 C9"));

	// note - dex; stop call to CIronSightController::RenderScopeEffect inside CViewRender::RenderView.
	if (g_menu.main.visuals.noscope.get()) {
		if (stack.ReturnAddress() == ret_1)
			return nullptr;
	}

	return g_hooks.m_GetActiveWeapon(this);
}

bool Hooks::EntityShouldInterpolate() {
	// cast thisptr to player ptr.
	Player* player = (Player*)this;

	// let us ensure we aren't disabling interpolation on our local player model.
	if (player == g_cl.m_local)
		return true;

	// lets disable interpolation on these niggas.
	if (player->IsPlayer()) {
		return false;
	}

	// call og.
	g_hooks.m_EntityShouldInterpolate(this);

	return false;
}


void CustomEntityListener::OnEntityCreated(Entity* ent) {
	if (ent) {
		// player created.
		if (ent->IsPlayer()) {
			Player* player = ent->as< Player* >();

			// access out player data stucture and reset player data.
			AimPlayer* data = &g_aimbot.m_players[player->index() - 1];
			if (data)
				data->reset();

			// get ptr to vmt instance and reset tables.
			VMT* vmt = &g_hooks.m_player[player->index() - 1];
			if (vmt) { 
				// init vtable with new ptr.
				vmt->reset();
				vmt->init(player);

				// hook this on every player.
				g_hooks.m_DoExtraBoneProcessing = vmt->add< Hooks::DoExtraBoneProcessing_t >(Player::DOEXTRABONEPROCESSING, util::force_cast(&Hooks::DoExtraBoneProcessing));
				g_hooks.m_StandardBlendingRules = vmt->add< Hooks::StandardBlendingRules_t >(Player::STANDARDBLENDINGRULES, util::force_cast(&Hooks::StandardBlendingRules));
				//g_hooks.m_EntityShouldInterpolate = vmt->add< Hooks::EntityShouldInterpolate_t >(Player::ENTITYSHOULDINTERPOLATE, util::force_cast(&Hooks::EntityShouldInterpolate));

				// local gets special treatment.
				if (player->index() == g_csgo.m_engine->GetLocalPlayer()) {
					g_hooks.m_UpdateClientSideAnimation = vmt->add< Hooks::UpdateClientSideAnimation_t >(Player::UPDATECLIENTSIDEANIMATION, util::force_cast(&Hooks::UpdateClientSideAnimation));
					g_hooks.m_1CalcView = vmt->add< Hooks::CalcView_t >(270, util::force_cast(&Hooks::CalcView));
					g_hooks.m_GetActiveWeapon           = vmt->add< Hooks::GetActiveWeapon_t >( Player::GETACTIVEWEAPON, util::force_cast( &Hooks::GetActiveWeapon ) );
					g_hooks.m_BuildTransformations = vmt->add< Hooks::BuildTransformations_t >( Player::BUILDTRANSFORMATIONS, util::force_cast( &Hooks::BuildTransformations ) );
				}
			}
		}
	}
}

void CustomEntityListener::OnEntityDeleted(Entity* ent) {
	// note; IsPlayer doesn't work here because the ent class is CBaseEntity.
	if (ent && ent->index() >= 1 && ent->index() <= 64) {
		Player* player = ent->as< Player* >();

		// access out player data stucture and reset player data.
		AimPlayer* data = &g_aimbot.m_players[player->index() - 1];
		if (data)
			data->reset();

		// get ptr to vmt instance and reset tables.
		VMT* vmt = &g_hooks.m_player[player->index() - 1];
		if (vmt)
			vmt->reset();
	}
}