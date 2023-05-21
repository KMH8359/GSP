#pragma once
#include "stdafx.h"


enum S_STATE { ST_FREE, ST_ALLOC, ST_INGAME };
class CHARACTER
{
public:
	atomic<S_STATE> _state;
	int _id;
	int		x, y;
	short	HP;
	char	_name[NAME_SIZE];
	my_unordered_set <int> _view_list;
public:
	CHARACTER()
	{
		_id = -1;
		x = y = HP = 0;
		_name[0] = 0;
		_state = ST_FREE;
	}

	~CHARACTER() {}
};

