#pragma once
#include "CHARACTER.h"
#include "protocol.h"

class SESSION : public CHARACTER {
	OVER_EXP _recv_over;
public:
	SOCKET _socket;
	int		_prev_remain;
	int		last_move_time;
public:
	SESSION()
	{
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
	void send_move_packet(int c_id, char direction);
	void send_add_player_packet(int c_id);


	void send_chat_packet(int c_id, const char* mess)
	{
		SC_CHAT_PACKET packet;
		packet.id = c_id;
		packet.size = sizeof(packet);
		packet.type = SC_CHAT;
		strcpy_s(packet.mess, mess);
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
