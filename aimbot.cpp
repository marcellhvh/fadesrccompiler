#include "includes.h"

Aimbot g_aimbot{ };;

void Aimbot::update_shoot_pos( )
{
	if ( !g_cl.m_local || !g_cl.m_processing )
		return;

	const auto anim_state = g_cl.m_local->m_PlayerAnimState( );
	if ( !anim_state )
		return;

	const auto backup = g_cl.m_local->m_flPoseParameter( )[12];
	const auto originbackup = g_cl.m_local->GetAbsOrigin( );

	// set pitch, rotation etc
	g_cl.m_local->SetAbsOrigin( g_cl.m_local->m_vecOrigin( ) );
	g_cl.m_local->m_flPoseParameter( )[12] = 0.5f;

	// update shoot pos
	g_cl.m_shoot_pos = g_cl.m_local->GetShootPosition( );

	// set to backup
	g_cl.m_local->SetAbsOrigin( originbackup );
	g_cl.m_local->m_flPoseParameter( )[12] = backup;
}

void FixVelocity( Player* m_player, LagRecord* record, LagRecord* previous, float max_speed )
{
	if ( !previous )
	{
		if ( record->m_layers[6].m_playback_rate > 0.0f && record->m_layers[6].m_weight != 0.f && record->m_velocity.length( ) > 0.1f )
		{
			auto v30 = max_speed;

			if ( record->m_flags & 6 )
				v30 *= 0.34f;
			else if ( m_player->m_bIsWalking( ) )
				v30 *= 0.52f;

			auto v35 = record->m_layers[6].m_weight * v30;
			record->m_velocity *= v35 / record->m_velocity.length( );
		} else
			record->m_velocity.clear( );

		if ( record->m_flags & 1 )
			record->m_velocity.z = 0.f;

		record->m_anim_velocity = record->m_velocity;
		return;
	}

	if ( (m_player->m_fEffects( ) & 8) != 0
		 || m_player->m_ubEFNoInterpParity( ) != m_player->m_ubEFNoInterpParityOld( ) )
	{
		record->m_velocity.clear( );
		record->m_anim_velocity.clear( );
		return;
	}

	auto is_jumping = !(record->m_flags & FL_ONGROUND && previous->m_flags & FL_ONGROUND);

	if ( record->m_lag > 1 )
	{
		record->m_velocity.clear( );
		auto origin_delta = (record->m_origin - previous->m_origin);

		if ( !(previous->m_flags & FL_ONGROUND || record->m_flags & FL_ONGROUND) )// if not previous on ground or on ground
		{
			auto currently_ducking = record->m_flags & FL_DUCKING;
			if ( (previous->m_flags & FL_DUCKING) != currently_ducking )
			{
				float duck_modifier = 0.f;

				if ( currently_ducking )
					duck_modifier = 9.f;
				else
					duck_modifier = -9.f;

				origin_delta.z -= duck_modifier;
			}
		}

		auto sqrt_delta = origin_delta.length_2d_sqr( );

		if ( sqrt_delta > 0.f && sqrt_delta < 1000000.f )
			record->m_velocity = origin_delta / game::TICKS_TO_TIME( record->m_lag );

		record->m_velocity.validate_vec( );

		if ( is_jumping )
		{
			if ( record->m_flags & FL_ONGROUND && !g_csgo.sv_enablebunnyhopping->GetInt( ) )
			{

				// 260 x 1.1 = 286 units/s.
				float max = m_player->m_flMaxspeed( ) * 1.1f;

				// get current velocity.
				float speed = record->m_velocity.length( );

				// reset velocity to 286 units/s.
				if ( max > 0.f && speed > max )
					record->m_velocity *= (max / speed);
			}

			// assume the player is bunnyhopping here so set the upwards impulse.
			record->m_velocity.z = g_csgo.sv_jump_impulse->GetFloat( );
		}
		// we are not on the ground
		// apply gravity and airaccel.
		else if ( !(record->m_flags & FL_ONGROUND) )
		{
			// apply one tick of gravity.
			record->m_velocity.z -= g_csgo.sv_gravity->GetFloat( ) * g_csgo.m_globals->m_interval;
		}
	}

	if ( record->m_velocity.length( ) > 1.f
		 && record->m_lag >= 12
		 && record->m_layers[12].m_weight == 0.0f
		 && record->m_layers[6].m_weight == 0.0f
		 && record->m_layers[6].m_playback_rate < 0.0001f
		 && (record->m_flags & FL_ONGROUND) )
		record->m_fake_walk = true;

	record->m_anim_velocity = record->m_velocity;

	if ( !record->m_fake_walk )
	{
		if ( record->m_anim_velocity.length_2d( ) > 0 && (record->m_flags & FL_ONGROUND) )
		{
			float anim_speed = 0.f;

			if ( !is_jumping
				 && record->m_layers[11].m_weight > 0.0f
				 && record->m_layers[11].m_weight < 1.0f
				 && record->m_layers[11].m_playback_rate == previous->m_layers[11].m_playback_rate )
			{
				// calculate animation speed yielded by anim overlays
				auto flAnimModifier = 0.35f * (1.0f - record->m_layers[11].m_weight);
				if ( flAnimModifier > 0.0f && flAnimModifier < 1.0f )
					anim_speed = max_speed * (flAnimModifier + 0.55f);
			}

			// this velocity is valid ONLY IN ANIMFIX UPDATE TICK!!!
			// so don't store it to record as m_vecVelocity
			// -L3D451R7
			if ( anim_speed > 0.0f )
			{
				anim_speed /= record->m_anim_velocity.length_2d( );
				record->m_anim_velocity.x *= anim_speed;
				record->m_anim_velocity.y *= anim_speed;
			}
		}
	} else
		record->m_anim_velocity.clear( );

	record->m_anim_velocity.validate_vec( );
}

void AimPlayer::UpdateAnimations( LagRecord* record )
{
	CCSGOPlayerAnimState* state = this->m_player->m_PlayerAnimState( );
	if ( !state || this->m_player != record->m_player )
		return;

	// player respawned.
	if ( this->m_player->m_flSpawnTime( ) != this->m_spawn )
	{
		// reset animation state.
		game::ResetAnimationState( state );

		// note new spawn time.
		this->m_spawn = this->m_player->m_flSpawnTime( );
	}

	// s/o onetap
	const float m_flRealtime = g_csgo.m_globals->m_realtime;
	const float m_flCurtime = g_csgo.m_globals->m_curtime;
	const float m_flFrametime = g_csgo.m_globals->m_frametime;
	const float m_flAbsFrametime = g_csgo.m_globals->m_abs_frametime;
	const int m_iFramecount = g_csgo.m_globals->m_frame;
	const int m_iTickcount = g_csgo.m_globals->m_tick_count;
	const float interpolation = g_csgo.m_globals->m_interp_amt;

	// @ruka: simtime & player simtime are the same btw
	g_csgo.m_globals->m_realtime = record->m_sim_time;
	g_csgo.m_globals->m_curtime = record->m_sim_time;
	g_csgo.m_globals->m_frametime = g_csgo.m_globals->m_interval;
	g_csgo.m_globals->m_abs_frametime = g_csgo.m_globals->m_interval;
	g_csgo.m_globals->m_frame = game::TIME_TO_TICKS( record->m_sim_time );
	g_csgo.m_globals->m_tick_count = game::TIME_TO_TICKS( record->m_sim_time );
	g_csgo.m_globals->m_interp_amt = 0.f;

	// backup stuff that we do not want to fuck with.
	AnimationBackup_t backup;

	backup.m_origin = this->m_player->m_vecOrigin( );
	backup.m_abs_origin = this->m_player->GetAbsOrigin( );
	backup.m_velocity = this->m_player->m_vecVelocity( );
	backup.m_abs_velocity = this->m_player->m_vecAbsVelocity( );
	backup.m_flags = this->m_player->m_fFlags( );
	backup.m_eflags = this->m_player->m_iEFlags( );
	backup.m_duck = this->m_player->m_flDuckAmount( );
	backup.m_body = this->m_player->m_flLowerBodyYawTarget( );
	this->m_player->GetAnimLayers( backup.m_layers );

	LagRecord* previous = nullptr;
	LagRecord* pre_previous = nullptr;

	if ( m_records.size( ) > 1 )
		previous = this->m_records[1].get( )->dormant( ) ? nullptr : this->m_records[1].get( );

	if ( m_records.size( ) > 2 )
		pre_previous = this->m_records[2].get( )->dormant( ) ? nullptr : this->m_records[2].get( );

	// is player a bot?
	bool bot = game::IsFakePlayer( this->m_player->index( ) );

	// reset fakewalk state.
	record->m_fake_walk = false;
	record->m_broke_lc = false;
	record->max_speed = 260.f;
	record->m_mode = Resolver::Modes::RESOLVE_NONE;

	auto m_weapon = this->m_player->GetActiveWeapon( );
	if ( m_weapon && !m_weapon->IsKnife( ) )
	{
		auto data = m_weapon->GetWpnData( );

		if ( data )
			record->max_speed = this->m_player->m_bIsScoped( ) ? data->m_max_player_speed_alt : data->m_max_player_speed;
	}

	// get network lag
	record->m_lag_time = this->m_player->m_flSimulationTime( ) - this->m_player->m_flOldSimulationTime( );
	record->m_sim_ticks = game::TIME_TO_TICKS( record->m_lag_time );

	// detect players breaking teleport distance
	// https://github.com/perilouswithadollarsign/cstrike15_src/blob/master/game/server/player_lagcompensation.cpp#L384-L388
	if ( record->brokelc( ) )
		record->m_broke_lc = true;

	// fix various issues with the game eW91dHViZS5jb20vZHlsYW5ob29r
	// these issues can only occur when a player is choking data.
	if ( record->m_lag >= 2 && !bot )
	{
		// detect fakewalk.
		float speed = record->m_velocity.length( );

		if ( record->m_fake_walk )
			record->m_velocity = record->m_anim_velocity = {};
		
		// we need atleast 2 updates/records
		// to fix these issues.
		if ( m_records.size( ) >= 2 )
		{
			// get pointer to previous record.
			LagRecord* previous = m_records[1].get( );

			if ( previous && !previous->dormant( ) )
			{
				bool bOnGround = record->m_flags & FL_ONGROUND;
				bool bJumped = false;
				bool bLandedOnServer = false;
				float flLandTime = 0.f;

				if ( record->m_layers[4].m_cycle < 0.5f && (!(record->m_flags & FL_ONGROUND) || !(previous->m_flags & FL_ONGROUND)) )
				{
					// note - VIO;
					// well i guess when llama wrote v3, he was drunk or sum cuz this is incorrect. -> cuz he changed this in v4.
					// and alpha didn't realize this but i did, so its fine.
					// improper way to do this -> flLandTime = record->m_flSimulationTime - float( record->m_serverAnimOverlays[ 4 ].m_flPlaybackRate * record->m_serverAnimOverlays[ 4 ].m_flCycle );
					// we need to divide instead of multiplication.
					flLandTime = record->m_sim_time - float( record->m_layers[4].m_playback_rate / record->m_layers[4].m_cycle );
					bLandedOnServer = flLandTime >= previous->m_sim_time;
				}

				if ( bLandedOnServer && !bJumped )
				{
					if ( flLandTime <= record->m_sim_time )
					{
						bJumped = true;
						bOnGround = true;
					} else
					{
						bOnGround = previous->m_flags & FL_ONGROUND;
					}
				}

				if ( bOnGround )
				{
					this->m_player->m_fFlags( ) |= FL_ONGROUND;
				} else
				{
					this->m_player->m_fFlags( ) &= ~FL_ONGROUND;
				}

				// delta in duckamt and delta in time..
				float duck = record->m_duck - previous->m_duck;
				float time = record->m_sim_time - previous->m_sim_time;

				// get the duckamt change per tick.
				float change = (duck / time) * g_csgo.m_globals->m_interval;

				// fix crouching players.
				this->m_player->m_flDuckAmount( ) = previous->m_duck + change;
			}
		}
	}

	// fix player's velocity
	FixVelocity( this->m_player, record, previous, record->max_speed );

	bool fake = !bot && g_menu.main.aimbot.correct.get( );

	// if using fake angles, correct angles.
	if ( fake )
		g_resolver.ResolveAngles( this->m_player, record );

	// set stuff before animating.
	this->m_player->m_vecOrigin( ) = record->m_origin;
	this->m_player->m_vecVelocity( ) = this->m_player->m_vecAbsVelocity( ) = record->m_anim_velocity;
	this->m_player->m_flLowerBodyYawTarget( ) = record->m_body;

	// EFL_DIRTY_ABSVELOCITY
	// skip call to C_BaseEntity::CalcAbsoluteVelocity
	this->m_player->m_iEFlags( ) &= ~(EFL_DIRTY_ABSVELOCITY | EFL_DIRTY_ABSTRANSFORM);

	// write potentially resolved angles.
	this->m_player->m_angEyeAngles( ) = record->m_eye_angles;

	// fix animating in same frame.
	if ( state->m_last_update_frame >= g_csgo.m_globals->m_frame )
		state->m_last_update_frame = g_csgo.m_globals->m_frame - 1;

	// 'm_animating' returns true if being called from SetupVelocity, passes raw velocity to animstate.
	this->m_player->m_bClientSideAnimation( ) = true;
	this->m_player->UpdateClientSideAnimation( );
	this->m_player->m_bClientSideAnimation( ) = false;

	// player animations have updated.
	this->m_player->InvalidatePhysicsRecursive( InvalidatePhysicsBits_t::ANIMATION_CHANGED );

	// store updated/animated poses and rotation in lagrecord.
	this->m_player->SetAnimLayers( record->m_layers );
	this->m_player->GetPoseParameters( record->m_poses );
	record->m_abs_ang = m_player->GetAbsAngles( );

	// setup bones
	g_bones.setup( this->m_player, nullptr, record );

	// restore backup data.
	this->m_player->m_vecOrigin( ) = backup.m_origin;
	this->m_player->m_vecVelocity( ) = backup.m_velocity;
	this->m_player->m_vecAbsVelocity( ) = backup.m_abs_velocity;
	this->m_player->m_fFlags( ) = backup.m_flags;
	this->m_player->m_iEFlags( ) = backup.m_eflags;
	this->m_player->m_flDuckAmount( ) = backup.m_duck;
	this->m_player->m_flLowerBodyYawTarget( ) = backup.m_body;
	this->m_player->SetAbsOrigin( backup.m_abs_origin );

	// restore globals.
	g_csgo.m_globals->m_realtime = m_flRealtime;
	g_csgo.m_globals->m_curtime = m_flCurtime;
	g_csgo.m_globals->m_frametime = m_flFrametime;
	g_csgo.m_globals->m_abs_frametime = m_flAbsFrametime;
	g_csgo.m_globals->m_frame = m_iFramecount;
	g_csgo.m_globals->m_tick_count = m_iTickcount;
	g_csgo.m_globals->m_interp_amt = interpolation;
}

void AimPlayer::OnNetUpdate( Player* player )
{
	bool reset = (!g_menu.main.aimbot.enable.get( ) || player->m_lifeState( ) == LIFE_DEAD || !player->enemy( g_cl.m_local ));

	// if this happens, delete all the lagrecords.
	if ( reset )
	{
		player->m_bClientSideAnimation( ) = true;
		this->m_records.clear( );

		this->m_has_body_updated = false;
		this->m_fam_reverse_index = 0;
		this->m_airback_index = 0;
		this->m_fakewalk_index = 0;
		this->m_testfs_index = 0;
		this->m_lowlby_index = 0;
		this->m_logic_index = 0;
		this->m_stand_index4 = 0;
		this->m_air_index = 0;
		this->m_airlby_index = 0;
		this->m_broke_lby = false;
		this->m_spin_index = 0;
		this->m_stand_index1 = 0;
		this->m_stand_index2 = 0;
		this->m_stand_index3 = 0;
		this->m_edge_index = 0;
		this->m_back_index = 0;
		this->m_reversefs_index = 0;
		this->m_back_at_index = 0;
		this->m_reversefs_at_index = 0;
		this->m_lastmove_index = 0;
		this->m_lby_index = 0;
		this->m_airlast_index = 0;
		this->m_body_index = 0;
		this->m_lbyticks = 0;
		this->m_sidelast_index = 0;
		this->m_moving_index = 0;

		return;
	}

	// update player ptr if required.
	// reset player if changed.
	if ( this->m_player != player )
		this->m_records.clear( );

	// update player ptr.
	this->m_player = player;

	// indicate that this player has been out of pvs.
	// insert dummy record to separate records
	// to fix stuff like animation and prediction.
	if ( player->dormant( ) )
	{
		bool insert = true;

		// we have any records already?
		if ( !this->m_records.empty( ) )
		{

			LagRecord* front = this->m_records.front( ).get( );

			// we already have a dormancy separator.
			if ( front->dormant( ) )
				insert = false;
		}

		if ( insert )
		{
			// add new record.
			this->m_records.emplace_front( std::make_shared< LagRecord >( player ) );

			// get reference to newly added record.
			LagRecord* current = m_records.front( ).get( );

			// mark as dormant.
			current->m_dormant = true;
		}

		// limit to 1 record since we set m_dormant to true
		while ( this->m_records.size( ) > 1 )
			this->m_records.pop_back( );

		return;
	}

	bool update = this->m_records.empty( )
		|| this->m_records.front( )->m_layers[11].m_cycle != player->m_AnimOverlay( )[11].m_cycle
		|| this->m_records.front( )->m_layers[11].m_playback_rate != player->m_AnimOverlay( )[11].m_playback_rate
		|| this->m_records.front( )->m_sim_time != player->m_flSimulationTime( )
		|| player->m_flSimulationTime( ) != player->m_flOldSimulationTime( )
		|| player->m_vecOrigin( ) != this->m_records.front( )->m_origin;

	// this is the first data update we are receving
	// OR we received data with a newer simulation context.
	if ( update )
	{
		// add new record.
		this->m_records.emplace_front( std::make_shared< LagRecord >( player ) );

		// get reference to newly added record.
		LagRecord* current = this->m_records.front( ).get( );

		// mark as non dormant.
		current->m_dormant = false;

		// update animations on current record.
		// call resolver.
		UpdateAnimations( current );

		// set shifting tickbase record.
		current->m_shift = game::TIME_TO_TICKS( current->m_sim_time ) - g_csgo.m_globals->m_tick_count;
	}

	// no need to store insane amt of data.
	while ( this->m_records.size( ) > 256 )
		this->m_records.pop_back( );

	// pop back records if they broke lc
	while ( this->m_records.size( ) > 1
			&& this->m_records.front( )->m_broke_lc )
		this->m_records.pop_back( );

	// @evitable: dont shoot at exploit records
	while ( this->m_records.size( ) > 1
			&& !this->m_records.front( )->m_player->dormant( )
			&& this->m_records.front( )->m_sim_time < this->m_records.front( )->m_old_sim_time )
		this->m_records.pop_back( );
}

void AimPlayer::OnRoundStart( Player* player )
{
	this->m_walk_record = {};
	this->m_player = player;
	this->m_shots = 0;
	this->m_missed_shots = 0;
	this->m_moved = false;
	this->m_body_update = FLT_MAX;
	this->m_test_index = 0;

	// reset stand and body index.
	this->m_has_body_updated = false;
	this->m_fam_reverse_index = 0;
	this->m_airback_index = 0;
	this->m_fakewalk_index = 0;
	this->m_testfs_index = 0;
	this->m_lowlby_index = 0;
	this->m_logic_index = 0;
	this->m_stand_index4 = 0;
	this->m_air_index = 0;
	this->m_airlby_index = 0;
	this->m_broke_lby = false;
	this->m_spin_index = 0;
	this->m_stand_index1 = 0;
	this->m_stand_index2 = 0;
	this->m_stand_index3 = 0;
	this->m_edge_index = 0;
	this->m_back_index = 0;
	this->m_reversefs_index = 0;
	this->m_back_at_index = 0;
	this->m_reversefs_at_index = 0;
	this->m_lastmove_index = 0;
	this->m_lby_index = 0;
	this->m_airlast_index = 0;
	this->m_body_index = 0;
	this->m_lbyticks = 0;
	this->m_sidelast_index = 0;
	this->m_moving_index = 0;

	this->m_records.clear( );
	this->m_hitboxes.clear( );

	// IMPORTANT: DO NOT CLEAR LAST HIT SHIT.
}

void AimPlayer::SetupHitboxes( LagRecord* record, bool history )
{
	// reset hitboxes.
	m_hitboxes.clear( );

	if ( g_cl.m_weapon_id == ZEUS )
	{
		// hitboxes for the zeus.
		m_hitboxes.push_back( {HITBOX_BODY, HitscanMode::NORMAL} );
		return;
	}

	bool prefer_head = record->m_velocity.length_2d( ) > 71.f;

	// prefer

	if ( g_menu.main.aimbot.head1.get( 0 ) )
		m_hitboxes.push_back( {HITBOX_HEAD, HitscanMode::PREFER} );

	if ( g_menu.main.aimbot.head1.get( 1 ) && prefer_head )
		m_hitboxes.push_back( {HITBOX_HEAD, HitscanMode::PREFER} );

	if ( g_menu.main.aimbot.head1.get( 2 ) && !(record->m_mode != Resolver::Modes::RESOLVE_NONE && record->m_mode != Resolver::Modes::RESOLVE_WALK && record->m_mode != Resolver::Modes::RESOLVE_BODY) )
		m_hitboxes.push_back( {HITBOX_HEAD, HitscanMode::PREFER} );

	if ( g_menu.main.aimbot.head1.get( 3 ) && !(record->m_pred_flags & FL_ONGROUND) )
		m_hitboxes.push_back( {HITBOX_HEAD, HitscanMode::PREFER} );

	// prefer, always.
	if ( g_menu.main.aimbot.baim1.get( 0 ) )
		m_hitboxes.push_back( {HITBOX_BODY, HitscanMode::PREFER} );

	// prefer, lethal.
	if ( g_menu.main.aimbot.baim1.get( 1 ) )
		m_hitboxes.push_back( {HITBOX_BODY, HitscanMode::LETHAL} );

	// prefer, lethal x2.
	if ( g_menu.main.aimbot.baim1.get( 2 ) )
		m_hitboxes.push_back( {HITBOX_BODY, HitscanMode::LETHAL2} );

	// prefer, fake.
	if ( g_menu.main.aimbot.baim1.get( 3 ) && record->m_mode != Resolver::Modes::RESOLVE_NONE && record->m_mode != Resolver::Modes::RESOLVE_WALK && record->m_mode != Resolver::Modes::RESOLVE_BODY )
		m_hitboxes.push_back( {HITBOX_BODY, HitscanMode::PREFER} );

	// prefer, in air.
	if ( g_menu.main.aimbot.baim1.get( 4 ) && !(record->m_pred_flags & FL_ONGROUND) )
		m_hitboxes.push_back( {HITBOX_BODY, HitscanMode::PREFER} );

	// prefer, always.
	if ( g_menu.main.aimbot.baim1.get( 5 ) && record->brokelc( ) )
		m_hitboxes.push_back( {HITBOX_BODY, HitscanMode::PREFER} );

	// prefer, always.
	if ( g_menu.main.aimbot.baim1.get( 6 ) && record->m_mode == Resolver::Modes::RESOLVE_STAND3 )
		m_hitboxes.push_back( {HITBOX_BODY, HitscanMode::PREFER} );

	bool only{false};

	// only, always.
	if ( g_menu.main.aimbot.baim2.get( 0 ) )
	{
		only = true;
		m_hitboxes.push_back( {HITBOX_BODY, HitscanMode::PREFER} );
	}

	// only, health.
	if ( g_menu.main.aimbot.baim2.get( 1 ) && m_player->m_iHealth( ) <= (int) g_menu.main.aimbot.baim_hp.get( ) )
	{
		only = true;
		m_hitboxes.push_back( {HITBOX_BODY, HitscanMode::PREFER} );
	}

	// only, fake.
	if ( g_menu.main.aimbot.baim2.get( 2 ) && record->m_mode != Resolver::Modes::RESOLVE_NONE && record->m_mode != Resolver::Modes::RESOLVE_WALK && record->m_mode != Resolver::Modes::RESOLVE_BODY )
	{
		only = true;
		m_hitboxes.push_back( {HITBOX_BODY, HitscanMode::PREFER} );
	}

	// only, in air.
	if ( g_menu.main.aimbot.baim2.get( 3 ) && !(record->m_pred_flags & FL_ONGROUND) )
	{
		only = true;
		m_hitboxes.push_back( {HITBOX_BODY, HitscanMode::PREFER} );
	}

	// only, on key.
	if ( g_input.GetKeyState( g_menu.main.aimbot.baim_key.get( ) ) )
	{
		only = true;
		m_hitboxes.push_back( {HITBOX_BODY, HitscanMode::PREFER} );
	}

	// only baim conditions have been met.
	// do not insert more hitboxes.
	if ( only )
		return;

	std::vector< size_t > hitbox{g_menu.main.aimbot.hitbox.GetActiveIndices( )};
	if ( hitbox.empty( ) )
		return;

	bool ignore_limbs = record->m_velocity.length_2d( ) > 71.f && g_menu.main.aimbot.ignor_limbs.get( );

	for ( const auto& h : hitbox )
	{
		// head.
		if ( h == 0 )
			m_hitboxes.push_back( {HITBOX_HEAD, HitscanMode::NORMAL} );

		// chest.
		if ( h == 1 )
		{
			m_hitboxes.push_back( {HITBOX_THORAX, HitscanMode::NORMAL} );
			m_hitboxes.push_back( {HITBOX_CHEST, HitscanMode::NORMAL} );
			m_hitboxes.push_back( {HITBOX_UPPER_CHEST, HitscanMode::NORMAL} );
		}

		// stomach.
		if ( h == 2 )
		{
			m_hitboxes.push_back( {HITBOX_PELVIS, HitscanMode::NORMAL} );
			m_hitboxes.push_back( {HITBOX_BODY, HitscanMode::NORMAL} );
		}

		// arms.
		if ( h == 3 && !ignore_limbs )
		{
			m_hitboxes.push_back( {HITBOX_L_UPPER_ARM, HitscanMode::NORMAL} );
			m_hitboxes.push_back( {HITBOX_R_UPPER_ARM, HitscanMode::NORMAL} );
		}

		// legs.
		if ( h == 4 )
		{
			m_hitboxes.push_back( {HITBOX_L_THIGH, HitscanMode::NORMAL} );
			m_hitboxes.push_back( {HITBOX_R_THIGH, HitscanMode::NORMAL} );
			m_hitboxes.push_back( {HITBOX_L_CALF, HitscanMode::NORMAL} );
			m_hitboxes.push_back( {HITBOX_R_CALF, HitscanMode::NORMAL} );
		}

		// foot.
		if ( h == 5 && !ignore_limbs )
		{
			m_hitboxes.push_back( {HITBOX_L_FOOT, HitscanMode::NORMAL} );
			m_hitboxes.push_back( {HITBOX_R_FOOT, HitscanMode::NORMAL} );
		}
	}
}

void Aimbot::init( )
{
	// clear old targets.
	m_targets.clear( );

	m_target = nullptr;
	m_aim = vec3_t{ };
	m_angle = ang_t{ };
	m_damage = 0.f;
	m_record = nullptr;
	m_stop = false;

	m_best_dist = std::numeric_limits< float >::max( );
	m_best_fov = 180.f + 1.f;
	m_best_damage = 0.f;
	m_best_hp = 100 + 1;
	m_best_lag = std::numeric_limits< float >::max( );
	m_best_height = std::numeric_limits< float >::max( );
}

void Aimbot::StripAttack( )
{
	if ( g_cl.m_weapon_id == REVOLVER )
		g_cl.m_cmd->m_buttons &= ~IN_ATTACK2;

	else
		g_cl.m_cmd->m_buttons &= ~IN_ATTACK;
}

void Aimbot::think( )
{
	// do all startup routines.
	init( );

	// sanity.
	if ( !g_cl.m_weapon )
		return;

	// no grenades or bomb.
	if ( g_cl.m_weapon_type == WEAPONTYPE_GRENADE || g_cl.m_weapon_type == WEAPONTYPE_C4 )
		return;

	if ( !g_cl.m_weapon_fire )
		StripAttack( );

	// we have no aimbot enabled.
	if ( !g_menu.main.aimbot.enable.get( ) )
		return;

	// animation silent aim, prevent the ticks with the shot in it to become the tick that gets processed.
	// we can do this by always choking the tick before we are able to shoot.
	bool revolver = g_cl.m_weapon_id == REVOLVER && g_cl.m_revolver_cock != 0;

	// one tick before being able to shoot.
	if ( revolver && g_cl.m_revolver_cock > 0 && g_cl.m_revolver_cock == g_cl.m_revolver_query )
	{
		*g_cl.m_packet = false;
		return;
	}

	// we have a normal weapon or a non cocking revolver
	// choke if its the processing tick.
	if ( g_cl.m_weapon_fire && !g_cl.m_lag && !revolver )
	{
		*g_cl.m_packet = false;
		StripAttack( );
		return;
	}

	// no point in aimbotting if we cannot fire this tick.
	if ( !g_cl.m_weapon_fire )
		return;

	// setup bones for all valid targets.
	for ( int i{1}; i <= g_csgo.m_globals->m_max_clients; ++i )
	{
		Player* player = g_csgo.m_entlist->GetClientEntity< Player* >( i );

		if ( !player )
			continue;

		if ( !IsValidTarget( player ) )
			continue;

		AimPlayer* data = &m_players[i - 1];
		if ( !data )
			continue;

		if ( g_menu.main.misc.whitelist.get( ) )
		{
			player_info_t info;

			if ( g_csgo.m_engine->GetPlayerInfo( i, &info ) )
			{
				if ( !g_cl.fade_users.empty( ) )
				{
					if ( std::find( g_cl.fade_users.begin( ), g_cl.fade_users.end( ), info.m_user_id ) != g_cl.fade_users.end( ) )
						continue;
				}
			}
		}

		// store player as potential target this tick.
		m_targets.emplace_back( data );
	}

	// run knifebot.
	if ( g_cl.m_weapon_type == WEAPONTYPE_KNIFE && g_cl.m_weapon_id != ZEUS )
	{

		if ( g_menu.main.aimbot.knifebot.get( ) )
			knife( );

		return;
	}

	// scan available targets... if we even have any.
	find( );

	// finally set data when shooting.
	apply( );
}

void Aimbot::find( )
{
	struct BestTarget_t { Player* player; vec3_t pos; float damage; LagRecord* record; int hitbox; int hitgroup; };

	vec3_t       tmp_pos;
	float        tmp_damage;
	int          tmp_hitbox, tmp_hitgroup;
	BestTarget_t best;
	best.player = nullptr;
	best.damage = -1.f;
	best.pos = vec3_t{ };
	best.record = nullptr;
	best.hitbox = -1;
	best.hitgroup = -1;

	if ( m_targets.empty( ) )
		return;

	if ( g_cl.m_weapon_id == ZEUS && !g_menu.main.aimbot.zeusbot.get( ) )
		return;

	// iterate all targets.
	for ( const auto& t : m_targets )
	{
		if ( t->m_records.empty( ) )
			continue;

		// this player broke lagcomp.
		// his bones have been resetup by our lagcomp.
		// therfore now only the front record is valid.
		if ( g_menu.main.aimbot.lagfix.get( ) && g_lagcomp.StartPrediction( t ) )
		{
			LagRecord* front = t->m_records.front( ).get( );

			t->SetupHitboxes( front, false );
			if ( t->m_hitboxes.empty( ) )
				continue;

			// rip something went wrong..
			if ( t->GetBestAimPosition( tmp_pos, tmp_damage, front, tmp_hitbox, tmp_hitgroup ) && SelectTarget( front, tmp_pos, tmp_damage ) )
			{

				// if we made it so far, set shit.
				best.player = t->m_player;
				best.pos = tmp_pos;
				best.damage = tmp_damage;
				best.record = front;
				best.hitbox = tmp_hitbox;
				best.hitgroup = tmp_hitgroup;
			}
		}

		// player did not break lagcomp.
		// history aim is possible at this point.
		else
		{
			LagRecord* ideal = g_resolver.FindIdealRecord( t );
			if ( !ideal )
				continue;

			t->SetupHitboxes( ideal, false );
			if ( t->m_hitboxes.empty( ) )
				continue;

			// try to select best record as target.
			if ( t->GetBestAimPosition( tmp_pos, tmp_damage, ideal, tmp_hitbox, tmp_hitgroup ) && SelectTarget( ideal, tmp_pos, tmp_damage ) )
			{
				// if we made it so far, set shit.
				best.player = t->m_player;
				best.pos = tmp_pos;
				best.damage = tmp_damage;
				best.record = ideal;
				best.hitbox = tmp_hitbox;
				best.hitgroup = tmp_hitgroup;
			}

			LagRecord* last = g_resolver.FindLastRecord( t );
			if ( !last || last == ideal )
				continue;

			t->SetupHitboxes( last, true );
			if ( t->m_hitboxes.empty( ) )
				continue;

			// rip something went wrong..
			if ( t->GetBestAimPosition( tmp_pos, tmp_damage, last, tmp_hitbox, tmp_hitgroup ) && SelectTarget( last, tmp_pos, tmp_damage ) )
			{
				// if we made it so far, set shit.
				best.player = t->m_player;
				best.pos = tmp_pos;
				best.damage = tmp_damage;
				best.record = last;
				best.hitbox = tmp_hitbox;
				best.hitgroup = tmp_hitgroup;
			}
		}
	}

	// verify our target and set needed data.
	if ( best.player && best.record )
	{
		update_shoot_pos( );

		// calculate aim angle.
		math::VectorAngles( best.pos - g_cl.m_shoot_pos, m_angle );

		// set member vars.
		m_target = best.player;
		m_aim = best.pos;
		m_damage = best.damage;
		m_record = best.record;
		m_hitbox = best.hitbox;
		m_hitgroup = best.hitgroup;

		// write data, needed for traces / etc.
		m_record->cache( );

		// set autostop shit.
		m_stop = !(g_cl.m_buttons & IN_JUMP);

		g_movement.AutoStop( );

		bool on = g_menu.main.aimbot.hitchance.get( ) && g_menu.main.config.mode.get( ) == 0;
		bool hit = on && CheckHitchance( m_target, m_angle );

		// if we can scope.
		bool can_scope = !g_cl.m_local->m_bIsScoped( ) && (g_cl.m_weapon_id == AUG || g_cl.m_weapon_id == SG553 || g_cl.m_weapon_type == WEAPONTYPE_SNIPER_RIFLE);

		if ( can_scope )
		{
			// always.
			if ( g_menu.main.aimbot.zoom.get( ) == 1 )
			{
				g_cl.m_cmd->m_buttons |= IN_ATTACK2;
				return;
			}

			// hitchance fail.
			else if ( g_menu.main.aimbot.zoom.get( ) == 2 && on && !hit )
			{
				g_cl.m_cmd->m_buttons |= IN_ATTACK2;
				return;
			}
		}

		if ( hit || !on )
		{
			// right click attack.
			if ( g_menu.main.config.mode.get( ) == 1 && g_cl.m_weapon_id == REVOLVER )
				g_cl.m_cmd->m_buttons |= IN_ATTACK2;

			// left click attack.
			else
				g_cl.m_cmd->m_buttons |= IN_ATTACK;
		}
	}
}

// @evitable: use this it's better and more accurate.
// @evitable: s/o kaaba.su <3
bool Aimbot::CanHitPlayer( LagRecord* pRecord, const vec3_t& vecEyePos, const vec3_t& vecEnd, int iHitboxIndex )
{
	if ( !pRecord || !pRecord->m_player || !pRecord->m_bones || !pRecord->m_setup )
		return false;

	const model_t* model = pRecord->m_player->GetModel( );
	if ( !model )
		return false;

	studiohdr_t* hdr = g_csgo.m_model_info->GetStudioModel( model );
	if ( !hdr )
		return false;

	auto pHitboxSet = hdr->GetHitboxSet( pRecord->m_player->m_nHitboxSet( ) );

	if ( !pHitboxSet )
		return false;

	auto pHitbox = pHitboxSet->GetHitbox( iHitboxIndex );

	if ( !pHitbox )
		return false;

	const auto pMatrix = pRecord->m_bones;
	if ( !pMatrix )
		return false;

	const float flRadius = pHitbox->m_radius;
	const bool bCapsule = flRadius != -1.f;

	vec3_t vecMin;
	vec3_t vecMax;

	math::VectorTransform( pHitbox->m_mins, pRecord->m_bones[pHitbox->m_bone], vecMin );
	math::VectorTransform( pHitbox->m_maxs, pRecord->m_bones[pHitbox->m_bone], vecMax );

	const bool bIntersected = bCapsule ? math::IntersectSegmentToSegment( vecEyePos, vecEnd, vecMin, vecMax, flRadius ) : math::IntersectionBoundingBox( vecEyePos, vecEnd, vecMin, vecMax );

	return bIntersected;
};

float GetIdealAccuracyBoost( )
{
	if ( g_menu.main.aimbot.accuracyboost_amount.get( ) == 1 )
		return 0.4f;
	else if ( g_menu.main.aimbot.accuracyboost_amount.get( ) == 2 )
		return 0.6f;
	else if ( g_menu.main.aimbot.accuracyboost_amount.get( ) == 3 )
		return 1.0f;
}

bool Aimbot::CheckHitchance( Player* player, const ang_t& angle )
{
	constexpr float HITCHANCE_MAX = 100.f;
	constexpr int   SEED_MAX = 255;

	float hc = g_menu.main.aimbot.hitchance_amount.get( );

	vec3_t     start{g_cl.m_shoot_pos}, end, fwd, right, up, dir, wep_spread;
	float      inaccuracy, spread;
	CGameTrace tr;
	float     total_hits{ }, needed_hits{(hc / HITCHANCE_MAX) * SEED_MAX};

	// get needed directional vectors.
	math::AngleVectors( angle, &fwd, &right, &up );

	// store off inaccuracy / spread ( these functions are quite intensive and we only need them once ).
	inaccuracy = g_cl.m_weapon->GetInaccuracy( );
	spread = g_cl.m_weapon->GetSpread( );

	// get player hp.
	float goal_damage = 1.f;
	int hp = std::min( 100, player->m_iHealth( ) );

	if ( g_cl.m_weapon_id == ZEUS )
	{
		goal_damage = hp + 1;
	} else
	{
		goal_damage = g_aimbot.m_damage_toggle ? g_menu.main.aimbot.override_dmg_value.get( ) : g_menu.main.aimbot.minimal_damage.get( );

		if ( goal_damage >= 100 || g_menu.main.aimbot.minimal_damage_hp.get( ) )
			goal_damage = hp + (goal_damage - 100);
	}

	float accuracy_boost = GetIdealAccuracyBoost( );

	// iterate all possible seeds.
	for ( int i{ }; i <= SEED_MAX; ++i )
	{
		// get spread.
		wep_spread = g_cl.m_weapon->CalculateSpread( i, inaccuracy, spread );

		// get spread direction.
		dir = (fwd + (right * wep_spread.x) + (up * wep_spread.y)).normalized( );

		// get end of trace.
		end = start + (dir * g_cl.m_weapon_info->m_range);

		// setup ray and trace.
		g_csgo.m_engine_trace->ClipRayToEntity( Ray( start, end ), MASK_SHOT_HULL | CONTENTS_HITBOX, player, &tr );

		// check if we hit a valid player / hitgroup on the player and increment total hits.
		if ( tr.m_entity == player && game::IsValidHitgroup( tr.m_hitgroup ) )
		{
			penetration::PenetrationInput_t in;
			in.m_damage = 1.f;
			in.m_damage_pen = 1.f;
			in.m_can_pen = g_cl.m_weapon_id == ZEUS ? false : true;
			in.m_target = player;
			in.m_from = g_cl.m_local;
			in.m_pos = end;

			penetration::PenetrationOutput_t out;

			bool hit = penetration::run( &in, &out );

			if ( g_menu.main.aimbot.accuracyboost_amount.get( ) != 0 )
			{
				if ( hit && (out.m_damage >= goal_damage * accuracy_boost) )
					++total_hits;
			} else if ( hit )
				++total_hits;
		}

		// we cant make it anymore.
		if ( (SEED_MAX - i + total_hits) < needed_hits )
			return false;
	}

	return total_hits >= needed_hits;
}

bool AimPlayer::SetupHitboxPoints( LagRecord* record, BoneArray* bones, int index, std::vector< vec3_t >& points )
{
	// reset points.
	points.clear( );

	const model_t* model = m_player->GetModel( );
	if ( !model )
		return false;

	studiohdr_t* hdr = g_csgo.m_model_info->GetStudioModel( model );
	if ( !hdr )
		return false;

	mstudiohitboxset_t* set = hdr->GetHitboxSet( m_player->m_nHitboxSet( ) );
	if ( !set )
		return false;

	mstudiobbox_t* bbox = set->GetHitbox( index );
	if ( !bbox )
		return false;

	// get hitbox scales.
	float scale = g_menu.main.aimbot.scale.get( ) / 100.f;

	// big inair fix.
	if ( !(record->m_pred_flags & FL_ONGROUND) && scale > 0.7f )
		scale = 0.7f;

	float bscale = g_menu.main.aimbot.body_scale.get( ) / 100.f;

	// big duck fix.
	//if (!(record->m_pred_flags & FL_DUCKING) && bscale > 0.6f)
	//	bscale = 0.6f;

	// these indexes represent boxes.
	if ( bbox->m_radius <= 0.f )
	{
		// references: 
		//      https://developer.valvesoftware.com/wiki/Rotation_Tutorial
		//      CBaseAnimating::GetHitboxBonePosition
		//      CBaseAnimating::DrawServerHitboxes

		// convert rotation angle to a matrix.
		matrix3x4_t rot_matrix;
		g_csgo.AngleMatrix( bbox->m_angle, rot_matrix );

		// apply the rotation to the entity input space ( local ).
		matrix3x4_t matrix;
		math::ConcatTransforms( record->m_bones[bbox->m_bone], rot_matrix, matrix );

		// extract origin from matrix.
		vec3_t origin = matrix.GetOrigin( );

		// compute raw center point.
		vec3_t center = (bbox->m_mins + bbox->m_maxs) / 2.f;

		// the feet hiboxes have a side, heel and the toe.
		if ( index == HITBOX_R_FOOT || index == HITBOX_L_FOOT )
		{

			// side is more optimal then center.
			points.push_back( {center.x, center.y, center.z} );

			if ( g_menu.main.aimbot.multipoint.get( ) == 2 )
			{
				// get point offset relative to center point
				// and factor in hitbox scale.
				float d2 = (bbox->m_mins.x - center.x) * scale;

				// heel.
				points.push_back( {center.x + d2, center.y, center.z} );
			}
		}

		// nothing to do here we are done.
		if ( points.empty( ) )
			return false;

		// rotate our bbox points by their correct angle
		// and convert our points to world space.
		for ( auto& p : points )
		{
			// VectorRotate.
			// rotate point by angle stored in matrix.
			p = {p.dot( matrix[0] ), p.dot( matrix[1] ), p.dot( matrix[2] )};

			// transform point to world space.
			p += origin;
		}
	}

	// these hitboxes are capsules.
	else
	{
		// factor in the pointscale.
		float r = bbox->m_radius * scale;
		float br = bbox->m_radius * bscale;

		// compute raw center point.
		vec3_t center = (bbox->m_mins + bbox->m_maxs) / 2.f;

		// head has 5 points.
		if ( index == HITBOX_HEAD )
		{
			// add center.
			points.push_back( center );

			if ( g_menu.main.aimbot.multipoint.get( ) == 0 || g_menu.main.aimbot.multipoint.get( ) == 1 || g_menu.main.aimbot.multipoint.get( ) == 2 )
			{
				// rotation matrix 45 degrees.
				// https://math.stackexchange.com/questions/383321/rotating-x-y-points-45-degrees
				// std::cos( deg_to_rad( 45.f ) )
				constexpr float rotation = 0.70710678f;

				// top/back 45 deg.
				// this is the best spot to shoot at.
				points.push_back( {bbox->m_maxs.x + (rotation * r), bbox->m_maxs.y + (-rotation * r), bbox->m_maxs.z} );

				// right.
				points.push_back( {bbox->m_maxs.x, bbox->m_maxs.y, bbox->m_maxs.z + r} );

				// left.
				points.push_back( {bbox->m_maxs.x, bbox->m_maxs.y, bbox->m_maxs.z - r} );

				// back.
				points.push_back( {bbox->m_maxs.x, bbox->m_maxs.y - r, bbox->m_maxs.z} );

				// get animstate ptr.
				CCSGOPlayerAnimState* state = record->m_player->m_PlayerAnimState( );

				if ( state && record->m_anim_velocity.length( ) <= 0.1f && record->m_eye_angles.x <= state->m_aim_pitch_min )
				{

					// bottom point.
					points.push_back( {bbox->m_maxs.x - r, bbox->m_maxs.y, bbox->m_maxs.z} );
				}
			}
		}

		// body has 4 points
		// center + back + left + right
		else if ( index == HITBOX_PELVIS )
		{
			if ( g_menu.main.aimbot.multipoint.get( ) == 0 || g_menu.main.aimbot.multipoint.get( ) == 1 || g_menu.main.aimbot.multipoint.get( ) == 2 )
			{
				points.push_back( {center.x, center.y, bbox->m_maxs.z + br} );
				points.push_back( {center.x, center.y, bbox->m_mins.z - br} );
				points.push_back( {center.x, bbox->m_maxs.y - br, center.z} );
			}
		} else if ( index == HITBOX_CHEST )
		{
			if ( g_menu.main.aimbot.multipoint.get( ) == 1 || g_menu.main.aimbot.multipoint.get( ) == 2 )
			{
				points.push_back( {center.x, center.y, bbox->m_maxs.z + br} );
				points.push_back( {center.x, center.y, bbox->m_mins.z - br} );
				points.push_back( {center.x, bbox->m_maxs.y - br, center.z} );
			}
		}
		// exact same as pelvis but inverted ( god knows what theyre doing at valve )
		else if ( index == HITBOX_BODY )
		{
			points.push_back( center );

			if ( g_menu.main.aimbot.multipoint.get( ) == 0 || g_menu.main.aimbot.multipoint.get( ) == 1 || g_menu.main.aimbot.multipoint.get( ) == 2 )
			{
				points.push_back( {center.x, bbox->m_maxs.y - br, center.z} );
			}
		}

		// other stomach/chest hitboxes have 3 points.
		else if ( index == HITBOX_THORAX )
		{
			// add center.
			points.push_back( center );

			// add extra point on back.
			if ( g_menu.main.aimbot.multipoint.get( ) == 1 || g_menu.main.aimbot.multipoint.get( ) == 2 )
			{
				points.push_back( {center.x, bbox->m_maxs.y - br, center.z} );
			}
		}

		else if ( index == HITBOX_R_CALF || index == HITBOX_L_CALF )
		{
			// add center.
			points.push_back( center );

			// half bottom.
			if ( g_menu.main.aimbot.multipoint.get( ) == 2 )
				points.push_back( {bbox->m_maxs.x - (bbox->m_radius / 2.f), bbox->m_maxs.y, bbox->m_maxs.z} );
		}

		else if ( index == HITBOX_R_THIGH || index == HITBOX_L_THIGH )
		{
			// add center.
			if ( g_menu.main.aimbot.multipoint.get( ) == 0 || g_menu.main.aimbot.multipoint.get( ) == 1 || g_menu.main.aimbot.multipoint.get( ) == 2 )
				points.push_back( center );
		}

		// arms get only one point.
		else if ( index == HITBOX_R_UPPER_ARM || index == HITBOX_L_UPPER_ARM )
		{
			// elbow.
			if ( g_menu.main.aimbot.multipoint.get( ) == 0 || g_menu.main.aimbot.multipoint.get( ) == 1 || g_menu.main.aimbot.multipoint.get( ) == 2 )
				points.push_back( {bbox->m_maxs.x + bbox->m_radius, center.y, center.z} );
		}

		// nothing left to do here.
		if ( points.empty( ) )
			return false;

		// transform capsule points.`
		for ( auto& p : points )
			math::VectorTransform( p, record->m_bones[bbox->m_bone], p );
	}

	return true;
}

bool AimPlayer::GetBestAimPosition( vec3_t& aim, float& damage, LagRecord* record, int& hitbox, int& hitgroup )
{
	bool                  done, pen;
	float                 dmg;
	HitscanData_t         scan;
	std::vector< vec3_t > points;

	// get player hp.
	int hp = std::min( 100, m_player->m_iHealth( ) );

	if ( g_cl.m_weapon_id == ZEUS )
	{
		dmg = hp;
		pen = false;
	}

	else
	{
		dmg = g_aimbot.m_damage_toggle ? g_menu.main.aimbot.override_dmg_value.get( ) : g_menu.main.aimbot.minimal_damage.get( );
		if ( g_menu.main.aimbot.minimal_damage_hp.get( ) )
			dmg = std::ceil( (dmg / 100.f) * hp );

		pen = true;
	}

	// write all data of this record l0l.
	record->cache( );

	// iterate hitboxes.
	for ( const auto& it : m_hitboxes )
	{
		done = false;

		// setup points on hitbox.
		if ( !SetupHitboxPoints( record, record->m_bones, it.m_index, points ) )
			continue;

		// iterate points on hitbox.
		for ( const auto& point : points )
		{
			penetration::PenetrationInput_t in;

			in.m_damage = dmg;
			in.m_damage_pen = dmg;
			in.m_can_pen = pen;
			in.m_target = m_player;
			in.m_from = g_cl.m_local;
			in.m_pos = point;

			// ignore mindmg.
			//if ( it.m_mode == HitscanMode::LETHAL || it.m_mode == HitscanMode::LETHAL2 )
			//	in.m_damage = in.m_damage_pen = 1.f;

			penetration::PenetrationOutput_t out;

			// we can hit p!
			if ( penetration::run( &in, &out ) )
			{

				// nope we did not hit head..
				if ( it.m_index == HITBOX_HEAD && out.m_hitgroup != HITGROUP_HEAD )
					continue;

				// prefered hitbox, just stop now.
				if ( it.m_mode == HitscanMode::PREFER )
					done = true;

				// this hitbox requires lethality to get selected, if that is the case.
				// we are done, stop now.
				else if ( it.m_mode == HitscanMode::LETHAL && out.m_damage >= m_player->m_iHealth( ) )
					done = true;

				// 2 shots will be sufficient to kill.
				else if ( it.m_mode == HitscanMode::LETHAL2 && (out.m_damage * 2.f) >= m_player->m_iHealth( ) )
					done = true;

				// this hitbox has normal selection, it needs to have more damage.
				else if ( it.m_mode == HitscanMode::NORMAL )
				{
					// we did more damage.
					if ( out.m_damage > scan.m_damage )
					{
						// save new best data.
						scan.m_damage = out.m_damage;
						scan.m_pos = point;
						scan.m_hitbox = it.m_index;
						scan.m_hitgroup = out.m_hitgroup;

						// if the first point is lethal
						// screw the other ones.
						if ( point == points.front( ) && out.m_damage >= m_player->m_iHealth( ) )
							break;
					}
				}

				// we found a preferred / lethal hitbox.
				if ( done )
				{
					// save new best data.
					scan.m_damage = out.m_damage;
					scan.m_pos = point;
					scan.m_hitbox = it.m_index;
					scan.m_hitgroup = out.m_hitgroup;
					break;
				}
			}
		}

		// ghetto break out of outer loop.
		if ( done )
			break;
	}

	// we found something that we can damage.
	// set out vars.
	if ( scan.m_damage > 0.f )
	{
		aim = scan.m_pos;
		damage = scan.m_damage;
		hitbox = scan.m_hitbox;
		hitgroup = scan.m_hitgroup;
		return true;
	}

	return false;
}

bool Aimbot::SelectTarget( LagRecord* record, const vec3_t& aim, float damage )
{
	float dist, fov, height;
	int   hp;

	// fov check.
	//if (g_menu.main.aimbot.fov.get()) {
	//	// if out of fov, retn false.
	//	if (math::GetFOV(g_cl.m_view_angles, g_cl.m_shoot_pos, aim) > g_menu.main.aimbot.fov_amount.get())
	//		return false;
	//}

	switch ( g_menu.main.aimbot.selection.get( ) )
	{

		// distance.
		case 0:
			dist = (record->m_pred_origin - g_cl.m_shoot_pos).length( );

			if ( dist < m_best_dist )
			{
				m_best_dist = dist;
				return true;
			}

			break;

			// crosshair.
		case 1:
			fov = math::GetFOV( g_cl.m_view_angles, g_cl.m_shoot_pos, aim );

			if ( fov < m_best_fov )
			{
				m_best_fov = fov;
				return true;
			}

			break;

			// damage.
		case 2:
			if ( damage > m_best_damage )
			{
				m_best_damage = damage;
				return true;
			}

			break;

			// lowest hp.
		case 3:
			// fix for retarded servers?
			hp = std::min( 100, record->m_player->m_iHealth( ) );

			if ( hp < m_best_hp )
			{
				m_best_hp = hp;
				return true;
			}

			break;

			// least lag.
		case 4:
			if ( record->m_lag < m_best_lag )
			{
				m_best_lag = record->m_lag;
				return true;
			}

			break;

			// height.
		case 5:
			height = record->m_pred_origin.z - g_cl.m_local->m_vecOrigin( ).z;

			if ( height < m_best_height )
			{
				m_best_height = height;
				return true;
			}

			break;

		default:
			return false;
	}

	return false;
}

void Aimbot::apply( )
{
	bool attack, attack2;
	player_info_t info;

	// attack states.
	attack = (g_cl.m_cmd->m_buttons & IN_ATTACK);
	attack2 = (g_cl.m_weapon_id == REVOLVER && g_cl.m_cmd->m_buttons & IN_ATTACK2);

	// ensure we're attacking.
	if ( attack || attack2 )
	{
		// choke every shot.
		*g_cl.m_packet = g_cl.m_lag >= 14;

		if ( m_target && g_csgo.m_engine->GetPlayerInfo( m_target->index( ), &info ) )
		{
			// make sure to aim at un-interpolated data.
			// do this so BacktrackEntity selects the exact record.
			if ( m_record && !m_record->m_broke_lc )
				g_cl.m_cmd->m_tick = game::TIME_TO_TICKS( m_record->m_sim_time + g_cl.m_lerp );

			// set angles to target.
			g_cl.m_cmd->m_view_angles = m_angle;

			// if not silent aim, apply the viewangles.
			if ( !g_menu.main.aimbot.silent.get( ) )
				g_csgo.m_engine->SetViewAngles( m_angle );

			g_cl.print( tfm::format( XOR( "aimbot -> fired shot at %s for %s damage [ choke: %s | lc: %s | fw: %s | mode: %s] \n" ),
									 info.m_name, m_damage, m_record->m_lag, m_record->brokelc( ), m_record->m_fake_walk, m_record->m_mode ) );

			// store fired shot.
			g_shots.OnShotFire( m_target, m_damage, m_record, m_hitbox, m_hitgroup, m_aim );
		}

		// nospread.
		if ( g_menu.main.aimbot.nospread.get( ) && g_menu.main.config.mode.get( ) == 1 )
			NoSpread( );

		// norecoil.
		if ( g_menu.main.aimbot.norecoil.get( ) )
			g_cl.m_cmd->m_view_angles -= g_cl.m_local->m_aimPunchAngle( ) * g_csgo.weapon_recoil_scale->GetFloat( );

		// set that we fired.
		g_cl.m_shot = true;
	}
}

void Aimbot::NoSpread( )
{
	bool    attack2;
	vec3_t  spread, forward, right, up, dir;

	// revolver state.
	attack2 = (g_cl.m_weapon_id == REVOLVER && (g_cl.m_cmd->m_buttons & IN_ATTACK2));

	// get spread.
	spread = g_cl.m_weapon->CalculateSpread( g_cl.m_cmd->m_random_seed, attack2 );

	// compensate.
	g_cl.m_cmd->m_view_angles -= { -math::rad_to_deg( std::atan( spread.length_2d( ) ) ), 0.f, math::rad_to_deg( std::atan2( spread.x, spread.y ) ) };
}