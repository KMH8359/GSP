#pragma once
#include "CHARACTER.h"

class MONSTER : public CHARACTER {
public:
	atomic_bool	_is_active;
	lua_State* _L;
	mutex	_ll;
public:
	MONSTER()
	{
	}

	~MONSTER() {}

};