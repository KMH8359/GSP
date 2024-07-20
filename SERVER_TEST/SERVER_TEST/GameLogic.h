#pragma once
#include "Session.h"

array<CHARACTER*, MAX_USER + MAX_NPC> characters;
concurrent_priority_queue<TIMER_EVENT> timer_queue;

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

void InitializeNPC(const string& filename)
{
	std::cout << "NPC intialize begin.\n";
	int i = 0;
	while (i < MAX_USER) {
		SESSION* PLAYER = new SESSION();
		characters[i++] = PLAYER;
	}

	//for (int i = MAX_USER; i < MAX_USER + MAX_NPC; ++i) {
	//	MONSTER* NPC = new MONSTER();
	//	NPC->point.x = rand() % W_WIDTH;
	//	NPC->point.y = rand() % W_HEIGHT;
	//	sprintf_s(NPC->_name, "NPC%d", i);
	//	NPC->_state = ST_INGAME;
	//	NPC->a_type = static_cast<ATTACK_TYPE>(rand() % 2);
	//	NPC->m_type = static_cast<MOVE_TYPE>(rand() % 2);
	//	NPC->Level = rand() % 9;
	//	NPC->HP = NPC->Level * 100;
	//	NPC->EXP = NPC->Level * NPC->Level * 2 * int(NPC->a_type + 1) * int(NPC->m_type + 1);
	//	characters[i] = NPC;
	//	characters[i]->_id = i;

	//}

	ifstream file(filename);
	if (file.is_open()) {
		string line;
		while (std::getline(file, line)) {
			if (line.find("x: ") != std::string::npos) {
				MONSTER* NPC = new MONSTER();
				NPC->_state = ST_INGAME;
				NPC->point.x = std::stoi(line.substr(line.find(": ") + 2));
				std::getline(file, line);
				NPC->point.y = std::stoi(line.substr(line.find(": ") + 2));
				std::getline(file, line);
				NPC->HP = std::stoi(line.substr(line.find(": ") + 2));
				std::getline(file, line);
				NPC->Level = std::stoi(line.substr(line.find(": ") + 2));
				std::getline(file, line);
				NPC->EXP = std::stoi(line.substr(line.find(": ") + 2));
				std::getline(file, line);
				NPC->a_type = static_cast<ATTACK_TYPE>(std::stoi(line.substr(line.find(": ") + 2)));
				std::getline(file, line);
				NPC->m_type = static_cast<MOVE_TYPE>(std::stoi(line.substr(line.find(": ") + 2)));
				characters[i] = NPC;
				characters[i]->_id = i;
				i++;
				if (i >= MAX_USER + MAX_NPC) break;
			}
		}
	}
	else {
		std::cerr << "NPC initialize Failed.\n";
	}
	std::cout << i - MAX_USER << " NPC initialize end.\n";
}

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


void WakeUpNPC(int npc_id, int waker)
{
	if (is_pc(npc_id)) {
		std::cout << "ERROR\n";
	}
	auto NPC = (MONSTER*)characters[npc_id];

	if (NPC->_is_active.load()) return;
	bool old_state = false;
	if (false == atomic_compare_exchange_strong(&NPC->_is_active, &old_state, true))
		return;
	TIMER_EVENT ev{ npc_id, chrono::system_clock::now(), EV_NPC_UPDATE, 0 };
	timer_queue.push(ev);
}


bool can_see(int from, int to)
{
	try {
		if (abs(characters.at(from)->point.x - characters.at(to)->point.x) > VIEW_RANGE || characters.at(to)->is_alive == false) return false;
		return abs(characters.at(from)->point.y - characters.at(to)->point.y) <= VIEW_RANGE;
	}
	catch (const out_of_range& e) {
		return false;
	}
}

bool in_monsterAgro(int from, int to)
{
	try {
		if (abs(characters.at(from)->point.x - characters.at(to)->point.x) > MONSTER_VIEW_RANGE || characters.at(to)->is_alive == false) return false;
		return abs(characters.at(from)->point.y - characters.at(to)->point.y) <= MONSTER_VIEW_RANGE;
	}
	catch (const out_of_range& e) {
		return false;
	}
}

bool can_attack(int from, int to)
{
	try {
		if (abs(characters.at(from)->point.x - characters.at(to)->point.x) > ATTACK_RANGE || characters.at(to)->is_alive == false) return false;
		return abs(characters.at(from)->point.y - characters.at(to)->point.y) <= ATTACK_RANGE;
	}
	catch (const out_of_range& e) {
		return false;
	}
}

int get_new_client_id()
{
	for (int i = 0; i < MAX_USER; ++i) {
		if (characters[i]->_state.load() == ST_FREE)
			return i;
	}
	return -1;
}

void MONSTER::move()
{
	unordered_set<int> old_vl = _view_list;

	if (target_id > -1) {
		if (!can_see(_id, target_id)) return;
		if (path.empty()) {
			TILEPOINT start = point;
			TILEPOINT dest = characters[target_id]->point;
			path = Trace_Player(start, dest);
			if (path.empty()) return;
		}
		TILEPOINT next_point = path.top();

		int dx = next_point.x - point.x;
		int dy = next_point.y - point.y;

		set_direction(dx, dy);

		point = next_point;
		path.pop();
	}
	else {
		int x = point.x;
		int y = point.y;
		direction = rand() % 4;
		switch (direction) {
		case 0: if (y < (W_HEIGHT - 1)) y++; break;
		case 1: if (x > 0) x--; break;
		case 2: if (y > 0) y--; break;
		case 3: if (x < (W_WIDTH - 1)) x++; break;
		}
		if (GridMap[x][y]) {
			point.x = x;
			point.y = y;
		}
	}


	my_unordered_set<int> new_vl;
	for (int i = 0; i < MAX_USER; ++i) {
		auto& obj = characters[i];
		if (ST_INGAME != obj->_state.load()) continue;
		if (true == can_see(_id, obj->_id)) {
			new_vl.insert(obj->_id);
			if (a_type == AGRO && target_id < 0 && in_monsterAgro(_id, obj->_id)) {
				//cout << a_type << ", " << m_type << "타입 몬스터 " << _id << "가 " << obj->_id << "플레이어 추격 시작\n";
				target_id = obj->_id;
			}
		}
	}

	for (auto pl : new_vl) {
		auto session = (SESSION*)characters[pl];
		if (0 == old_vl.count(pl)) {
			session->send_add_player_packet(characters[_id]);
		}
		else {
			session->send_move_packet(characters[_id]);
		}
	}

	for (auto pl : old_vl) {
		if (0 == new_vl.count(pl)) {
			auto session = (SESSION*)characters[pl];
			session->_view_list.s_mutex.lock_shared();
			if (0 != session->_view_list.count(_id)) {
				session->_view_list.s_mutex.unlock_shared();
				session->send_remove_player_packet(_id);
			}
			else {
				session->_view_list.s_mutex.unlock_shared();
			}
		}
	}
	_view_list = new_vl;
}

void do_lockednpc_update(int npc_id)
{
	auto npc = (MONSTER*)characters[npc_id];
	unordered_set<int> old_vl = npc->_view_list;

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
			session->send_add_player_packet(characters[npc->_id]);
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