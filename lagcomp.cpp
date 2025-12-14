#include "includes.h"

LagCompensation g_lagcomp{};;

bool LagCompensation::StartPrediction( AimPlayer* data ) {
	if ( !data )
		return false;

	// we have no data to work with.
	// this should never happen if we call this
	if( data->m_records.empty( ) )
		return false;

	// meme.
	if( data->m_player->dormant( ) )
		return false;
	
	// compute the true amount of updated records
	// since the last time the player entered pvs.
	size_t size{};

	// iterate records.
	for( const auto &it : data->m_records ) {
		if( it->dormant( ) )
			break;

		// increment total amount of data.
		++size;
	}

	// get first record.
	LagRecord* record = data->m_records[ 0 ].get( );

	// reset all prediction related variables.
	// this has been a recurring problem in all my hacks lmfao.
	// causes the prediction to stack on eachother.
	record->predict( );

	// we are not breaking lagcomp at this point.
	// return false so it can aim at all the records it once
	// since server-sided lagcomp is still active and we can abuse that.
	if( !record->m_broke_lc )
		return false;

	int simulation = game::TIME_TO_TICKS( record->m_sim_time );

	// this is too much lag to fix.
	if( std::abs( g_cl.m_arrival_tick - simulation ) >= 128 )
		return true;

	// compute the amount of lag that we will predict for, if we have one set of data, use that.
	// if we have more data available, use the prevoius lag delta to counter weird fakelags that switch between 14 and 2.
	int lag = record->m_lag_time;

	// clamp this just to be sure.
	math::clamp( lag, 1, 15 );

	// get the delta in ticks between the last server net update
	// and the net update on which we created this record.
	int updatedelta = g_cl.m_server_tick - record->m_tick;

	// if the lag delta that is remaining is less than the current netlag
	// that means that we can shoot now and when our shot will get processed
	// the origin will still be valid, therefore we do not have to predict.
	if( g_cl.m_latency_ticks <= lag - updatedelta )
		return true;

	// the next update will come in, wait for it.
	int next = record->m_tick + 1;
	if( next + lag >= g_cl.m_arrival_tick )
		return true;

	// invalidate our bones
	record->invalidate( );
	g_bones.setup( data->m_player, nullptr, record );

	return true;
}