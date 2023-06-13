#pragma once
#include "stdafx.h"

// Packet ID
constexpr char CS_LOGIN = 0;
constexpr char CS_SIGNUP = 1;
constexpr char CS_MOVE = 2;
constexpr char CS_CHAT = 3;
constexpr char CS_ATTACK = 4;			// 4 방향 공격
constexpr char CS_TELEPORT = 5;			// RANDOM한 위치로 Teleport, Stress Test할 때 Hot Spot현상을 피하기 위해 구현
constexpr char CS_LOGOUT = 6;			// 클라이언트에서 정상적으로 접속을 종료하는 패킷

constexpr char SC_LOGIN_INFO = 2;
constexpr char SC_ADD_OBJECT = 3;
constexpr char SC_REMOVE_OBJECT = 4;
constexpr char SC_MOVE_OBJECT = 5;
constexpr char SC_CHAT = 6;
constexpr char SC_LOGIN_OK = 7;
constexpr char SC_LOGIN_FAIL = 8;
constexpr char SC_STAT_CHANGE = 9;

#pragma pack (push, 1)
struct CS_LOGIN_PACKET {
	unsigned short size;
	char	type;
	wchar_t	id[NAME_SIZE];
	wchar_t	password[NAME_SIZE];
};

struct CS_MOVE_PACKET {
	unsigned short size;
	char	type;
	char	direction;  // 0 : UP, 1 : DOWN, 2 : LEFT, 3 : RIGHT
	unsigned	move_time;
};

struct CS_CHAT_PACKET {
	unsigned short size;			// 크기가 가변이다, mess가 작으면 size도 줄이자.
	char	type;
	char	mess[CHAT_SIZE];
};

struct CS_TELEPORT_PACKET {
	unsigned short size;
	char	type;
};

struct CS_ATTACK_PACKET {
	unsigned short size;
	char	type;
};

struct CS_LOGOUT_PACKET {
	unsigned short size;
	char	type;
};

struct SC_LOGIN_INFO_PACKET {
	unsigned short size;
	char	type;
	int		id;
	int		hp;
	int		max_hp;
	int		exp;
	int		level;
	TILEPOINT point;
	char	name[NAME_SIZE];
};

struct SC_ADD_OBJECT_PACKET {
	unsigned short size;
	char	type;
	char	monster_type;
	int		id;
	TILEPOINT point;
	char	name[NAME_SIZE];
};

struct SC_REMOVE_OBJECT_PACKET {
	unsigned short size;
	char	type;
	int		id;
};

struct SC_MOVE_OBJECT_PACKET {
	unsigned short size;
	char	type;
	int		id;
	char	direction;
	TILEPOINT point;
	unsigned int move_time;
};

struct SC_CHAT_PACKET {
	unsigned short size;
	char	type;
	int		id;
	char	mess[CHAT_SIZE];
};

struct SC_LOGIN_OK_PACKET {
	unsigned short size;
	char	type;
	int		id;
	short	HP;
	int		EXP;
};

struct SC_LOGIN_FAIL_PACKET {
	unsigned short size;
	char	type;

};

struct SC_STAT_CHANGE_PACKET {
	unsigned short size;
	char	type;
	int		hp;
	int		max_hp;
	int		exp;
	int		level;

};

#pragma pack (pop)