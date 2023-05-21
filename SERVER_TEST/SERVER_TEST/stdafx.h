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

constexpr int VIEW_RANGE = 5;
constexpr int ATTACK_RANGE = 1;

constexpr int PORT_NUM = 4000;
constexpr int BUF_SIZE = 200;
constexpr int NAME_SIZE = 20;
constexpr int CHAT_SIZE = 100;

constexpr int MAX_USER = 10000;
constexpr int MAX_NPC = 20000;

constexpr int W_WIDTH = 2000;
constexpr int W_HEIGHT = 2000;

enum EVENT_TYPE { EV_RANDOM_MOVE, EV_CHASE_MOVE };

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

template <typename T>
class my_unordered_set : public unordered_set<T>
{
public:
	mutable shared_mutex s_mutex;
};

enum COMP_TYPE { OP_ACCEPT, OP_RECV, OP_SEND, OP_NPC_MOVE, OP_AI_HELLO };
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


