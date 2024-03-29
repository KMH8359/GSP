#pragma once
#include "stdafx.h"


enum S_STATE { ST_FREE, ST_ALLOC, ST_INGAME };
class CHARACTER
{
public:
	atomic<S_STATE> _state;
	int _id;
	TILEPOINT point;
	atomic<int>	HP;
	int	MAX_HP;
	atomic<int>	EXP;
	int	Level;
	char	_name[10];
	atomic_bool		is_alive;
	int		last_move_time;
	my_unordered_set <int> _view_list;
	short	direction;
public:
	CHARACTER()
	{
		_id = -1;
		HP = MAX_HP = point.x = point.y = EXP = Level = last_move_time = direction = -1;
		_name[0] = 0;
		_state = ST_FREE;
		is_alive = true;
	}

	~CHARACTER() {}

	//virtual bool can_see(int to) const = 0;
};

