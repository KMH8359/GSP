#pragma once
#include "CHARACTER.h"
#include "protocol.h"

class SESSION : public CHARACTER {
	OVER_EXP _recv_over;
public:
	atomic_bool	is_healing;
	SOCKET _socket;
	int		_prev_remain;
	shared_mutex _lock;
public:
	SESSION()
	{
		HP = MAX_HP = 1000;
		is_healing = false;
		_socket = 0;
		_prev_remain = 0;
	}

	~SESSION() {}

	void do_recv()
	{
		DWORD recv_flag = 0;
		memset(&_recv_over._over, 0, sizeof(_recv_over._over));
		_recv_over._wsabuf.len = BUF_SIZE - _prev_remain;
		_recv_over._wsabuf.buf = _recv_over._send_buf + _prev_remain;
		WSARecv(_socket, &_recv_over._wsabuf, 1, 0, &recv_flag,
			&_recv_over._over, 0);
	}

	void do_send(void* packet)
	{
		OVER_EXP* sdata = new OVER_EXP{ reinterpret_cast<char*>(packet) };
		WSASend(_socket, &sdata->_wsabuf, 1, 0, 0, &sdata->_over, 0);
	}
	void send_login_info_packet()
	{
		SC_LOGIN_INFO_PACKET p;
		p.id = _id;
		p.size = sizeof(SC_LOGIN_INFO_PACKET);
		p.type = SC_LOGIN_INFO;
		p.point.x = point.x;
		p.point.y = point.y;
		do_send(&p);
	}
	void send_move_packet(CHARACTER* obj)
	{
		//auto session = (SESSION*)characters[c_id];
		SC_MOVE_OBJECT_PACKET p;
		p.id = obj->_id;
		p.size = sizeof(SC_MOVE_OBJECT_PACKET);
		p.type = SC_MOVE_OBJECT;
		p.point.x = obj->point.x;
		p.point.y = obj->point.y;
		p.direction = obj->direction;
		p.move_time = obj->last_move_time;
		do_send(&p);
	}

	void send_add_player_packet(CHARACTER* obj)
	{
		SC_ADD_OBJECT_PACKET add_packet;
		add_packet.id = obj->_id;
		strcpy_s(add_packet.name, obj->_name);
		add_packet.size = sizeof(add_packet);
		add_packet.type = SC_ADD_OBJECT;
		add_packet.point.x = obj->point.x;
		add_packet.point.y = obj->point.y;
		_view_list.s_mutex.lock();
		_view_list.insert(obj->_id);
		_view_list.s_mutex.unlock();
		do_send(&add_packet);
	}


	void send_chat_packet(int c_id, const char* mess)
	{
		SC_CHAT_PACKET packet;
		packet.id = c_id;
		packet.size = sizeof(packet);
		packet.type = SC_CHAT;
		strcpy_s(packet.mess, mess);
		do_send(&packet);
	}

	void send_loginOK_packet()
	{
		SC_LOGIN_OK_PACKET packet;
		packet.size = sizeof(packet);
		packet.type = SC_LOGIN_OK;
		packet.id = _id;
		packet.HP = HP;
		packet.EXP = EXP;
		do_send(&packet);
	}

	void send_loginFail_packet()
	{
		SC_LOGIN_FAIL_PACKET packet;
		packet.size = sizeof(packet);
		packet.type = SC_LOGIN_FAIL;
		do_send(&packet);
	}

	void send_dead_packet(int c_id)
	{
		SC_DEAD_PACKET packet;
		packet.id = c_id;
		packet.size = sizeof(packet);
		packet.type = SC_DEAD;
		do_send(&packet);
	}

	void send_statchange_packet()
	{
		SC_STAT_CHANGE_PACKET packet;
		packet.size = sizeof(packet);
		packet.type = SC_STAT_CHANGE;
		packet.hp = HP;
		packet.max_hp = MAX_HP;
		packet.exp = EXP;
		packet.level = Level;
		do_send(&packet);
	}

	void send_remove_player_packet(int c_id)
	{
		_view_list.s_mutex.lock();
		if (_view_list.count(c_id))
			_view_list.erase(c_id);
		else {
			_view_list.s_mutex.unlock();
			return;
		}
		_view_list.s_mutex.unlock();
		SC_REMOVE_OBJECT_PACKET p;
		p.id = c_id;
		p.size = sizeof(p);
		p.type = SC_REMOVE_OBJECT;
		do_send(&p);
	}
};
