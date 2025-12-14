#include "includes.h"
Resolver g_resolver{};;

void Resolver::FindBestAngle( Player* player, LagRecord* record )
{
	AimPlayer* data = &g_aimbot.m_players[player->index( ) - 1];

	if ( !data || !player )
		return;

	// constants.
	constexpr float STEP{4.f};
	constexpr float RANGE{32.f};

	// get the away angle for this record.
	float away = GetAwayAngle( record );

	vec3_t enemy_eyepos = player->wpn_shoot_pos( );

	// construct vector of angles to test.
	std::vector< AdaptiveAngle > angles{ };
	angles.emplace_back( away + 90.f );
	angles.emplace_back( away - 90.f );

	// start the trace at the enemy shoot pos.
	vec3_t start = g_cl.m_shoot_pos;

	// see if we got any valid result.
	// if this is false the path was not obstructed with anything.
	bool valid{false};

	// iterate vector of angles.
	for ( auto it = angles.begin( ); it != angles.end( ); ++it )
	{

		// compute the 'rough' estimation of where our head will be.
		vec3_t end{enemy_eyepos.x + std::cos( math::deg_to_rad( it->m_yaw ) ) * RANGE,
			enemy_eyepos.y + std::sin( math::deg_to_rad( it->m_yaw ) ) * RANGE,
			enemy_eyepos.z};

		// draw a line for debugging purposes.
		//g_csgo.m_debug_overlay->AddLineOverlay( start, end, 255, 0, 0, true, 0.1f );

		// compute the direction.
		vec3_t dir = end - start;
		float len = dir.normalize( );

		// should never happen.
		if ( len <= 0.f )
			continue;

		// step thru the total distance, 4 units per step.
		for ( float i{0.f}; i < len; i += STEP )
		{
			// get the current step position.
			vec3_t point = start + (dir * i);

			// get the contents at this point.
			int contents = g_csgo.m_engine_trace->GetPointContents( point, MASK_SHOT_HULL );

			// contains nothing that can stop a bullet.
			if ( !(contents & MASK_SHOT_HULL) )
				continue;

			float mult = 1.f;

			// over 50% of the total length, prioritize this shit.
			if ( i > (len * 0.5f) )
				mult = 1.25f;

			// over 90% of the total length, prioritize this shit.
			if ( i > (len * 0.75f) )
				mult = 1.5f;

			// over 90% of the total length, prioritize this shit.
			if ( i > (len * 0.9f) )
				mult = 2.f;

			// append 'penetrated distance'.
			it->m_dist += (STEP * mult);

			// mark that we found anything.
			valid = true;
		}
	}

	if ( !valid )
	{
		data->freestand_data = false;
		data->m_anti_fs_angle = away + 180.f;
		return;
	}

	// put the most distance at the front of the container.
	std::sort( angles.begin( ), angles.end( ),
			   [] ( const AdaptiveAngle& a, const AdaptiveAngle& b )
	{
		return a.m_dist > b.m_dist;
	} );

	// the best angle should be at the front now.
	AdaptiveAngle* best = &angles.front( );

	data->m_anti_fs_angle = math::NormalizedAngle( best->m_yaw );
	data->freestand_data = true;
}

LagRecord* Resolver::FindIdealRecord( AimPlayer* data )
{
	LagRecord* first_valid, * current;

	if ( data->m_records.empty( ) )
		return nullptr;

	first_valid = nullptr;

	// iterate records.
	for ( const auto& it : data->m_records )
	{
		if ( it->dormant( ) || it->immune( ) || !it->valid( ) || it->brokelc( ) || !it->m_setup )
			continue;

		// get current record.
		current = it.get( );

		// first record that was valid, store it for later.
		if ( !first_valid )
			first_valid = current;

		// try to find a record with a shot, lby update, walking or no anti-aim.
		if ( it->m_shot || it->m_mode == Modes::RESOLVE_BODY || it->m_mode == Modes::RESOLVE_BODY_PRED || it->m_mode == Modes::RESOLVE_LBY || it->m_mode == Modes::RESOLVE_WALK || it->m_mode == Modes::RESOLVE_NONE )
			return current;
	}

	// none found above, return the first valid record if possible.
	return (first_valid) ? first_valid : nullptr;
}

LagRecord* Resolver::FindLastRecord( AimPlayer* data )
{
	LagRecord* current;

	if ( data->m_records.empty( ) )
		return nullptr;

	// iterate records in reverse.
	for ( auto it = data->m_records.crbegin( ); it != data->m_records.crend( ); ++it )
	{
		current = it->get( );

		// if this record is valid.
		// we are done since we iterated in reverse.
		if ( current->valid( ) && !current->immune( ) && !current->dormant( ) && !current->brokelc( ) && current->m_setup )
			return current;
	}

	return nullptr;
}

void Resolver::OnBodyUpdate( Player* player, float value )
{
	AimPlayer* data = &g_aimbot.m_players[player->index( ) - 1];

	if ( !data )
		return;

	// set data.
	data->m_old_body = data->m_body;
	data->m_body = value;
}

float Resolver::GetAwayAngle( LagRecord* record )
{
	vec3_t pos;
	ang_t  away;

	math::VectorAngles( g_cl.m_local->m_vecOrigin( ) - record->m_pred_origin, away );
	return away.y;
}

void Resolver::MatchShot( AimPlayer* data, LagRecord* record )
{
	// do not attempt to do this in nospread mode.
	if ( g_menu.main.config.mode.get( ) == 1 )
		return;

	Weapon* weapon = data->m_player->GetActiveWeapon( );
	if ( !weapon )
		return;

	// ty kaaba
	const auto simulation_ticks = game::TIME_TO_TICKS( record->m_sim_time );
	const auto old_simulation_ticks = game::TIME_TO_TICKS( record->m_old_sim_time );

	if ( weapon && !weapon->IsKnife( ) && !weapon->IsGrenade( ) )
	{
		const auto& shot_tick = game::TIME_TO_TICKS( weapon->m_fLastShotTime( ) );

		if ( shot_tick > old_simulation_ticks && simulation_ticks >= shot_tick )
		{
			record->m_shot = true;
			//m_pCvar->ConsoleColorPrintf(Color::Red(), "shot fired! [%s]:[%i]\n", (pNewAnimRecord->m_AnimationFlags & ELagRecordFlags::RF_ShotNoPSilent ? "+" : "-"), TIME_TO_TICKS(m_flSimulationTime) - shot_tick);
		} else
		{
			if ( abs( simulation_ticks - shot_tick ) > record->m_lag + 2 )
			{
				data->m_valid_pitch = record->m_eye_angles.x;
			}
		}
	}

	if ( record->m_shot )
	{
		if ( record->m_lag > 1 )
		{
			if ( record->m_eye_angles.x < 70.0f )
			{
				if ( data->m_valid_pitch > 80.0f )
					record->m_eye_angles.x = data->m_valid_pitch;
			}
		}
	}

}

void Resolver::SetMode( LagRecord* record )
{
	// the resolver has 3 modes to chose from.
	// these modes will vary more under the hood depending on what data we have about the player
	// and what kind of hack vs. hack we are playing (mm/nospread).

	float speed = record->m_anim_velocity.length( );

	if ( g_input.GetKeyState( g_menu.main.aimbot.override.get( ) ) && record->m_flags & FL_ONGROUND && (speed <= 0.1f || record->m_fake_walk) )
		record->m_mode = Modes::RESOLVE_OVERRIDE;
	else if ( (record->m_flags & FL_ONGROUND) && speed > 0.1f && !record->m_fake_walk )
		record->m_mode = Modes::RESOLVE_WALK;
	else if ( (record->m_flags & FL_ONGROUND) && (speed <= 0.1f || record->m_fake_walk) )
		record->m_mode = Modes::RESOLVE_STAND;
	else if ( !(record->m_flags & FL_ONGROUND) )
		record->m_mode = Modes::RESOLVE_AIR;
}

void Resolver::ResolveAngles( Player* player, LagRecord* record )
{
	AimPlayer* data = &g_aimbot.m_players[player->index( ) - 1];

	if ( !data )
		return;

	// mark this record if it contains a shot.
	MatchShot( data, record );

	// next up mark this record with a resolver mode that will be used.
	SetMode( record );

	// run antifreestand
	FindBestAngle( player, record );

	// we arrived here we can do the acutal resolve.
	switch ( record->m_mode )
	{
		case Modes::RESOLVE_WALK:
			ResolveWalk( data, record, player );
			break;
		case Modes::RESOLVE_STAND:
			ResolveStand( data, record, player );
			break;
		case Modes::RESOLVE_AIR:
			ResolveAir( data, record, player );
			break;
		case Modes::RESOLVE_OVERRIDE:
			ResolveOverride( player, record, data );
			break;
	}

	// normalize the eye angles, doesn't really matter but its clean.
	math::NormalizeAngle( record->m_eye_angles.y );
}

void Resolver::ResolveWalk( AimPlayer* data, LagRecord* record, Player* player )
{
	// apply lby to eyeangles.
	record->m_eye_angles.y = record->m_body;

	data->m_body_update = record->m_anim_time + 0.22f;

	// reset stand and body index.
	if ( record->m_velocity.length_2d( ) > 25.f )
	{
		data->m_test_index = 0;
		data->m_fakewalk_index = 0;
		data->m_testfs_index = 0;
		data->m_logic_index = 0;
		data->m_airback_index = 0;
		data->m_stand_index4 = 0;
		data->m_air_index = 0;
		data->m_lowlby_index = 0;
		data->m_airlby_index = 0;
		data->m_spin_index = 0;
		data->m_stand_index1 = 0;
		data->m_stand_index2 = 0;
		data->m_stand_index3 = 0;
		data->m_edge_index = 0;
		data->m_fam_reverse_index = 0;
		data->m_back_index = 0;
		data->m_reversefs_index = 0;
		data->m_back_at_index = 0;
		data->m_reversefs_at_index = 0;
		data->m_lastmove_index = 0;
		data->m_lby_index = 0;
		data->m_airlast_index = 0;
		data->m_body_index = 0;
		data->m_sidelast_index = 0;
		data->m_moving_index = 0;

		// reset data about moving and lby
		// @ruka: lol this was never set to true anywhere
		// @ruka: remove that if it fucks up resolver somehow
		data->is_last_moving_lby_valid = true;
	}

	data->m_broke_lby = false;
	data->m_has_body_updated = false;

	// reset flick body
	data->m_flick_body = record->m_body;
	data->m_lbyticks = 0;

	// copy the last record that this player was walking
	// we need it later on because it gives us crucial data.
	std::memcpy( &data->m_walk_record, record, sizeof( LagRecord ) );
}

bool Resolver::IsYawSideways( Player* entity, float yaw )
{
	auto local_player = g_cl.m_local;
	if ( !local_player )
		return false;

	const auto at_target_yaw = math::CalcAngle( local_player->m_vecOrigin( ), entity->m_vecOrigin( ) ).y;
	const float delta = fabs( math::AngleDiff( at_target_yaw, yaw ) );

	return delta > 35.f && delta < 145.f;
}

void Resolver::ResolveStand( AimPlayer* data, LagRecord* record, Player* player )
{

	// for no-spread call a seperate resolver.
	if ( g_menu.main.config.mode.get( ) == 1 )
	{
		StandNS( data, record );
		return;
	}

	// get predicted away angle for the player.
	float away = GetAwayAngle( record );

	// pointer for easy access.
	LagRecord* move = &data->m_walk_record;
	LagRecord* previous = nullptr;

	if ( data->m_records.size( ) > 1 )
		previous = data->m_records[1].get( );

	// we have a valid moving record.
	if ( move->m_sim_time > 0.f )
	{
		vec3_t delta = move->m_origin - record->m_origin;

		// check if moving record is close.
		if ( delta.length( ) <= 128.f && !record->m_fake_walk )
		{
			// indicate that we are using the moving lby.
			data->m_moved = true;
		}
	}

	if ( record->m_fake_walk )
		data->is_last_moving_lby_valid = false;

	// @evitable: check for angle diff for more accuracy
	if ( previous && previous->m_body != record->m_body && data->m_body_index <= 3 )
	{
		record->m_eye_angles.y = record->m_body;
		data->m_body_update = record->m_anim_time + 1.1f;
		data->m_lbyticks++;
		data->m_has_body_updated = true;
		data->m_flick_body = record->m_body;
		record->m_mode = Modes::RESOLVE_BODY;
		return;
	}

	else if ( data->m_has_body_updated && (record->m_anim_time >= data->m_body_update) && data->m_body_index <= 3 )
	{
		record->m_eye_angles.y = record->m_body;
		data->m_body_update = record->m_anim_time + 1.1f;
		data->m_lbyticks++;
		data->m_flick_body = record->m_body;
		record->m_mode = Modes::RESOLVE_BODY_PRED;
		return;
	}

	bool above_120 = record->m_player->GetSequenceActivity( record->m_layers[3].m_sequence ) == 979;

	if ( data->m_lbyticks == 0 && data->m_lby_index <= 0 && !above_120 && fabsf( math::AngleDiff( data->m_old_body, record->m_body ) ) <= 25.f )
	{
		record->m_mode = Modes::RESOLVE_LBY;
		record->m_eye_angles.y = record->m_body;
		return;
	}

	if ( !data->m_moved )
	{
		record->m_mode = Modes::RESOLVE_STAND3;

		switch ( data->m_stand_index3 % 7 )
		{
			case 0:
				if ( fabsf( math::AngleDiff( record->m_body, data->m_flick_body ) ) <= 35.f )
					record->m_eye_angles.y = record->m_body;
				else
					record->m_eye_angles.y = away + 180.f;
				break;
			case 1:
				record->m_eye_angles.y = data->m_anti_fs_angle;
				break;
			case 2:
				record->m_eye_angles.y = away - 70.f;
				break;
			case 3:
				record->m_eye_angles.y = away + 70.f;
				break;
			case 4:
				record->m_eye_angles.y = away + 120.f;
				break;
			case 5:
				record->m_eye_angles.y = away - 120.f;
				break;
			case 6:
				record->m_eye_angles.y = away;
				break;
		}
	} else
	{
		const float flMoveDelta = fabsf( math::AngleDiff( move->m_body, record->m_body ) );

		bool is_sideways = IsYawSideways( player, move->m_body );
		bool is_backwards = !is_sideways;
		ang_t edge;

		// @evitable:
		// logic behind this should work
		// you need atleast 36 degrees difference to break lby and if your lastmove and lby flick body is <= 35 degrees away then go for lastmove =D
		if ( data->m_lowlby_index < 1 && fabsf( math::AngleDiff( move->m_body, data->m_flick_body ) ) <= 35.f )
		{
			record->m_mode = Modes::RESOLVE_LOW_LBY;
			record->m_eye_angles.y = move->m_body;
			return;
		}

		if ( data->m_sidelast_index < 1 && IsYawSideways( player, move->m_body ) && flMoveDelta < 12.5f && (data->is_last_moving_lby_valid && fabsf( math::AngleDiff( record->m_body, data->m_flick_body ) ) < 12.5f) )
		{
			record->m_mode = Modes::RESOLVE_SIDE_LASTMOVE;
			record->m_eye_angles.y = move->m_body;
		} else if ( data->m_reversefs_index < 1 && (is_sideways || (is_sideways && fabsf( math::AngleDiff( data->m_anti_fs_angle, data->m_flick_body ) ) <= 90.f)) && data->freestand_data )
		{
			record->m_mode = Modes::RESOLVE_REVERSEFS;
			record->m_eye_angles.y = data->m_anti_fs_angle;
		} else if ( data->m_lastmove_index < 1 && flMoveDelta < 15.f && (data->is_last_moving_lby_valid && fabsf( math::AngleDiff( record->m_body, data->m_flick_body ) ) < 15.f) )
		{
			record->m_mode = Modes::RESOLVE_LASTMOVE;
			record->m_eye_angles.y = move->m_body;
		} else if ( data->m_back_index < 1 && (is_backwards || !data->freestand_data || (is_backwards && fabsf( math::AngleDiff( record->m_body, data->m_flick_body ) ) <= 35.f)) )
		{
			record->m_mode = Modes::RESOLVE_BACK;
			record->m_eye_angles.y = away + 180.f;
		} else if ( data->m_lastmove_index >= 1 || data->m_back_index >= 1 && is_backwards )
		{
			record->m_mode = Modes::RESOLVE_STAND2;
			switch ( data->m_stand_index2 % 3 )
			{
				case 0:
					record->m_eye_angles.y = data->m_flick_body;
					break;
				case 1:
					record->m_eye_angles.y = away + 120.f;
					break;
				case 2:
					record->m_eye_angles.y = away - 120.f;
					break;
			}
		} else if ( data->m_reversefs_index >= 1 || data->m_sidelast_index >= 1 && is_sideways )
		{
			record->m_mode = Modes::RESOLVE_STAND1;
			switch ( data->m_stand_index1 % 8 )
			{
				case 0:
					record->m_eye_angles.y = -data->m_anti_fs_angle;
					break;
				case 1:
					record->m_eye_angles.y = +data->m_anti_fs_angle;
					break;
				case 2:
					record->m_eye_angles.y = away;
					break;
				case 3:
					record->m_eye_angles.y = away + 180.f;
					break;
				case 4:
					record->m_eye_angles.y = away + 90.f;
					break;
				case 5:
					record->m_eye_angles.y = away - 90.f;
					break;
				case 6:
					record->m_eye_angles.y = away + 110.f;
					break;
				case 7:
					record->m_eye_angles.y = away - 110.f;
					break;
				default:
					break;
			}
		}
	}
}

void Resolver::StandNS( AimPlayer* data, LagRecord* record )
{
	// get away angles.
	float away = GetAwayAngle( record );

	switch ( data->m_shots % 8 )
	{
		case 0:
			record->m_eye_angles.y = away + 180.f;
			break;

		case 1:
			record->m_eye_angles.y = away + 90.f;
			break;
		case 2:
			record->m_eye_angles.y = away - 90.f;
			break;

		case 3:
			record->m_eye_angles.y = away + 45.f;
			break;
		case 4:
			record->m_eye_angles.y = away - 45.f;
			break;

		case 5:
			record->m_eye_angles.y = away + 135.f;
			break;
		case 6:
			record->m_eye_angles.y = away - 135.f;
			break;

		case 7:
			record->m_eye_angles.y = away + 0.f;
			break;

		default:
			break;
	}

	// force LBY to not fuck any pose and do a true bruteforce.
	record->m_body = record->m_eye_angles.y;
}

void Resolver::ResolveAir( AimPlayer* data, LagRecord* record, Player* player )
{
	// for no-spread call a seperate resolver.
	if ( g_menu.main.config.mode.get( ) == 1 )
	{
		AirNS( data, record );
		return;
	}

	float away = GetAwayAngle( record );

	// reset those stuff
	data->is_last_moving_lby_valid = false;
	data->m_moved = false;

	LagRecord* move = &data->m_walk_record;

	switch ( data->m_air_index % 9 )
	{
		case 0:
			record->m_eye_angles.y = record->m_body;
			break;
		case 1:
			if ( move && fabs( math::AngleDiff( record->m_body, move->m_body ) ) > 60.f )
				record->m_eye_angles.y = move->m_body;
			else
				record->m_eye_angles.y = away + 180.f;
			break;
		case 2:
			record->m_eye_angles.y = away - 150.f;
			break;
		case 3:
			record->m_eye_angles.y = away + 165.f;
			break;
		case 4:
			record->m_eye_angles.y = away - 165.f;
			break;
		case 5:
			record->m_eye_angles.y = away + 135.f;
			break;
		case 6:
			record->m_eye_angles.y = away - 135.f;
			break;
		case 7:
			record->m_eye_angles.y = away + 90.f;
			break;
		case 8:
			record->m_eye_angles.y = away - 90.f;
			break;
	}
}

void Resolver::AirNS( AimPlayer* data, LagRecord* record )
{
	// get away angles.
	float away = GetAwayAngle( record );

	switch ( data->m_shots % 9 )
	{
		case 0:
			record->m_eye_angles.y = away + 180.f;
			break;

		case 1:
			record->m_eye_angles.y = away + 150.f;
			break;
		case 2:
			record->m_eye_angles.y = away - 150.f;
			break;

		case 3:
			record->m_eye_angles.y = away + 165.f;
			break;
		case 4:
			record->m_eye_angles.y = away - 165.f;
			break;

		case 5:
			record->m_eye_angles.y = away + 135.f;
			break;
		case 6:
			record->m_eye_angles.y = away - 135.f;
			break;

		case 7:
			record->m_eye_angles.y = away + 90.f;
			break;
		case 8:
			record->m_eye_angles.y = away - 90.f;
			break;

		default:
			break;
	}
}

void Resolver::ResolvePoses( Player* player, LagRecord* record )
{
	AimPlayer* data = &g_aimbot.m_players[player->index( ) - 1];

	// only do this bs when in air.
	if ( record->m_mode == Modes::RESOLVE_AIR )
	{
		// ang = pose min + pose val x ( pose range )

		// lean_yaw
		player->m_flPoseParameter( )[2] = g_csgo.RandomInt( 0, 4 ) * 0.25f;

		// body_yaw
		player->m_flPoseParameter( )[11] = g_csgo.RandomInt( 1, 3 ) * 0.25f;
	}
}


void Resolver::ResolveOverride( Player* player, LagRecord* record, AimPlayer* data )
{

	// get predicted away angle for the player.
	float away = GetAwayAngle( record );

	// pointer for easy access.
	LagRecord* move = &data->m_walk_record;

	C_AnimationLayer* curr = &record->m_layers[3];
	int act = data->m_player->GetSequenceActivity( curr->m_sequence );

	if ( g_input.GetKeyState( g_menu.main.aimbot.override.get( ) ) )
	{
		ang_t                          viewangles;
		g_csgo.m_engine->GetViewAngles( viewangles );

		//auto yaw = math::clamp (g_cl.m_local->GetAbsOrigin(), Player->origin()).y;
		const float at_target_yaw = math::CalcAngle( g_cl.m_local->m_vecOrigin( ), player->m_vecOrigin( ) ).y;

		if ( fabs( math::NormalizedAngle( viewangles.y - at_target_yaw ) ) > 30.f )
			return ResolveStand( data, record, player );

		record->m_eye_angles.y = (math::NormalizedAngle( viewangles.y - at_target_yaw ) > 0) ? at_target_yaw + 90.f : at_target_yaw - 90.f;

		//return UTILS::GetLBYRotatedYaw(entity->m_flLowerBodyYawTarget(), (math::NormalizedAngle(viewangles.y - at_target_yaw) > 0) ? at_target_yaw + 90.f : at_target_yaw - 90.f);

		record->m_mode = Modes::RESOLVE_OVERRIDE;
	}

	bool did_lby_flick{false};

	if ( data->m_body != data->m_old_body )
	{
		record->m_eye_angles.y = record->m_body;

		data->m_body_update = record->m_anim_time + 1.1f;

		iPlayers[record->m_player->index( )] = false;
		record->m_mode = Modes::RESOLVE_BODY;
	} else
	{
		// LBY SHOULD HAVE UPDATED HERE.
		if ( record->m_anim_time >= data->m_body_update )
		{
			// only shoot the LBY flick 3 times.
			// if we happen to miss then we most likely mispredicted
			if ( data->m_body_index < 1 )
			{
				// set angles to current LBY.
				record->m_eye_angles.y = record->m_body;

				data->m_body_update = record->m_anim_time + 1.1f;

				// set the resolve mode.
				iPlayers[record->m_player->index( )] = false;
				record->m_mode = Modes::RESOLVE_BODY;
			}
		}
	}
}