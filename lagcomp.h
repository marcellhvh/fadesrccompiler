#pragma once

class AimPlayer;

class LagCompensation {
public:
	enum LagType : size_t {
		INVALID = 0,
		CONSTANT,
		ADAPTIVE,
		RANDOM,
	};

public:
	bool StartPrediction( AimPlayer* player );
};

extern LagCompensation g_lagcomp;