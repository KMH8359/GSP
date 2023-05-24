#pragma once
#include "CHARACTER.h"

enum ATTACK_TYPE { PEACE, AGRO };	// PEACE : ���� �±� ������ ���,	AGRO : �þ߿� ���̸� �ٷ� �߰�
enum MOVE_TYPE{ LOCKED, ROAMING };	// LOCKED : �̵� ����
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