#include "includes.h"

Shots g_shots{ };

void Shots::OnShotFire(Player *target, float damage, LagRecord *record, int hitbox, int hitgroup, vec3_t aim_point) {
	// setup new shot data.
	ShotRecord shot;
	shot.m_target = target;
	shot.m_record = record;
	shot.m_time = g_csgo.m_globals->m_realtime;
	shot.m_lat = g_cl.m_latency;
	shot.m_damage = damage;
	shot.m_pos = g_cl.m_shoot_pos;
	shot.m_impacted = false;
	shot.m_confirmed = false;
	shot.m_aim_point = aim_point;
	shot.m_hurt = false;
	shot.m_hitbox = hitbox;
	shot.m_hitgroup = hitgroup;
	shot.m_range = g_cl.m_weapon ? g_cl.m_weapon_info->m_range : 8192.f;

	// we are not shooting manually.
	// and this is the first bullet, only do this once.
	if (target && record) {
		// increment total shots on this player.
		AimPlayer* data = &g_aimbot.m_players[target->index() - 1];
		if (data)
			++data->m_shots;
	}

	// add to tracks.
	m_shots.push_front(shot);

	// no need to keep an insane amount of shots.
	while (m_shots.size() > 128)
		m_shots.pop_back();
}

void Shots::OnImpact(IGameEvent *evt) {
	int        attacker;
	vec3_t     pos, dir, start, end;
	float      time;
	CGameTrace trace;

	// screw this.
	if (!evt || !g_cl.m_local)
		return;

	// get attacker, if its not us, screw it.
	attacker = g_csgo.m_engine->GetPlayerForUserID(evt->m_keys->FindKey(HASH("userid"))->GetInt());
	if (attacker != g_csgo.m_engine->GetLocalPlayer())
		return;

	// decode impact coordinates and convert to vec3.
	pos = {
		evt->m_keys->FindKey(HASH("x"))->GetFloat(),
		evt->m_keys->FindKey(HASH("y"))->GetFloat(),
		evt->m_keys->FindKey(HASH("z"))->GetFloat()
	};

	// get prediction time at this point.
	time = game::TICKS_TO_TIME(g_cl.m_local->m_nTickBase());

	// add to visual impacts if we have features that rely on it enabled.
	// todo - dex; need to match shots for this to have proper GetShootPosition, don't really care to do it anymore.
	//if (g_menu.main.visuals.impact_beams.get())
	//	m_vis_impacts.push_back({ pos, g_cl.m_local->wpn_shoot_pos(), g_cl.m_local->m_nTickBase() });

	// we did not take a shot yet.
	if (m_shots.empty())
		return;

	struct ShotMatch_t { float delta; ShotRecord *shot; };
	ShotMatch_t match;
	match.delta = std::numeric_limits< float >::max();
	match.shot = nullptr;

	// iterate all shots.
	for (auto &s : m_shots) {

		// this shot was already matched
		// with a 'bullet_impact' event.
		if (s.m_impacted)
			continue;

		// get the delta between the current time
		// and the predicted arrival time of the shot.
		float delta = fabsf(g_csgo.m_globals->m_realtime - s.m_time);

		// store this shot as being the best for now.
		if (delta <= match.delta) {
			match.delta = delta;
			match.shot = &s;
		}
	}

	// no valid shotrecord was found.
	ShotRecord *shot = match.shot;
	if (!shot)
		return;

	// this shot was matched.
	shot->m_impacted = true;
	shot->m_impact_pos = pos;
}

void Shots::OnHurt(IGameEvent *evt) {
	int         attacker, victim, group, hp;
	float       time, damage;
	std::string name;
	vec3_t     pos, dir, start, end;
	CGameTrace trace;

	if (!evt || !g_cl.m_local)
		return;

	attacker = g_csgo.m_engine->GetPlayerForUserID(evt->m_keys->FindKey(HASH("attacker"))->GetInt());
	victim = g_csgo.m_engine->GetPlayerForUserID(evt->m_keys->FindKey(HASH("userid"))->GetInt());

	// skip invalid player indexes.
	// should never happen? world entity could be attacker, or a nade that hits you.
	if (attacker < 1 || attacker > 64 || victim < 1 || victim > 64)
		return;

	// we were not the attacker or we hurt ourselves.
	else if (attacker != g_csgo.m_engine->GetLocalPlayer() || victim == g_csgo.m_engine->GetLocalPlayer())
		return;

	// get hitgroup.
	// players that get naded ( DMG_BLAST ) or stabbed seem to be put as HITGROUP_GENERIC.
	group = evt->m_keys->FindKey(HASH("hitgroup"))->GetInt();

	// invalid hitgroups ( note - dex; HITGROUP_GEAR isn't really invalid, seems to be set for hands and stuff? ).
	if (group == HITGROUP_GEAR)
		return;

	// get the player that was hurt.
	Player *target = g_csgo.m_entlist->GetClientEntity< Player * >(victim);
	if (!target)
		return;

	// get player info.
	player_info_t info;
	if (!g_csgo.m_engine->GetPlayerInfo(victim, &info))
		return;

	// get player name;
	name = std::string(info.m_name).substr(0, 24);

	// get damage reported by the server.
	damage = (float)evt->m_keys->FindKey(HASH("dmg_health"))->GetInt();

	// get remaining hp.
	hp = evt->m_keys->FindKey(HASH("health"))->GetInt();

	// get prediction time at this point.
	time = game::TICKS_TO_TIME(g_cl.m_local->m_nTickBase());

	// hitmarker.
	if (g_menu.main.misc.hitmarker.get()) {
		g_visuals.m_hit_duration = 1.f;
		g_visuals.m_hit_start = g_csgo.m_globals->m_curtime;
		g_visuals.m_hit_end = g_visuals.m_hit_start + g_visuals.m_hit_duration;

		g_csgo.m_sound->EmitAmbientSound(XOR("buttons/arena_switch_press_02.wav"), 1.f);
	}

	// print this shit.
	if (group == HITGROUP_GENERIC)
		return;

	if (g_menu.main.misc.notifications.get(1)) {
		std::string out = tfm::format(XOR("hit %s in the %s for %i damage (%i health remaining)\n"), name, m_groups[group], (int)damage, hp);
		g_notify.add(out);
	}

	// if we hit a player, mark vis impacts.
	if (!m_vis_impacts.empty()) {
		for (auto &i : m_vis_impacts) {
			if (i.m_tickbase == g_cl.m_local->m_nTickBase())
				i.m_hit_player = true;
		}
	}

	struct ShotMatch_t { float delta; ShotRecord *shot; };
	ShotMatch_t match;
	match.delta = std::numeric_limits< float >::max();
	match.shot = nullptr;

	// iterate all shots.
	for (auto &s : m_shots) {

		// this shot was already matched
		// with a 'player_hurt' event.
		if (s.m_hurt)
			continue;

		// get the delta between the current time
		// and the predicted arrival time of the shot.
		float delta = fabsf(g_csgo.m_globals->m_realtime - s.m_time);

		// store this shot as being the best for now.
		if (delta <= match.delta) {
			match.delta = delta;
			match.shot = &s;
		}
	}

	// no valid shotrecord was found.
	ShotRecord* shot = match.shot;
	if (!shot)
		return;

	// this shot was matched.
	shot->m_hurt = true;
}

void Shots::OnWeaponFire(IGameEvent *evt) {
	int        attacker;

	// screw this.
	if (!evt || !g_cl.m_local)
		return;

	// get attacker, if its not us, screw it.
	attacker = g_csgo.m_engine->GetPlayerForUserID(evt->m_keys->FindKey(HASH("userid"))->GetInt());
	if (attacker != g_csgo.m_engine->GetLocalPlayer())
		return;

	struct ShotMatch_t { float delta; ShotRecord *shot; };
	ShotMatch_t match;
	match.delta = std::numeric_limits< float >::max();
	match.shot = nullptr;

	// iterate all shots.
	for (auto &s : m_shots) {

		// this shot was already matched
		// with a 'weapon_fire' event.
		if (s.m_confirmed)
			continue;

		// get the delta between the current time
		// and the predicted arrival time of the shot.
		float delta = fabsf(g_csgo.m_globals->m_realtime - s.m_time);

		// store this shot as being the best for now.
		if (delta <= match.delta) {
			match.delta = delta;
			match.shot = &s;
		}
	}

	// no valid shotrecord was found.
	ShotRecord* shot = match.shot;
	if (!shot)
		return;

	// this shot was matched.
	shot->m_confirmed = true;
}

void Shots::OnShotMiss(ShotRecord& shot) {
	vec3_t     pos, dir, start, end;
	CGameTrace trace;

	// shots we fire manually won't have a record.
	if (!shot.m_record)
		return;

	// nospread mode.
	if (g_menu.main.config.mode.get() == 1)
		return;

	// not in nospread mode, see if the shot missed due to spread.
	Player *target = shot.m_target;
	if (!target)
		return;

	// not gonna bother anymore.
	if (!target->alive())
		return;

	AimPlayer *data = &g_aimbot.m_players[target->index() - 1];
	if (!data)
		return;

	// this record was deleted already.
	if ( !shot.m_record->m_bones || !shot.m_record->m_setup || !shot.m_record->m_player )
		return;

	// we are going to alter this player.
	// store all his og data.
	BackupRecord backup;
	backup.store( target );

	// write historical matrix of the time that we shot
	// into the games bone cache, so we can trace against it.
	shot.m_record->cache( );

	// start position of trace is where we took the shot.
	start = shot.m_pos;

	// where our shot landed at.
	pos = shot.m_impact_pos;

	// the impact pos contains the spread from the server
	// which is generated with the server seed, so this is where the bullet
	// actually went, compute the direction of this from where the shot landed
	// and from where we actually took the shot.
	dir = (pos - start).normalized();

	// get end pos by extending direction forward.
	end = start + (dir * shot.m_range);

	bool resolver_issue = g_aimbot.CanHitPlayer(shot.m_record, start, end, shot.m_hitbox);

	if (resolver_issue)
	{
		size_t mode = shot.m_record->m_mode;

		// if we miss a shot on body update.
		// we can chose to stop shooting at them.
		// if we miss a shot on body update.
		// we can chose to stop shooting at them.
		switch (mode)
		{
		case Resolver::Modes::RESOLVE_WALK:
			++data->m_moving_index;
			break;
		case Resolver::Modes::RESOLVE_BODY:
			++data->m_body_index;
			break;
		case Resolver::Modes::RESOLVE_BODY_PRED:
			++data->m_body_index;
			break;
		case Resolver::Modes::RESOLVE_STAND2:
			++data->m_stand_index2;
			break;
		case Resolver::Modes::RESOLVE_STAND1:
			++data->m_stand_index1;
			break;
		case Resolver::Modes::RESOLVE_STAND3:
			++data->m_stand_index3;
			break;
		case Resolver::Modes::RESOLVE_STAND4:
			++data->m_stand_index4;
			break;
		case Resolver::Modes::RESOLVE_REVERSEFS:
			++data->m_reversefs_index;
			break;
		case Resolver::Modes::RESOLVE_EDGE:
			++data->m_edge_index;
			break;
		case Resolver::Modes::RESOLVE_LBY:
			++data->m_lby_index;
			break;
		case Resolver::Modes::RESOLVE_BACK:
			++data->m_back_index;
			break;
		case Resolver::Modes::RESOLVE_LASTMOVE:
			++data->m_lastmove_index;
			break;
		case Resolver::Modes::RESOLVE_SIDE_LASTMOVE:
			++data->m_sidelast_index;
			break;
		case Resolver::Modes::RESOLVE_AIR_TEST:
			++data->m_airlby_index;
			break;
		case Resolver::Modes::RESOLVE_AIR:
			++data->m_air_index;
			break;
		case Resolver::Modes::RESOLVE_LOW_LBY:
			++data->m_lowlby_index;
			break;
		case Resolver::Modes::RESOLVE_LOGIC:
			++data->m_logic_index;
			break;
		case Resolver::Modes::RESOLVE_FAKEWALK:
			++data->m_fakewalk_index;
			break;
		case Resolver::Modes::RESOLVE_TEST_FS:
			++data->m_test_index;
			break;
		}

		++data->m_missed_shots;

		if (shot.m_record->m_mode == Resolver::Modes::RESOLVE_WALK)
			g_notify.add(XOR("missed shot due to animations\n"));
		else
			g_notify.add(tfm::format(XOR("missed shot due to fake angles (%s)\n"), shot.m_record->m_mode));
	}
	else if (!trace.m_entity || !trace.m_entity->IsPlayer() || trace.m_entity != target)
		g_notify.add(XOR("missed shot due to spread\n"));
	else
		g_notify.add(XOR("missed shot due to unknown\n"));

	// restore player to his original state.
	backup.restore( target );
}

void Shots::Think() {
	if (!g_cl.m_processing || m_shots.empty()) {
		// we're dead, we won't need this data anymore.
		if (!m_shots.empty())
			m_shots.clear();

		// we don't handle shots if we're dead or if there are none to handle.
		return;
	}

	// iterate all shots.
	for (auto it = m_shots.begin(); it != m_shots.end(); ) {
		// too much time has passed, we don't need this anymore.
		if (it->m_time + 1.f < g_csgo.m_globals->m_realtime)
			// remove it.
			it = m_shots.erase(it);
		else
			it = next(it);
	}

	// iterate all shots.
	for (auto it = m_shots.begin(); it != m_shots.end(); ) {
		// our shot impacted, and it was confirmed, but we didn't damage anyone. we missed.
		if (it->m_impacted && it->m_confirmed && !it->m_hurt) {
			// handle the shot.
			OnShotMiss(*it);

			// since we've handled this shot, we won't need it anymore.
			it = m_shots.erase(it);
		}
		else
			it = next(it);
	}
}