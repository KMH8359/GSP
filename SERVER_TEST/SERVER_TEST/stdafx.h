#pragma once
#define _CRT_SECURE_NO_WARNINGS
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#include <iostream>
#include <array>
#include <WS2tcpip.h>
#include <MSWSock.h>
#include <thread>
#include <vector>
#include <codecvt>
#include <mutex>
#include <queue>
#include <stack>
#include <string>
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
constexpr int MAX_NPC = 200000;

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

enum DB_EVENT_TYPE { EV_SIGNIN, EV_SIGNUP, EV_SAVE };
struct DB_EVENT {
	unsigned short session_id = -1;
	wchar_t user_id[NAME_SIZE]{};
	wchar_t user_password[NAME_SIZE]{};
	int _event = -1;
	int Hp = -1;
	int Max_Hp = -1;
	int Lv = -1;
	int Exp = -1;
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
	size_t operator()(const TILEPOINT& point) const {
		size_t h1 = std::hash<short>()(point.x);
		size_t h2 = std::hash<short>()(point.y);
		return h1 ^ (h2 << 1);
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
	float F = 0;
	float G = 0;
	float H = 0;
	A_star_Node* parent;
	TILEPOINT Pos = { 0,0 };
	A_star_Node() {}
	A_star_Node(TILEPOINT _Pos, float _G, float _H, A_star_Node* _parent)
		: Pos(_Pos), G(_G), H(_H), F(_G + _H), parent(_parent) {}

	void Reset()
	{
		F = G = H = 0;
		parent = nullptr;
		Pos = { 0, 0 };
	}
};

struct CompareNodes {
	bool operator()(A_star_Node*& node1, A_star_Node*& node2) {
		return node1->F > node2->F;
	}
};


class AStar_Pool {
private:
	concurrent_queue<A_star_Node*> objectQueue;
public:
	AStar_Pool() {}
	AStar_Pool(size_t MemorySize)
	{
		for (int i = 0; i < MemorySize; ++i) {
			objectQueue.push(new A_star_Node());
		}
	}
	~AStar_Pool()
	{
		A_star_Node* mem;
		while (objectQueue.try_pop(mem))
		{
			delete mem;
		}
	}

	A_star_Node* GetMemory(TILEPOINT _Pos, float _G, float _H, A_star_Node* _parent)
	{
		A_star_Node* front;
		if (objectQueue.try_pop(front) == false) {
			for (int i = 0; i < 50; ++i)
				objectQueue.push(new A_star_Node());
			objectQueue.try_pop(front);
			front->Pos = _Pos;
			front->G = _G;
			front->H = _H;                   
			front->F = _G + _H;
			front->parent = _parent;
			return front;
		}
		else {
			front->Pos = _Pos;
			front->G = _G;
			front->H = _H;
			front->F = _G + _H;
			front->parent = _parent;
			return front;
		}
	}

	void ReturnMemory(A_star_Node* Mem)
	{
		Mem->Reset();
		objectQueue.push(Mem);
	}
	void PrintSize()
	{
		cout << "CurrentSize - " << objectQueue.unsafe_size() << endl;
	}
};