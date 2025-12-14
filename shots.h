#pragma once

class ShotRecord {
public:
	__forceinline ShotRecord() : m_target{}, m_aim_point{}, m_hitbox {}, m_hitgroup{}, m_record{}, m_time{}, m_lat{}, m_damage{}, m_range{}, m_pos{}, m_impact_pos{}, m_confirmed{}, m_hurt{}, m_impacted{} {}

public:
	Player*    m_target;
	LagRecord* m_record;
	float      m_time, m_lat, m_damage, m_range;
	vec3_t     m_pos, m_impact_pos, m_aim_point;
	bool       m_confirmed, m_hurt, m_impacted;
	int        m_hitbox, m_hitgroup;
};

class VisualImpactData_t {
public:
	vec3_t m_impact_pos, m_shoot_pos;
	int    m_tickbase;
	bool   m_ignore, m_hit_player;

public:
	__forceinline VisualImpactData_t(const vec3_t &impact_pos, const vec3_t &shoot_pos, int tickbase) :
		m_impact_pos{ impact_pos }, m_shoot_pos{ shoot_pos }, m_tickbase{ tickbase }, m_ignore{ false }, m_hit_player{ false } {}
};

class Shots {
public:
	std::array< std::string, 8 > m_groups = {
		XOR("body"),
		XOR("head"),
		XOR("chest"),
		XOR("stomach"),
		XOR("left arm"),
		XOR("right arm"),
		XOR("left leg"),
		XOR("right leg")
	};

public:
	void OnShotFire(Player* target, float damage, LagRecord* record, int hitbox, int hitgroup, vec3_t aim_point);
	void OnImpact(IGameEvent* evt);
	void OnHurt(IGameEvent* evt);
	void OnWeaponFire(IGameEvent* evt);
	void OnShotMiss(ShotRecord& shot);
	void Think();

public:
	std::deque< ShotRecord >          m_shots;
	std::vector< VisualImpactData_t > m_vis_impacts;
};

extern Shots g_shots;