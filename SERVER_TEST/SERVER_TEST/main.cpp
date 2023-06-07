#include "protocol.h"
#include "stdafx.h"
#include "Monster.h"
#include "Session.h"
#include <sqlext.h>
#include <sql.h>
#include <sqltypes.h>

concurrent_priority_queue<TIMER_EVENT> timer_queue;
concurrent_queue<DB_EVENT> db_queue;
HANDLE h_iocp;
array<CHARACTER*, MAX_USER + MAX_NPC> characters;
array<array<bool, W_WIDTH>, W_HEIGHT> GridMap;
SOCKET listenSocket, g_c_socket;
OVER_EXP g_a_over;

TILEPOINT Trace_Player(TILEPOINT origin, TILEPOINT destination);


#define USE_LOGINDB 1


TILEPOINT vec[4]{
	TILEPOINT(-1,0),
	TILEPOINT(1,0),
	TILEPOINT(0,1),
	TILEPOINT(0,-1),
};

void ConvertCharArrayToWideCharArray(const char* source, size_t sourceSize, wchar_t* destination, size_t destinationSize)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
	std::wstring wideString = converter.from_bytes(source, source + sourceSize);

	wcsncpy(destination, wideString.c_str(), destinationSize - 1);
	destination[destinationSize - 1] = L'\0';
}

void HandleDiagnosticRecord(SQLHANDLE hHandle, SQLSMALLINT hType, RETCODE RetCode) {
	SQLSMALLINT iRec = 0;
	SQLINTEGER  iError;
	WCHAR       wszMessage[1000];
	WCHAR       wszState[SQL_SQLSTATE_SIZE + 1];

	if (RetCode == SQL_INVALID_HANDLE)
	{
		fwprintf(stderr, L"Invalid handle!\n");
		return;
	}
	while (SQLGetDiagRec(hType, hHandle, ++iRec, wszState, &iError, wszMessage,
		(SQLSMALLINT)(sizeof(wszMessage) / sizeof(WCHAR)), (SQLSMALLINT*)NULL) == SQL_SUCCESS)
	{
		// Hide data truncated.. 
		if (wcsncmp(wszState, L"01004", 5))
		{
			fwprintf(stderr, L"[%5.5s] %s (%d)\n", wszState, wszMessage, iError);
		}
	}
}

void DB_Thread()
{
	SQLHENV henv;
	SQLHDBC hdbc;
	SQLHSTMT hstmt = 0;
	SQLRETURN retcode;
	//SQLWCHAR szUser_ID[IDPW_SIZE], szUser_PWD[IDPW_SIZE];
	SQLLEN OutSize;

	setlocale(LC_ALL, "korean");

	// Allocate environment handle  
	retcode = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv);

	// Set the ODBC version environment attribute  
	if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
		retcode = SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER*)SQL_OV_ODBC3, 0);

		// Allocate connection handle  
		if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
			retcode = SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc);


			// Set login timeout to 5 seconds  
			if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
				SQLSetConnectAttr(hdbc, SQL_LOGIN_TIMEOUT, (SQLPOINTER)5, 0);
				SQLSetConnectAttr(hdbc, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER)SQL_AUTOCOMMIT_ON, SQL_IS_INTEGER);

				SQLWCHAR* connectionString = (SQLWCHAR*)L"DRIVER=SQL Server;SERVER=14.36.243.161;DATABASE=SimpleMMORPG; UID=dbAdmin; PWD=2018180005;";

				retcode = SQLDriverConnect(hdbc, NULL, connectionString, SQL_NTS, NULL, 1024, NULL, SQL_DRIVER_NOPROMPT);

				// Allocate statement handle  
				if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
					retcode = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);
					printf("ODBC Connect OK \n");
				}
				else
					HandleDiagnosticRecord(hdbc, SQL_HANDLE_DBC, retcode);
				while (1)
				{
					DB_EVENT ev;
					if (db_queue.try_pop(ev)) {
						wcout << "GET REQUEST\n";
						SQLWCHAR* param1 = ev.user_id;
						SQLWCHAR* param2 = ev.user_password;
						SQLINTEGER param3 = ev.Hp;
						SQLINTEGER param4 = ev.Max_Hp;
						SQLINTEGER param5 = ev.Lv;
						SQLINTEGER param6 = ev.Exp;
						switch (ev._event) {
						case EV_SIGNUP:
							retcode = SQLPrepare(hstmt, (SQLWCHAR*)L"{CALL sign_up(?, ?)}", SQL_NTS);
							if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
								SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_WCHAR, SQL_WCHAR, 10, 0, (SQLPOINTER)param1, 0, NULL);
								SQLBindParameter(hstmt, 2, SQL_PARAM_INPUT, SQL_C_WCHAR, SQL_WCHAR, 10, 0, (SQLPOINTER)param2, 0, NULL);

								retcode = SQLExecute(hstmt);
								if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
									wcout << "SIGNUP OK \n";
								}
								else {
									wcout << "SIGNUP FAILED \n";
									HandleDiagnosticRecord(hdbc, SQL_HANDLE_DBC, retcode);
								}
							}
							else {
								printf("SQLPrepare failed \n");
								HandleDiagnosticRecord(hdbc, SQL_HANDLE_DBC, retcode);
							}
							SQLFreeStmt(hstmt, SQL_CLOSE);
							break;
						case EV_SIGNIN:
							retcode = SQLPrepare(hstmt, (SQLWCHAR*)L"{CALL sign_in(?, ?)}", SQL_NTS);
							if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
								SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_WCHAR, SQL_WCHAR, 10, 0, (SQLPOINTER)param1, 0, NULL);
								SQLBindParameter(hstmt, 2, SQL_PARAM_INPUT, SQL_C_WCHAR, SQL_WCHAR, 10, 0, (SQLPOINTER)param2, 0, NULL);

								retcode = SQLExecute(hstmt);
								auto session = (SESSION*)characters[ev.session_id];
								if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
									wcout << "LOGIN SUCCEED\n";
									SQLBindCol(hstmt, 1, SQL_C_CHAR, session->_name, sizeof(session->_name), &OutSize);
									SQLBindCol(hstmt, 3, SQL_C_LONG, &session->HP, sizeof(int), &OutSize);
									SQLBindCol(hstmt, 4, SQL_C_LONG, &session->MAX_HP, sizeof(int), &OutSize);
									SQLBindCol(hstmt, 5, SQL_C_LONG, &session->Level, sizeof(int), &OutSize);
									SQLBindCol(hstmt, 6, SQL_C_LONG, &session->EXP, sizeof(int), &OutSize);
									retcode = SQLFetch(hstmt);
									if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
										OVER_EXP* ov = new OVER_EXP;
										ov->_comp_type = OP_LOGIN_OK;
										PostQueuedCompletionStatus(h_iocp, 1, session->_id, &ov->_over);
									}
									else {
										wcout << "LOGIN FAILED \n";
										session->send_loginFail_packet();
									}
								}
								else {
									wcout << "LOGIN FAILED \n";
									HandleDiagnosticRecord(hdbc, SQL_HANDLE_DBC, retcode);
								}
							}
							else {
								wcout << "SQLPrepare failed \n";
								HandleDiagnosticRecord(hdbc, SQL_HANDLE_DBC, retcode);
							}
							SQLFreeStmt(hstmt, SQL_CLOSE);
							break;
						case EV_SAVE:
							retcode = SQLPrepare(hstmt, (SQLWCHAR*)L"{CALL Renewal(?, ?, ?, ?, ?)}", SQL_NTS);
							if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
								SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_WCHAR, SQL_WCHAR, 10, 0, (SQLPOINTER)param1, 0, NULL);
								SQLBindParameter(hstmt, 2, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, (SQLPOINTER)&param3, 0, NULL);
								SQLBindParameter(hstmt, 3, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, (SQLPOINTER)&param4, 0, NULL);
								SQLBindParameter(hstmt, 4, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, (SQLPOINTER)&param5, 0, NULL);
								SQLBindParameter(hstmt, 5, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, (SQLPOINTER)&param6, 0, NULL);
								retcode = SQLExecute(hstmt);
								if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
									retcode = SQLEndTran(SQL_HANDLE_DBC, hdbc, SQL_COMMIT);
									if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
										std::cout << "COMMIT OK\n";
									}
									else {
										std::cout << "COMMIT FAILED\n";
										HandleDiagnosticRecord(hdbc, SQL_HANDLE_DBC, retcode);
									}
								}
								else {
									std::cout << "UPDATE FAILED \n";
									HandleDiagnosticRecord(hdbc, SQL_HANDLE_DBC, retcode);
								}
							}
							else {
								wcout << "SQLPrepare failed \n";
								HandleDiagnosticRecord(hdbc, SQL_HANDLE_DBC, retcode);
							}
							SQLFreeStmt(hstmt, SQL_UNBIND);
							break;
						}
					}
					else this_thread::sleep_for(100ms);
				}

				if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
					SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
				}
				SQLDisconnect(hdbc);
			}
			SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
		}
		SQLFreeHandle(SQL_HANDLE_ENV, henv);
	}
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

bool can_see(int from, int to)
{
	try {
		if (abs(characters.at(from)->point.x - characters.at(to)->point.x) > VIEW_RANGE || characters.at(to)->is_alive == false) return false;
		return abs(characters.at(from)->point.y - characters.at(to)->point.y) <= VIEW_RANGE;
	}
	catch (const out_of_range& e){
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

void WakeUpNPC(int npc_id, int waker)
{
	if (is_pc(npc_id)) {
		std::cout << "ERROR" << endl;
	}
	auto NPC = (MONSTER*)characters[npc_id];

	if (NPC->_is_active.load()) return;
	bool old_state = false;
	if (false == atomic_compare_exchange_strong(&NPC->_is_active, &old_state, true))
		return;
	TIMER_EVENT ev{ npc_id, chrono::system_clock::now(), EV_RANDOM_MOVE, 0 };
	timer_queue.push(ev);
}

void process_packet(int c_id, char* packet)
{
	auto session = (SESSION*)characters[c_id];
	switch (packet[2]) {
	case CS_LOGIN: {
		CS_LOGIN_PACKET* p = reinterpret_cast<CS_LOGIN_PACKET*>(packet);
#ifdef USE_LOGINDB
		DB_EVENT ev;
		ev._event = EV_SIGNIN;
		wcscpy_s(ev.user_id, sizeof(ev.user_id) / sizeof(wchar_t), p->id);
		wcscpy_s(ev.user_password, sizeof(ev.user_password) / sizeof(wchar_t), p->password);
		ev.session_id = c_id;
		db_queue.push(ev);
#else
		session->point.x = rand() % W_WIDTH;
		session->point.y = rand() % W_HEIGHT;
		session->_state.store(ST_INGAME);
		session->send_login_info_packet();
		for (auto& pl : characters) {
			if (ST_INGAME != pl->_state.load()) continue;
			if (pl->_id == session->_id) continue;
			if (!can_see(session->_id, pl->_id)) continue;
			if (is_pc(pl->_id)) {
				auto other_session = (SESSION*)pl;
				other_session->send_add_player_packet(session);
			}
			else WakeUpNPC(pl->_id, session->_id);
			session->send_add_player_packet(characters[pl->_id]);
		}
#endif
	}
				 break;
	case CS_SIGNUP: {
		CS_LOGIN_PACKET* p = reinterpret_cast<CS_LOGIN_PACKET*>(packet);
		DB_EVENT ev;
		ev._event = EV_SIGNUP;
		wcscpy_s(ev.user_id, sizeof(ev.user_id) / sizeof(wchar_t), p->id);
		wcscpy_s(ev.user_password, sizeof(ev.user_password) / sizeof(wchar_t), p->password);
		ev.session_id = c_id;
		db_queue.push(ev);
	}
				  break;
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
		session->direction = p->direction;
		unordered_set<int> near_list;
		session->_view_list.s_mutex.lock_shared();
		unordered_set<int> old_vlist = session->_view_list;
		session->_view_list.s_mutex.unlock_shared();
		for (auto& cl : characters) {
			if (cl->_state.load() != ST_INGAME) continue;
			if (cl->_id == c_id) continue;
			if (can_see(c_id, cl->_id)) {
				near_list.insert(cl->_id);
			}
		}

		session->send_move_packet(session);

		for (auto& pl : near_list) {
			if (is_pc(pl)) {
				auto near_session = (SESSION*)characters[pl];
				near_session->_view_list.s_mutex.lock_shared();
				if (near_session->_view_list.count(c_id)) {
					near_session->_view_list.s_mutex.unlock_shared();
					near_session->send_move_packet(session);
				}
				else {
					near_session->_view_list.s_mutex.unlock_shared();
					near_session->send_add_player_packet(session);
				}
			}
			else WakeUpNPC(pl, c_id);

			if (old_vlist.count(pl) == 0)
				session->send_add_player_packet(characters[pl]);
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
	case CS_CHAT: {
		CS_CHAT_PACKET* p = reinterpret_cast<CS_CHAT_PACKET*>(packet);
		session->_view_list.s_mutex.lock_shared();
		unordered_set<int> near_list = session->_view_list;
		session->_view_list.s_mutex.unlock_shared();
		session->send_chat_packet(session->_id, p->mess);
		for (auto& obj_id : near_list) {
			if (is_npc(obj_id)) continue;
			auto other_session = (SESSION*)characters[obj_id];
			other_session->send_chat_packet(session->_id, p->mess);
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
				auto target_monster = (MONSTER*)obj;
				target_monster->HP.fetch_sub(50);
				std::cout << target_monster->a_type << " a_type," << target_monster->m_type << " m_type의 " <<
					target_monster->_id << "몬스터가 " << session->_id << "플레이어에게 50 데미지를 입음\n";
				if (target_monster->target_id < 0) target_monster->target_id = c_id;
				if (obj->HP.load() <= 0) {
					bool alive = true;
					if (false == atomic_compare_exchange_strong(&obj->is_alive, &alive, false))
						return;
					std::cout << obj->_id << " 몬스터 부활 타이머 시작\n";
					TIMER_EVENT ev{ obj->_id, chrono::system_clock::now() + 30s, EV_REVIVE, 0 };
					timer_queue.push(ev);
					session->EXP += obj->EXP;
					std::cout << session->_id << "플레이어가 " << obj->EXP << "경험치 획득\n";
					DB_EVENT update_event;
					update_event._event = EV_SAVE;
					wchar_t* wcharArray = new wchar_t[NAME_SIZE] {L""};
					ConvertCharArrayToWideCharArray(session->_name, sizeof(session->_name), wcharArray, sizeof(wcharArray));
					wcscpy_s(update_event.user_id, sizeof(update_event.user_id) / sizeof(update_event.user_id[0]), wcharArray);
					update_event.Hp = session->HP;
					update_event.Max_Hp = session->MAX_HP;
					update_event.Lv = session->Level;
					update_event.Exp = session->EXP;
					db_queue.push(update_event);
					delete[] wcharArray;
					obj->_view_list.s_mutex.lock_shared();
					for (auto& player_id : obj->_view_list) {
						auto session = (SESSION*)characters[player_id];
						session->send_remove_player_packet(obj_id);
					}
					obj->_view_list.clear();
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

void npc_move(int npc_id)
{
	auto npc = (MONSTER*)characters[npc_id];
	unordered_set<int> old_vl = npc->_view_list;
	
	if (npc->target_id > -1) {
		if (!can_see(npc_id, npc->target_id)) return;
		TILEPOINT new_point = Trace_Player(npc->point, characters[npc->target_id]->point);
		int dx = new_point.x - npc->point.x;
		int dy = new_point.y - npc->point.y;

		// 방향 계산
		if (dx == 0 && dy == 1)
			npc->direction = 0;  // 상
		else if (dx == -1 && dy == 0)
			npc->direction = 1;  // 좌
		else if (dx == 0 && dy == -1)
			npc->direction = 2;  // 하
		else if (dx == 1 && dy == 0)
			npc->direction = 3;  // 우
		npc->point = new_point;
	}
	else {
		int x = npc->point.x;
		int y = npc->point.y;
		npc->direction = rand() % 4;
		switch (npc->direction) {
		case 0: if (y < (W_HEIGHT - 1)) y++; break;
		case 1: if (x > 0) x--; break;
		case 2: if (y > 0) y--; break;
		case 3:if (x < (W_WIDTH - 1)) x++; break;
		}
		if (GridMap[x][y]) {
			npc->point.x = x;
			npc->point.y = y;
		}
	}


	my_unordered_set<int> new_vl;
	for (int i = 0; i < MAX_USER; ++i) {
		auto& obj = characters[i];
		if (ST_INGAME != obj->_state.load()) continue;
		if (true == can_see(npc->_id, obj->_id)) {
			new_vl.insert(obj->_id);
			if (npc->a_type == AGRO && npc->target_id < 0 && in_monsterAgro(npc->_id, obj->_id)) {
				cout << npc->a_type << ", " << npc->m_type << "타입 몬스터 " << npc->_id << "가 " << obj->_id << "플레이어 추격 시작\n";
				npc->target_id = obj->_id;
			}
		}
	}

	for (auto pl : new_vl) {
		auto session = (SESSION*)characters[pl];
		if (0 == old_vl.count(pl)) {
			session->send_add_player_packet(characters[npc->_id]);
		}
		else {
			session->send_move_packet(characters[npc->_id]);
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
	if (origin == destination) return origin;
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
		if (point_compare(S_Node->Pos, destination))
		{
			while (S_Node->parent != nullptr)
			{
				if (point_compare(S_Node->parent->Pos, origin))
				{
					return S_Node->Pos;
				}
				S_Node = S_Node->parent;
			}
		}
		for (int i = 0; i < 4; i++) {
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
			if (ex_over->_comp_type == OP_ACCEPT) { std::cout << "Accept Error\n"; exit(-1); }
			else {
				std::cout << "GQCS Error on client[" << key << "]\n";
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
				std::cout << "Max user exceeded.\n";
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
		case OP_LOGIN_OK: 
		{
			auto session = (SESSION*)characters[static_cast<int>(key)];
			session->point.x = rand() % W_WIDTH;
			session->point.y = rand() % W_HEIGHT;
			session->_state.store(ST_INGAME);
			session->send_login_info_packet();
			for (auto& pl : characters) {
				if (ST_INGAME != pl->_state.load()) continue;
				if (pl->_id == session->_id) continue;
				if (!can_see(session->_id, pl->_id)) continue;
				if (is_pc(pl->_id)) {
					auto other_session = (SESSION*)pl;
					other_session->send_add_player_packet(session);
				}
				else WakeUpNPC(pl->_id, session->_id);
				session->send_add_player_packet(characters[pl->_id]);
			}
			delete ex_over;
			break;
		}
		case OP_NPC_MOVE: {
			delete ex_over;
			bool keep_alive = false;
			auto NPC = (MONSTER*)characters[static_cast<int>(key)];
			if (!NPC->is_alive)
				break;
			for (int j = 0; j < MAX_USER; ++j) {
				if (characters[j]->_state.load() != ST_INGAME) continue;
				if (can_see(static_cast<int>(key), j)) {
					keep_alive = true;
					break;
				}
			}
			if (true == keep_alive) {
				if (can_attack(static_cast<int>(key), NPC->target_id)) {
					auto session = (SESSION*)characters[NPC->target_id];
					session->_lock.lock();
					session->HP -= 50;
					if (session->HP <= 0) { // 사망처리
						session->point.x = 1000;
						session->point.y = 1000;
						session->EXP /= 2;
						session->HP = session->MAX_HP;
						session->send_statchange_packet();
						unordered_set<int> near_list;
						session->_view_list.s_mutex.lock_shared();
						unordered_set<int> old_vlist = session->_view_list;
						session->_view_list.s_mutex.unlock_shared();
						for (auto& cl : characters) {
							if (cl->_state.load() != ST_INGAME) continue;
							if (cl->_id == session->_id) continue;
							if (can_see(session->_id, cl->_id))
								near_list.insert(cl->_id);
						}

						session->send_move_packet(session);

						for (auto& pl : near_list) {
							if (is_pc(pl)) {
								auto near_session = (SESSION*)characters[pl];
								near_session->_view_list.s_mutex.lock_shared();
								if (near_session->_view_list.count(session->_id)) {
									near_session->_view_list.s_mutex.unlock_shared();
									near_session->send_move_packet(session);
								}
								else {
									near_session->_view_list.s_mutex.unlock_shared();
									near_session->send_add_player_packet(session);
								}
							}
							else WakeUpNPC(pl, session->_id);

							if (old_vlist.count(pl) == 0)
								session->send_add_player_packet(characters[pl]);
						}

						for (auto& pl : old_vlist)
							if (0 == near_list.count(pl)) {
								session->send_remove_player_packet(pl);
								if (is_pc(pl)) {
									auto other_session = (SESSION*)characters[pl];
									other_session->send_remove_player_packet(session->_id);
								}
							}
					}
					session->_lock.unlock();
					std::cout << NPC->target_id << "플레이어가 " << NPC->a_type << ", " << NPC->m_type << " 타입의 " << NPC->_id << "몬스터에게 50 데미지를 입음\n";
					session->send_statchange_packet();
					bool isHealing = false;
					if (atomic_compare_exchange_strong(&session->is_healing, &isHealing, true)) {
						std::cout << session->_id << "플레이어 회복 시작\n";
						TIMER_EVENT heal_ev{ session->_id, chrono::system_clock::now() + 5s, EV_PLAYERHP_RECOVERY, 0 };
						timer_queue.push(heal_ev);
					}
				}
				if (NPC->m_type == LOCKED) {
					do_lockednpc_update(static_cast<int>(key));
				}
				else {
					npc_move(static_cast<int>(key));
				}
				TIMER_EVENT npc_ev{ static_cast<int>(key), chrono::system_clock::now() + 1s, EV_RANDOM_MOVE, 0 };
				timer_queue.push(npc_ev);
			}
			else {
				NPC->target_id = -1;
				NPC->_is_active = false;
			}
		}
						break;
		//case OP_NPC_ATTACK: {
		//	delete ex_over;
		//	auto NPC = (MONSTER*)characters[key];
		//	if (characters[NPC->target_id]->is_alive.load() == false) {
		//		std::cout << static_cast<int>(key) << "Player Revive Start\n";
		//		TIMER_EVENT ev{ static_cast<int>(key), chrono::system_clock::now() + 10s, EV_REVIVE, 0 };
		//		timer_queue.push(ev);
		//		break;
		//	}
		//	if (can_attack(static_cast<int>(key), NPC->target_id)) {
		//		characters[NPC->target_id]->HP.fetch_sub(50);
		//		TIMER_EVENT ev_attack{ static_cast<int>(key), chrono::system_clock::now() + 1s, EV_ATTACK, 0 };
		//		timer_queue.push(ev_attack);
		//		//TIMER_EVENT ev_healPlayer{ NPC->target_id, chrono::system_clock::now() + 5s, EV_PLAYERHP_RECOVERY, 0 };
		//		//timer_queue.push(ev_healPlayer);
		//	}
		//	else {
		//		TIMER_EVENT ev_move{ static_cast<int>(key), chrono::system_clock::now() + 1s, EV_RANDOM_MOVE, 0 };
		//		timer_queue.push(ev_move);
		//	}
		//}
		//				  break;
		case OP_HEAL: {
			delete ex_over;
			auto session = (SESSION*)characters[key];
			std::cout << session->_id << "회복중 - " << session->HP.load() << " / " << session->MAX_HP << endl;
			if (session->HP.load() < session->MAX_HP) {
				session->HP.fetch_add(session->MAX_HP / 10);
				if (session->HP.load() > session->MAX_HP) {
					session->HP.store(session->MAX_HP);
					session->is_healing.store(false);
				}
				std::cout << session->_id << "플레이어의 체력이 " << session->HP.load() << "만큼 회복됨\n";
				session->send_statchange_packet();
				TIMER_EVENT heal_ev{ static_cast<int>(key), chrono::system_clock::now() + 5s, EV_PLAYERHP_RECOVERY, 0 };
				timer_queue.push(heal_ev);
			}
			else {
				std::cout << session->_id << "플레이어 회복 종료\n";
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
			}
		}
	}
	else {
		std::cerr << "NPC initialize Failed.\n";
	}
	std::cout << "NPC initialize end.\n";
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
			case EV_CHASE_MOVE: {
				OVER_EXP* ov = new OVER_EXP;
				ov->_comp_type = OP_NPC_CHASE;
				PostQueuedCompletionStatus(h_iocp, 1, ev.obj_id, &ov->_over);
			}
							  break;
			case EV_REVIVE: {
				if (!is_pc(ev.obj_id)) {
					std::cout << ev.obj_id << " 몬스터가 부활함\n";
					auto Monster = (MONSTER*)characters[ev.obj_id];
					Monster->is_alive.store(true);
					Monster->HP.store(characters[ev.obj_id]->MAX_HP);
					Monster->target_id = -1;
					OVER_EXP* ov = new OVER_EXP;
					ov->_comp_type = OP_NPC_MOVE;
					PostQueuedCompletionStatus(h_iocp, 1, ev.obj_id, &ov->_over);
				
				}
			}
						  break;
			case EV_PLAYERHP_RECOVERY: {
				OVER_EXP* ov = new OVER_EXP;
				ov->_comp_type = OP_HEAL;
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
	wcout.imbue(locale("korean"));
	setlocale(LC_ALL, "korean");

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

	string monsterfilename = "monsters.txt";

	InitializeNPC(monsterfilename);
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
	thread db_thread{ DB_Thread };
	timer_thread.join();
	db_thread.join();
	for (auto& th : worker_threads)
		th.join();
	for (auto& obj : characters)
		delete obj;
	closesocket(listenSocket);
	WSACleanup();
}
