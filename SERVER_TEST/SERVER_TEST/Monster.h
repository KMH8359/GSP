#pragma once
#include "CHARACTER.h"

enum ATTACK_TYPE { PEACE, AGRO };	// PEACE : 선빵 맞기 전까지 대기,	AGRO : 시야에 보이면 바로 추격
enum MOVE_TYPE{ LOCKED, ROAMING };	// LOCKED : 이동 못함
class MONSTER : public CHARACTER {
public:
	atomic_bool	_is_active;
	int target_id;
	lua_State* _L;
	mutex	_ll;
	ATTACK_TYPE a_type;
	MOVE_TYPE m_type;
public:
	MONSTER()
	{
	}

	~MONSTER() {}

	//TILEPOINT Trace_Player(TILEPOINT origin, TILEPOINT destination);
};