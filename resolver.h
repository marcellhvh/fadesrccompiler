#pragma once

class ShotRecord;

class Resolver {
public:
	enum Modes : size_t {
		RESOLVE_NONE = 0,
		RESOLVE_WALK, // 1
		RESOLVE_LBY, // 2
		RESOLVE_EDGE, // 3
		RESOLVE_AIR_LAST, // 4
		RESOLVE_REVERSEFS, // 5
		RESOLVE_BACK, // 6
		RESOLVE_STAND, // 7
		RESOLVE_STAND1, // 8
		RESOLVE_STAND2, // 9
		RESOLVE_STAND3, // 10
		RESOLVE_AIR, // 11
		RESOLVE_BODY, // 12
		RESOLVE_LASTMOVE, //13
		RESOLVE_AIR_BACK, // 14
		RESOLVE_AIR_LBY, // 15
		RESOLVE_BACK_AT, // 16
		RESOLVE_SIDE_AT, // 17
		RESOLVE_STAND3_BACK_AT, // 18
		RESOLVE_STAND3_SIDE_AT, // 19
		RESOLVE_SIDE_LASTMOVE, //20
		RESOLVE_FAKEFLICK, //21
		RESOLVE_FAKE_BACK, // 22
		RESOLVE_FAKE_SIDE,//23
		RESOLVE_FAKE,//24
		RESOLVE_BODY_PRED, // 25
		RESOLVE_AIR_TEST, // 26
		RESOLVE_OVERRIDE, //27
		RESOLVE_STAND4, // 28
		RESOLVE_SPIN, // 29
		RESOLVE_LOW_LBY, // 30
		RESOLVE_DISTORTION, // 31
		RESOLVE_TEST_FS, // 32
		RESOLVE_LOGIC, // 33
		RESOLVE_UPD_LBY, // 34
		RESOLVE_LAST_UPD_LBY, // 35
		RESOLVE_FAKEWALK, // 36
		RESOLVE_KAABA, // 37
	};

public:
	LagRecord* FindIdealRecord(AimPlayer* data);
	LagRecord* FindLastRecord(AimPlayer* data);

	bool IsYawSideways(Player* entity, float yaw);

	void OnBodyUpdate(Player* player, float value);
	float GetAwayAngle(LagRecord* record);

	void MatchShot(AimPlayer* data, LagRecord* record);
	void SetMode(LagRecord* record);

	void FindBestAngle(Player* player, LagRecord* record);
	void ResolveAngles(Player* player, LagRecord* record);
	void ResolveWalk(AimPlayer* data, LagRecord* record, Player* player);
	void ResolveYawBruteforce(LagRecord* record, Player* player, AimPlayer* data);
	void ResolveStand(AimPlayer* data, LagRecord* record, Player* player);
	void StandNS(AimPlayer* data, LagRecord* record);
	void ResolveAir(AimPlayer* data, LagRecord* record, Player* player);

	void AirNS(AimPlayer* data, LagRecord* record);
	void ResolvePoses(Player* player, LagRecord* record);
	void ResolveOverride(Player* player, LagRecord* record, AimPlayer* data);

public:
	std::array< vec3_t, 64 > m_impacts;
	int	   iPlayers[64];
	bool   m_step_switch;
	int    m_random_lag;
	float  m_next_random_update;
	float  m_random_angle;
	float  m_direction;
	float  m_auto;
	float  m_auto_dist;
	float  m_auto_last;
	float  m_view;

	class PlayerResolveRecord
	{
	public:
		struct AntiFreestandingRecord
		{
			int right_damage = 0, left_damage = 0;
			float right_fraction = 0.f, left_fraction = 0.f;
		};

	public:
		AntiFreestandingRecord m_sAntiEdge;
	};

	PlayerResolveRecord player_resolve_records[33];
};

extern Resolver g_resolver;