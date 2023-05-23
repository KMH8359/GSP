#include "protocol.h"
#include "stdafx.h"
#include "Monster.h"
#include "Session.h"

concurrent_priority_queue<TIMER_EVENT> timer_queue;
HANDLE h_iocp;
array<CHARACTER*, MAX_USER + MAX_NPC> characters;
array<array<bool, W_WIDTH>, W_HEIGHT> GridMap;
SOCKET listenSocket, g_c_socket;
OVER_EXP g_a_over;

TILEPOINT Trace_Player(TILEPOINT origin, TILEPOINT destination);


TILEPOINT vec[8]{
	TILEPOINT(-1,0),
	TILEPOINT(1,0),
	TILEPOINT(0,1),
	TILEPOINT(0,-1),
	TILEPOINT(-1,-1),
	TILEPOINT(-1,1),
	TILEPOINT(1,-1),
	TILEPOINT(1,1)
};

bool point_compare(TILEPOINT p1, TILEPOINT p2)
{
	return p1.x == p2.x && p1.y == p2.y;
}

bool is_pc(int object_id)
{
	return object_id < MAX_USER;
}

bool is_npc(int object_id)
{
	return !is_pc(object_id);
}

bool can_see(int from, int to)
{
	if (abs(characters[from]->point.x - characters[to]->point.x) > VIEW_RANGE) return false;
	return abs(characters[from]->point.y - characters[to]->point.y) <= VIEW_RANGE;
}

bool can_attack(int from, int to)
{
	if (abs(characters[from]->point.x - characters[to]->point.x) > ATTACK_RANGE) return false;
	return abs(characters[from]->point.y - characters[to]->point.y) <= ATTACK_RANGE;
}

void SESSION::send_move_packet(int c_id, char direction)
{
	auto session = (SESSION*)characters[c_id];
	SC_MOVE_OBJECT_PACKET p;
	p.id = c_id;
	p.size = sizeof(SC_MOVE_OBJECT_PACKET);
	p.type = SC_MOVE_OBJECT;
	p.point.x = session->point.x;
	p.point.y = session->point.y;
	p.direction = direction;
	p.move_time = session->last_move_time;
	do_send(&p);
}

void SESSION::send_add_player_packet(int c_id)
{
	auto session = (SESSION*)characters[c_id];
	SC_ADD_OBJECT_PACKET add_packet;
	add_packet.id = c_id;
	strcpy_s(add_packet.name, session->_name);
	add_packet.size = sizeof(add_packet);
	add_packet.type = SC_ADD_OBJECT;
	add_packet.point.x = session->point.x;
	add_packet.point.y = session->point.y;
	_view_list.s_mutex.lock();
	_view_list.insert(c_id);
	_view_list.s_mutex.unlock();
	do_send(&add_packet);
}


int get_new_client_id()
{
	for (int i = 0; i < MAX_USER; ++i) {
		if (characters[i]->_state.load() == ST_FREE)
			return i;
	}
	return -1;
}

void WakeUpNPC(int npc_id, int waker)
{
	//OVER_EXP* exover = new OVER_EXP;
	//exover->_comp_type = OP_AI_HELLO;
	//exover->_ai_target_obj = waker;
	//PostQueuedCompletionStatus(h_iocp, 1, npc_id, &exover->_over);
	if (is_pc(npc_id)) {
		cout << "ERROR" << endl;
	}
	auto NPC = (MONSTER*)characters[npc_id];
	if (NPC->_is_active.load()) return;
	bool old_state = false;
	if (false == atomic_compare_exchange_strong(&NPC->_is_active, &old_state, true))
		return;
	NPC->target_id = waker;
	TIMER_EVENT ev{ npc_id, chrono::system_clock::now(), EV_RANDOM_MOVE, 0 };
	timer_queue.push(ev);
}

void process_packet(int c_id, char* packet)
{
	auto session = (SESSION*)characters[c_id];
	switch (packet[1]) {
	case CS_LOGIN: {
		CS_LOGIN_PACKET* p = reinterpret_cast<CS_LOGIN_PACKET*>(packet);
		strcpy_s(session->_name, p->name);
		{
			//lock_guard<mutex> ll{ session->_s_lock };
			session->point.x = rand() % 100;
			session->point.y = rand() % 100;
			session->_state.store(ST_INGAME);
		}
		session->send_login_info_packet();
		for (auto& pl : characters) {
			if (ST_INGAME != pl->_state.load()) continue;		
			if (pl->_id == c_id) continue;
			if (!can_see(c_id, pl->_id)) continue;
			if (is_pc(pl->_id)) {
				auto other_session = (SESSION*)pl;
				other_session->send_add_player_packet(c_id);
			}
			else WakeUpNPC(pl->_id, c_id);
			session->send_add_player_packet(pl->_id);
		}
		break;
	}
	case CS_MOVE: {
		CS_MOVE_PACKET* p = reinterpret_cast<CS_MOVE_PACKET*>(packet);
		session->last_move_time = p->move_time;
		short x = session->point.x;
		short y = session->point.y;
		switch (p->direction) {
		case 0: if (y < W_HEIGHT - 1) y++; break;
		case 1: if (x > 0) x--; break;
		case 2: if (y > 0) y--; break; 
		case 3: if (x < W_WIDTH - 1) x++; break;
		}
		if (GridMap[x][y]) {
			session->point.x = x;
			session->point.y = y;
		}

		unordered_set<int> near_list;
		session->_view_list.s_mutex.lock_shared();
		unordered_set<int> old_vlist = session->_view_list;
		session->_view_list.s_mutex.unlock_shared();
		for (auto& cl : characters) {
			if (cl->_state.load() != ST_INGAME) continue;
			if (cl->_id == c_id) continue;
			if (can_see(c_id, cl->_id))
				near_list.insert(cl->_id);
		}

		session->send_move_packet(c_id, p->direction);

		for (auto& pl : near_list) {
			if (is_pc(pl)) {
				auto near_session = (SESSION*)characters[pl];
				near_session->_view_list.s_mutex.lock_shared();
				if (near_session->_view_list.count(c_id)) {
					near_session->_view_list.s_mutex.unlock_shared();
					near_session->send_move_packet(c_id, p->direction);
				}
				else {
					near_session->_view_list.s_mutex.unlock_shared();
					near_session->send_add_player_packet(c_id);
				}
			}
			else WakeUpNPC(pl, c_id);

			if (old_vlist.count(pl) == 0)
				session->send_add_player_packet(pl);
		}

		for (auto& pl : old_vlist)
			if (0 == near_list.count(pl)) {
				session->send_remove_player_packet(pl);
				if (is_pc(pl)) {
					auto other_session = (SESSION*)characters[pl];
					other_session->send_remove_player_packet(c_id);
				}
			}
	}
				break;
	case CS_ATTACK: {
		session->_view_list.s_mutex.lock_shared();
		unordered_set<int> near_list = session->_view_list;
		session->_view_list.s_mutex.unlock_shared();
		for (auto& obj_id : near_list) {
			auto& obj = characters[obj_id];
			if (is_npc(obj_id) && can_attack(c_id, obj_id)) {
				obj->HP.fetch_sub(50);
				if (obj->HP.load() <= 0) {
					bool alive = true;
					if (false == atomic_compare_exchange_strong(&obj->is_alive, &alive, false))
						return;
					session->EXP += obj->EXP;	
					obj->_view_list.s_mutex.lock_shared();
					for (auto& player_id : obj->_view_list) {
						cout << "send remove packet\n";
						auto session = (SESSION*)characters[player_id];
						session->send_remove_player_packet(obj_id);
					}
					obj->_view_list.s_mutex.unlock_shared();
				}
			}
		}
	}
		break;
	}
}

void disconnect(int c_id)
{
	auto session = (SESSION*)characters[c_id];
	session->_view_list.s_mutex.lock_shared();
	unordered_set <int> vl = session->_view_list;
	session->_view_list.s_mutex.unlock_shared();
	for (auto& p_id : vl) {
		if (is_npc(p_id)) continue;
		auto pl = (SESSION*)characters[p_id];
		if (ST_INGAME != pl->_state.load()) continue;
		if (pl->_id == c_id) continue;
		pl->send_remove_player_packet(c_id);
	}
	closesocket(session->_socket);

	session->_state.store(ST_FREE);
}

void do_npc_random_move(int npc_id)
{
	auto npc = (MONSTER*)characters[npc_id];
	unordered_set<int> old_vl = npc->_view_list;


	int x = npc->point.x;
	int y = npc->point.y;
	short direction = rand() % 4;
	switch (direction) {
	case 0: if (y < (W_HEIGHT - 1)) y++; break; 
	case 1: if (x > 0) x--; break;
	case 2: if (y > 0) y--; break;
	case 3:if (x < (W_WIDTH - 1)) x++; break; 
	}
	if (GridMap[x][y]) {
		npc->point.x = x;
		npc->point.y = y;
	}


	my_unordered_set<int> new_vl;
	for (int i = 0; i < MAX_USER; ++i) {
		auto& obj = characters[i];
		if (ST_INGAME != obj->_state.load()) continue;
		if (true == can_see(npc->_id, obj->_id))
			new_vl.insert(obj->_id);
	}

	for (auto pl : new_vl) {
		auto session = (SESSION*)characters[pl];
		if (0 == old_vl.count(pl)) {
			session->send_add_player_packet(npc->_id);
		}
		else {
			session->send_move_packet(npc->_id, direction);
		}
	}

	for (auto pl : old_vl) {
		if (0 == new_vl.count(pl)) {
			auto session = (SESSION*)characters[pl];
			session->_view_list.s_mutex.lock_shared();
			if (0 != session->_view_list.count(npc->_id)) {
				session->_view_list.s_mutex.unlock_shared();
				session->send_remove_player_packet(npc->_id);
			}
			else {
				session->_view_list.s_mutex.unlock_shared();
			}
		}
	}
	npc->_view_list = new_vl;
}

unordered_map<TILEPOINT, shared_ptr<A_star_Node>, PointHash, PointEqual>::iterator getNode(unordered_map<TILEPOINT, shared_ptr<A_star_Node>, PointHash, PointEqual>& m_List)
{
	return min_element(m_List.begin(), m_List.end(),
		[](const pair<const TILEPOINT, shared_ptr<A_star_Node>>& p1, const pair<const TILEPOINT, shared_ptr<A_star_Node>>& p2)
		{
			return p1.second->F < p2.second->F;
		});
}
bool check_openList(TILEPOINT& _Pos, int _G, shared_ptr<A_star_Node> s_node, unordered_map<TILEPOINT, shared_ptr<A_star_Node>, PointHash, PointEqual>& m_List)
{
	auto iter = m_List.find(_Pos);
	if (iter != m_List.end()) {
		if ((*iter).second->G > _G) {
			(*iter).second->G = _G;
			(*iter).second->F = (*iter).second->G + (*iter).second->H;
			(*iter).second->parent = s_node;
		}
		return false;
	}
	return true;
}
TILEPOINT Trace_Player(TILEPOINT origin, TILEPOINT destination)
{
	unordered_set<TILEPOINT, PointHash, PointEqual> closelist{};
	unordered_map<TILEPOINT, shared_ptr<A_star_Node>, PointHash, PointEqual> openlist;
	openlist.reserve(200);
	closelist.reserve(600);
	shared_ptr<A_star_Node> S_Node;
	
	openlist.emplace(origin, make_shared<A_star_Node>(origin, destination, 0.f, nullptr));

	while (!openlist.empty())
	{
		auto iter = getNode(openlist);
		S_Node = (*iter).second;
		cout << S_Node->F << endl;
		if (point_compare(S_Node->Pos, destination))
		{
			while (S_Node->parent != nullptr)
			{
				if (point_compare(S_Node->parent->Pos, origin))
				{
					cout << S_Node->Pos.x << ", " << S_Node->Pos.y << endl;
					return S_Node->Pos;
				}
				S_Node = S_Node->parent;
			}
		}
		for (int i = 0; i < 8; i++) {
			TILEPOINT _Pos = S_Node->Pos + vec[i];
			int _G = S_Node->G + abs(vec[i].x) + abs(vec[i].y);
			if (closelist.count(_Pos) == 0 && GridMap[_Pos.x][_Pos.y] &&
				check_openList(_Pos, _G, S_Node, openlist)) {
				openlist.emplace(_Pos, make_shared<A_star_Node>(_Pos, destination, _G, S_Node));
			}
		}
		closelist.emplace(S_Node->Pos);
		openlist.erase(iter);
	}
	return origin;
}
void worker_thread(HANDLE h_iocp)
{
	while (true) {
		DWORD num_bytes;
		ULONG_PTR key;
		WSAOVERLAPPED* over = nullptr;
		BOOL ret = GetQueuedCompletionStatus(h_iocp, &num_bytes, &key, &over, INFINITE);
		OVER_EXP* ex_over = reinterpret_cast<OVER_EXP*>(over);
		if (FALSE == ret) {
			if (ex_over->_comp_type == OP_ACCEPT) { cout << "Accept Error\n"; exit(-1); }
			else {
				cout << "GQCS Error on client[" << key << "]\n";
				disconnect(static_cast<int>(key));
				if (ex_over->_comp_type == OP_SEND) delete ex_over;
				continue;
			}
		}

		if ((0 == num_bytes) && ((ex_over->_comp_type == OP_RECV) || (ex_over->_comp_type == OP_SEND))) {
			disconnect(static_cast<int>(key));
			if (ex_over->_comp_type == OP_SEND) delete ex_over;
			continue;
		}

		switch (ex_over->_comp_type) {
		case OP_ACCEPT: {
			int client_id = get_new_client_id();
			if (client_id != -1) {
				auto session = (SESSION*)characters[client_id];
				session->_state.store(ST_ALLOC);
				session->point.x = session->point.y = 0;
				session->_id = client_id;
				session->_name[0] = 0;
				session->_prev_remain = 0;
				session->_socket = g_c_socket;
				CreateIoCompletionPort(reinterpret_cast<HANDLE>(g_c_socket),
					h_iocp, client_id, 0);
				session->do_recv();
				g_c_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
			}
			else {
				cout << "Max user exceeded.\n";
			}
			ZeroMemory(&g_a_over._over, sizeof(g_a_over._over));
			int addr_size = sizeof(SOCKADDR_IN);
			AcceptEx(listenSocket, g_c_socket, g_a_over._send_buf, 0, addr_size + 16, addr_size + 16, 0, &g_a_over._over);
			break;
		}
		case OP_RECV: {
			auto session = (SESSION*)characters[key];
			int remain_data = num_bytes + session->_prev_remain;
			char* p = ex_over->_send_buf;
			while (remain_data > 0) {
				int packet_size = p[0];
				if (packet_size <= remain_data) {
					process_packet(static_cast<int>(key), p);
					p = p + packet_size;
					remain_data = remain_data - packet_size;
				}
				else break;
			}
			session->_prev_remain = remain_data;
			if (remain_data > 0) {
				memcpy(ex_over->_send_buf, p, remain_data);
			}
			session->do_recv();
			break;
		}
		case OP_SEND:
			delete ex_over;
			break;
		case OP_NPC_MOVE: {
			delete ex_over;
			if (characters[key]->is_alive.load() == false) {
				cout << static_cast<int>(key) << " Revive Start\n";
				TIMER_EVENT ev{ static_cast<int>(key), chrono::system_clock::now() + 5s, EV_REVIVE, 0 };
				timer_queue.push(ev);
				break;
			}


			bool keep_alive = false;
			for (int j = 0; j < MAX_USER; ++j) {
				if (characters[j]->_state.load() != ST_INGAME) continue;
				if (can_see(static_cast<int>(key), j)) {
					keep_alive = true;
					break;
				}
			}
			if (true == keep_alive) {
				do_npc_random_move(static_cast<int>(key));
				TIMER_EVENT ev{ static_cast<int>(key), chrono::system_clock::now() + 1s, EV_RANDOM_MOVE, 0 };
				timer_queue.push(ev);
			}
			else {
				auto NPC = (MONSTER*)characters[static_cast<int>(key)];
				NPC->_is_active = false;
			}
		}
						break;
		case OP_NPC_ATTACK: {
			delete ex_over;
			if (characters[key]->is_alive.load() == false) {
				cout << static_cast<int>(key) << " Revive Start\n";
				TIMER_EVENT ev{ static_cast<int>(key), chrono::system_clock::now() + 5s, EV_REVIVE, 0 };
				timer_queue.push(ev);
				break;
			}


			auto NPC = (MONSTER*)characters[key];
			if (can_attack(static_cast<int>(key), NPC->target_id)) {
				characters[NPC->target_id]->HP.fetch_sub(50);
				TIMER_EVENT ev{ static_cast<int>(key), chrono::system_clock::now() + 1s, EV_ATTACK, 0 };
				timer_queue.push(ev);
			}
			else {
				TIMER_EVENT ev{ static_cast<int>(key), chrono::system_clock::now() + 1s, EV_RANDOM_MOVE, 0 };
				timer_queue.push(ev);
			}
		}
						  break;
		}
	}
}

int API_get_x(lua_State* L)
{
	int user_id =
		(int)lua_tointeger(L, -1);
	lua_pop(L, 2);
	int x = characters[user_id]->point.x;
	lua_pushnumber(L, x);
	return 1;
}

int API_get_y(lua_State* L)
{
	int user_id =
		(int)lua_tointeger(L, -1);
	lua_pop(L, 2);
	int y = characters[user_id]->point.y;
	lua_pushnumber(L, y);
	return 1;
}

int API_SendMessage(lua_State* L)
{
	int my_id = (int)lua_tointeger(L, -3);
	int user_id = (int)lua_tointeger(L, -2);
	char* mess = (char*)lua_tostring(L, -1);

	lua_pop(L, 4);

	auto session = (SESSION*)characters[user_id];
	session->send_chat_packet(my_id, mess);
	return 0;
}

void InitializeMap()
{
	char value;
	ifstream file("map.txt");
	if (file.is_open()) {
		for (int i = 0; i < W_WIDTH; ++i) {
			for (int j = 0; j < W_WIDTH; ++j) {
				file >> value;
				GridMap[i][j] = (value == '1');
			}
		}
		file.close();
	}
}

void InitializeNPC()
{
	cout << "NPC intialize begin.\n";
	for (int i = 0; i < MAX_USER; ++i) {
		SESSION* PLAYER = new SESSION();
		characters[i] = PLAYER;
	}

	for (int i = MAX_USER; i < MAX_USER + MAX_NPC; ++i) {
		MONSTER* NPC = new MONSTER();
		NPC->point.x = rand() % W_WIDTH;
		NPC->point.y = rand() % W_HEIGHT;
		sprintf_s(NPC->_name, "NPC%d", i);
		NPC->_state = ST_INGAME;
		NPC->a_type = static_cast<ATTACK_TYPE>(rand() % 2);
		NPC->m_type = static_cast<MOVE_TYPE>(rand() % 2);

		auto L = NPC->_L = luaL_newstate();
		luaL_openlibs(L);
		luaL_loadfile(L, "npc.lua");
		lua_pcall(L, 0, 0, 0);

		lua_getglobal(L, "set_uid");
		lua_pushnumber(L, i);
		lua_pcall(L, 1, 0, 0);
		// lua_pop(L, 1);// eliminate set_uid from stack after call

		lua_register(L, "API_SendMessage", API_SendMessage);
		lua_register(L, "API_get_x", API_get_x);
		lua_register(L, "API_get_y", API_get_y);
		characters[i] = NPC;
		characters[i]->_id = i;

	}
	cout << "NPC initialize end.\n";
}

void do_timer()
{
	while (true) {
		TIMER_EVENT ev;
		auto current_time = chrono::system_clock::now();
		if (true == timer_queue.try_pop(ev)) {
			if (ev.wakeup_time > current_time) {
				timer_queue.push(ev);	
				this_thread::sleep_for(1ms);  
				continue;
			}
			switch (ev.event_id) {
			case EV_RANDOM_MOVE: {
				OVER_EXP* ov = new OVER_EXP;
				ov->_comp_type = OP_NPC_MOVE;
				PostQueuedCompletionStatus(h_iocp, 1, ev.obj_id, &ov->_over);
			}
				break;
			case EV_ATTACK: {
				OVER_EXP* ov = new OVER_EXP;
				ov->_comp_type = OP_NPC_ATTACK;
				PostQueuedCompletionStatus(h_iocp, 1, ev.obj_id, &ov->_over);
			}
				break;
			}
			continue;		
		}
		this_thread::sleep_for(1ms);  
	}
}

int main()
{
	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 2), &WSAData);
	listenSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

	SOCKADDR_IN server_addr{};
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT_NUM);
	server_addr.sin_addr.S_un.S_addr = INADDR_ANY;
	bind(listenSocket, (sockaddr*)&server_addr, sizeof(server_addr));

	listen(listenSocket, SOMAXCONN);

	SOCKADDR_IN cl_addr{};
	int addr_size = sizeof(cl_addr);


	InitializeNPC();
	InitializeMap();

	h_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
	CreateIoCompletionPort((HANDLE)listenSocket, h_iocp, 9999, 0);
	g_c_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	g_a_over._comp_type = OP_ACCEPT;
	AcceptEx(listenSocket, g_c_socket, g_a_over._send_buf, 0, addr_size + 16, addr_size + 16, 0, &g_a_over._over);

	vector <thread> worker_threads;
	int num_threads = std::thread::hardware_concurrency();
	for (int i = 0; i < num_threads; ++i)
		worker_threads.emplace_back(worker_thread, h_iocp);
	thread timer_thread{ do_timer };
	timer_thread.join();
	for (auto& th : worker_threads)
		th.join();
	for (auto& obj : characters)
		delete obj;
	closesocket(listenSocket);
	WSACleanup();
}
