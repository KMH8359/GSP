#pragma once

#include <iostream>
#include <array>
#include <WS2tcpip.h>
#include <MSWSock.h>
#include <thread>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <unordered_set>
#include <unordered_map>
#include <atomic>
#include <concurrent_priority_queue.h>
#include <concurrent_queue.h>
#include <concurrent_unordered_set.h>
#include <fstream>

extern "C"
{
#include "include\lua.h"
#include "include\lauxlib.h"
#include "include\lualib.h"
}


#pragma comment(lib, "WS2_32.lib")
#pragma comment(lib, "MSWSock.lib")
#pragma comment(lib, "lua54.lib")



using namespace std;
using namespace concurrency;

constexpr int VIEW_RANGE = 15;
constexpr int ATTACK_RANGE = 1;
constexpr int MONSTER_VIEW_RANGE = 11;

constexpr int PORT_NUM = 4000;
constexpr int BUF_SIZE = 200;
constexpr int NAME_SIZE = 10;
constexpr int CHAT_SIZE = 100;

constexpr int MAX_USER = 10000;
constexpr int MAX_NPC = 20000;

constexpr int W_WIDTH = 2000;
constexpr int W_HEIGHT = 2000;

enum EVENT_TYPE { EV_RANDOM_MOVE, EV_CHASE_MOVE, EV_ATTACK, EV_REVIVE, EV_PLAYERHP_RECOVERY };

struct TIMER_EVENT {
	int obj_id;
	chrono::system_clock::time_point wakeup_time;
	EVENT_TYPE event_id;
	int target_id;
	constexpr bool operator < (const TIMER_EVENT& L) const
	{
		return (wakeup_time > L.wakeup_time);
	}
};

enum DB_EVENT_TYPE { EV_SIGNIN, EV_SIGNUP };
struct DB_EVENT {
	unsigned short session_id = -1;
	wchar_t user_id[NAME_SIZE]{};
	wchar_t user_password[NAME_SIZE]{};
	int _event = -1;
};

template <typename T>
class my_unordered_set : public unordered_set<T>
{
public:
	mutable shared_mutex s_mutex;

	my_unordered_set& operator=(const my_unordered_set& other)
	{
		s_mutex.lock();
		unordered_set<T>::operator=(other);
		s_mutex.unlock();
		return *this;
	}
};

enum COMP_TYPE { OP_ACCEPT, OP_RECV, OP_SEND, OP_LOGIN_OK, OP_NPC_MOVE, OP_NPC_CHASE, OP_NPC_ATTACK,  OP_NPC_REVIVE, OP_HEAL };
class OVER_EXP {
public:
	WSAOVERLAPPED _over;
	WSABUF _wsabuf;
	char _send_buf[BUF_SIZE];
	COMP_TYPE _comp_type;
	int _ai_target_obj;
	OVER_EXP()
	{
		_wsabuf.len = BUF_SIZE;
		_wsabuf.buf = _send_buf;
		_comp_type = OP_RECV;
		ZeroMemory(&_over, sizeof(_over));
	}
	OVER_EXP(char* packet)
	{
		_wsabuf.len = packet[0];
		_wsabuf.buf = _send_buf;
		ZeroMemory(&_over, sizeof(_over));
		_comp_type = OP_SEND;
		memcpy(_send_buf, packet, packet[0]);
	}
};

struct TILEPOINT
{
	short x;
	short y;

	TILEPOINT operator+(const TILEPOINT& other) const {
		TILEPOINT result;
		result.x = this->x + other.x;
		result.y = this->y + other.y;
		return result;
	}

	bool operator==(const TILEPOINT& other) const {
		return this->x == other.x && this->y == other.y;
	}
};

struct PointHash {
	std::size_t operator()(const TILEPOINT& p) const {
		std::size_t hash = 17;
		hash = hash * 31 + std::hash<int>()(p.x);
		hash = hash * 31 + std::hash<int>()(p.y);
		return hash;
	}
};

struct PointEqual {
	bool operator()(const TILEPOINT& p1, const TILEPOINT& p2) const {
		return (p1.x == p2.x) && (p1.y == p2.y);
	}
};


class A_star_Node
{
public:
	int F = 0;
	int G = 0;
	int H = 0;
	shared_ptr<A_star_Node> parent;
	TILEPOINT Pos = { 0,0 };
	A_star_Node() {}
	A_star_Node(TILEPOINT _Pos, TILEPOINT _Dest_Pos, int _G, shared_ptr<A_star_Node> node)
	{
		Pos = _Pos;
		G = _G;
		H = abs(_Dest_Pos.y - Pos.y) + abs(_Dest_Pos.x - Pos.x);
		F = G + H;
		if (node) {
			parent = node;
		}
	}
	void Initialize(TILEPOINT _Pos, TILEPOINT _Dest_Pos, int _G, shared_ptr<A_star_Node> node)
	{
		Pos = _Pos;
		G = _G;
		H = abs(_Dest_Pos.y - Pos.y) + abs(_Dest_Pos.x - Pos.x);
		F = G + H;
		if (node) {
			parent = node;
		}
	}
};