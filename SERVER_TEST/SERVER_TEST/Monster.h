#pragma once
#include "CHARACTER.h"

enum ATTACK_TYPE { PEACE, AGRO };	// PEACE : ���� �±� ������ ���,	AGRO : �þ߿� ���̸� �ٷ� �߰�
enum MOVE_TYPE{ LOCKED, ROAMING };	// LOCKED : �̵� ����
class MONSTER : public CHARACTER {
public:
	atomic_bool	_is_active;
	int target_id = -1;
	lua_State* _L;
	mutex	_ll;
	ATTACK_TYPE a_type;
	MOVE_TYPE m_type;
	std::stack<TILEPOINT> path;
public:
	AStar_Pool* m_pool;
	MONSTER()
	{
		m_pool = new AStar_Pool(300);
		HP = MAX_HP = 200;
	}

	~MONSTER() 
	{
		delete m_pool;
	}

	void move();

	void set_direction(int dx, int dy)
	{
		if (dx == 0 && dy == 1)
			direction = 0;  // ��
		else if (dx == -1 && dy == 0)
			direction = 1;  // ��
		else if (dx == 0 && dy == -1)
			direction = 2;  // ��
		else if (dx == 1 && dy == 0)
			direction = 3;  // ��
	}
	stack<TILEPOINT> Trace_Player(const TILEPOINT start, const TILEPOINT dest);
};