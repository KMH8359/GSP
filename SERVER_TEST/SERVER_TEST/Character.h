#pragma once
#include "stdafx.h"


enum S_STATE { ST_FREE, ST_ALLOC, ST_INGAME };
class CHARACTER
{
public:
	atomic<S_STATE> _state;
	int _id;
	TILEPOINT point;
	atomic<short>	HP;
	short	EXP;
	char	_name[NAME_SIZE];
	atomic_bool		is_alive;
	my_unordered_set <int> _view_list;
public:
	CHARACTER()
	{
		_id = -1;
		point.x = point.y = EXP = 0;
		HP = 200;
		_name[0] = 0;
		_state = ST_FREE;
		is_alive = true;
	}

	~CHARACTER() {}
};

