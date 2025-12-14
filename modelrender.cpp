#include "includes.h"

inline bool isWeapon(std::string& modelName) {
	if ((modelName.find("v_") != std::string::npos || modelName.find("uid") != std::string::npos || modelName.find("stattrack") != std::string::npos) && modelName.find("arms") == std::string::npos) {
		return true;
	}

	return false;
}

void Hooks::DrawModelExecute( uintptr_t ctx, const DrawModelState_t& state, const ModelRenderInfo_t& info, matrix3x4_t* bone ) {
	// do chams.
	if( g_chams.DrawModel( ctx, state, info, bone ) ) {
		g_hooks.m_model_render.GetOldMethod< Hooks::DrawModelExecute_t >( IVModelRender::DRAWMODELEXECUTE )( this, ctx, state, info, bone );
	}

	// disable material force for next call.
	//g_csgo.m_studio_render->ForcedMaterialOverride( nullptr );

	const char* name = g_csgo.m_model_info->GetModelName(info.m_model);
	if ((
		(strstr(name, "defuser") && !strstr(name, "arms") && !strstr(name, "ied_dropped")) && !g_csgo.m_studio_render->m_pForcedMaterial
		||
		strstr(name, "models/weapons/w") && !strstr(name, "arms") && !strstr(name, "ied_dropped") && !g_csgo.m_studio_render->m_pForcedMaterial
		||
		(isWeapon(std::string(info.m_model->m_name)))) && g_menu.main.players.chams_viewmodel.get()) {
		g_chams.SetAlpha(g_menu.main.players.chams_viewmodel_col.get().a());
		g_chams.SetupMaterial(g_chams.materialMetallnZ, g_menu.main.players.chams_viewmodel_col.get(), false);
		g_hooks.m_model_render.GetOldMethod< Hooks::DrawModelExecute_t >(IVModelRender::DRAWMODELEXECUTE)(this, ctx, state, info, bone);
		g_csgo.m_studio_render->ForcedMaterialOverride(nullptr);
		g_csgo.m_render_view->SetColorModulation(colors::white);
		g_csgo.m_render_view->SetBlend(1.f);
		return;
	}
}