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
	short	MAX_HP;
	short	EXP;
	short	Level;
	char	_name[NAME_SIZE];
	atomic_bool		is_alive;
	int		last_move_time;
	my_unordered_set <int> _view_list;
	short	direction;
public:
	CHARACTER()
	{
		_id = -1;
		point.x = point.y = EXP = Level = last_move_time = direction = 0;
		HP = MAX_HP = 0;
		_name[0] = 0;
		_state = ST_FREE;
		is_alive = true;
	}

	~CHARACTER() {}

	//virtual bool can_see(int to) const = 0;
};

