#pragma once
#include "stdafx.h"
#include <sqlext.h>
#include <sql.h>
#include <sqltypes.h>

// #define USE_DB 

concurrent_queue<DB_EVENT> db_queue;
HANDLE h_iocp;
SOCKET listenSocket, g_c_socket;
OVER_EXP g_a_over;

array<int, 25> LevelUp_Required_Experience{};

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
		if (wcsncmp(wszState, L"01004", 5))
		{
			fwprintf(stderr, L"[%5.5s] %s (%d)\n", wszState, wszMessage, iError);
		}
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

void process_packet(int c_id, char* packet)
{
	auto session = (SESSION*)characters[c_id];
	switch (packet[2]) {
	case CS_LOGIN: {
		CS_LOGIN_PACKET* p = reinterpret_cast<CS_LOGIN_PACKET*>(packet);
#ifdef USE_DB
		DB_EVENT ev;
		ev._event = EV_SIGNIN;
		wcscpy_s(ev.user_id, sizeof(ev.user_id) / sizeof(wchar_t), p->id);
		wcscpy_s(ev.user_password, sizeof(ev.user_password) / sizeof(wchar_t), p->password);
		ev.session_id = c_id;
		db_queue.push(ev);
#else
		session->point.x = rand() % 2000;
		session->point.y = rand() % 2000;
		session->HP = session->MAX_HP = 1000;
		session->Level = 1;
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
	case CS_LOGOUT: {
		cout << c_id << " 플레이어 로그아웃\n";
		disconnect(c_id);
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
				//std::cout << target_monster->a_type << " a_type," << target_monster->m_type << " m_type의 " << target_monster->_id << "몬스터가 " << session->_id << "플레이어에게 50 데미지를 입음\n";
				if (target_monster->target_id < 0) target_monster->target_id = c_id;
				if (obj->HP.load() <= 0) {
					bool alive = true;
					if (false == atomic_compare_exchange_strong(&obj->is_alive, &alive, false))
						return;
					//std::cout << obj->_id << " 몬스터 부활 타이머 시작\n";
					TIMER_EVENT ev{ obj->_id, chrono::system_clock::now() + 30s, EV_REVIVE, 0 };
					timer_queue.push(ev);
					session->EXP.fetch_add(obj->EXP);
					if (session->EXP.load() > LevelUp_Required_Experience[session->Level]) {
						session->HP.store(1000);
						session->EXP.fetch_sub(LevelUp_Required_Experience[session->Level]);
						session->Level++;
						session->send_statchange_packet();
					}
					//std::cout << session->_id << "플레이어가 " << obj->EXP << "경험치 획득\n";
#ifdef USE_DB
					DB_EVENT update_event;
					update_event._event = EV_SAVE;
					wchar_t* wcharArray = new wchar_t[NAME_SIZE] {L""};
					ConvertCharArrayToWideCharArray(session->_name, sizeof(session->_name), wcharArray, sizeof(wcharArray));
					wcscpy_s(update_event.user_id, sizeof(update_event.user_id) / sizeof(update_event.user_id[0]), wcharArray);
					update_event.Hp = session->HP.load();
					update_event.Max_Hp = session->MAX_HP;
					update_event.Lv = session->Level;
					update_event.Exp = session->EXP.load();
					update_event.pos = session->point;
					db_queue.push(update_event);
					delete[] wcharArray;
#endif
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
					session->HP.fetch_sub(50);
					if (session->HP.load() <= 0) { // 사망처리
						session->point.x = 1000;
						session->point.y = 1000;
						session->EXP.fetch_sub(session->EXP / 2);
						session->HP.store(session->MAX_HP);
						session->send_statchange_packet();
#ifdef USE_DB
						DB_EVENT update_event;
						update_event._event = EV_SAVE;
						wchar_t* wcharArray = new wchar_t[NAME_SIZE] {L""};
						ConvertCharArrayToWideCharArray(session->_name, sizeof(session->_name), wcharArray, sizeof(wcharArray));
						wcscpy_s(update_event.user_id, sizeof(update_event.user_id) / sizeof(update_event.user_id[0]), wcharArray);
						update_event.Hp = session->HP.load();
						update_event.Max_Hp = session->MAX_HP;
						update_event.Lv = session->Level;
						update_event.Exp = session->EXP.load();
						update_event.pos = session->point;
						db_queue.push(update_event);
						delete[] wcharArray;
#endif
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
					//std::cout << NPC->target_id << "플레이어가 " << NPC->a_type << ", " << NPC->m_type << " 타입의 " << NPC->_id << "몬스터에게 50 데미지를 입음\n";
					session->send_statchange_packet();
					bool isHealing = false;
					if (atomic_compare_exchange_strong(&session->is_healing, &isHealing, true)) {
						//std::cout << session->_id << "플레이어 회복 시작\n";
						TIMER_EVENT heal_ev{ session->_id, chrono::system_clock::now() + 5s, EV_PLAYERHP_RECOVERY, 0 };
						timer_queue.push(heal_ev);
					}
				}
				if (NPC->m_type == LOCKED) {
					do_lockednpc_update(static_cast<int>(key));
				}
				else {
					NPC->move();
				}
				TIMER_EVENT npc_ev{ static_cast<int>(key), chrono::system_clock::now() + 1s, EV_NPC_UPDATE, 0 };
				timer_queue.push(npc_ev);
			}
			else {
				NPC->target_id = -1;
				NPC->_is_active = false;
			}
		}
						break;
		case OP_HEAL: {
			delete ex_over;
			auto session = (SESSION*)characters[key];
			if (session->_state.load() == ST_INGAME && session->HP.load() < session->MAX_HP) {
				session->HP.fetch_add(session->MAX_HP / 10);
				if (session->HP.load() > session->MAX_HP) {
					session->HP.store(session->MAX_HP);
					session->is_healing.store(false);
				}
				//std::cout << session->_id << "플레이어의 체력이 " << session->HP.load() << "만큼 회복됨\n";
				session->send_statchange_packet();
				TIMER_EVENT heal_ev{ static_cast<int>(key), chrono::system_clock::now() + 5s, EV_PLAYERHP_RECOVERY, 0 };
				timer_queue.push(heal_ev);
			}
			else {
				//std::cout << session->_id << "플레이어 회복 종료\n";
			}
		}
					break;
		}
	}
}

void DB_Thread()
{
	SQLHENV henv;
	SQLHDBC hdbc;
	SQLHSTMT hstmt = 0;
	SQLRETURN retcode;
	SQLLEN OutSize{};

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
				//retcode = SQLConnect(hdbc, (SQLWCHAR*)L"2023_gsp_tp", SQL_NTS, (SQLWCHAR*)NULL, 0, NULL, 0);

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
						SQLINTEGER param7 = ev.pos.x;
						SQLINTEGER param8 = ev.pos.y;
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
									SQLBindCol(hstmt, 3, SQL_C_LONG, &session->HP, sizeof(session->HP), &OutSize);
									SQLBindCol(hstmt, 4, SQL_C_LONG, &session->MAX_HP, sizeof(session->MAX_HP), &OutSize);
									SQLBindCol(hstmt, 5, SQL_C_LONG, &session->Level, sizeof(session->Level), &OutSize);
									SQLBindCol(hstmt, 6, SQL_C_LONG, &session->EXP, sizeof(session->EXP), &OutSize);
									SQLBindCol(hstmt, 7, SQL_C_SHORT, &session->point.x, sizeof(session->point.x), &OutSize);
									SQLBindCol(hstmt, 8, SQL_C_SHORT, &session->point.y, sizeof(session->point.y), &OutSize);
									retcode = SQLFetch(hstmt);
									if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
										bool in_use = false;
										for (int i = 0; i < MAX_USER; ++i) {
											if (characters[i]->_state.load() != ST_INGAME || session->_id == characters[i]->_id) continue;
											if (strcmp(session->_name, characters[i]->_name) == 0) {
												in_use = true;
												break;
											}
										}
										if (in_use == false) {
											OVER_EXP* ov = new OVER_EXP;
											ov->_comp_type = OP_LOGIN_OK;
											PostQueuedCompletionStatus(h_iocp, 1, session->_id, &ov->_over);
										}
										else {
											wcout << "This ID is already in use\n";
											session->send_loginFail_packet();
										}
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
							retcode = SQLPrepare(hstmt, (SQLWCHAR*)L"{CALL Renewal(?, ?, ?, ?, ?, ?, ?)}", SQL_NTS);
							if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
								SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_WCHAR, SQL_WCHAR, 10, 0, (SQLPOINTER)param1, 0, NULL);
								SQLBindParameter(hstmt, 2, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, (SQLPOINTER)&param3, 0, NULL);
								SQLBindParameter(hstmt, 3, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, (SQLPOINTER)&param4, 0, NULL);
								SQLBindParameter(hstmt, 4, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, (SQLPOINTER)&param5, 0, NULL);
								SQLBindParameter(hstmt, 5, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, (SQLPOINTER)&param6, 0, NULL);
								SQLBindParameter(hstmt, 6, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, (SQLPOINTER)&param7, 0, NULL);
								SQLBindParameter(hstmt, 7, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 0, 0, (SQLPOINTER)&param8, 0, NULL);
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

void Timer_Thread()
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
			case EV_NPC_UPDATE: {
				OVER_EXP* ov = new OVER_EXP;
				ov->_comp_type = OP_NPC_MOVE;
				PostQueuedCompletionStatus(h_iocp, 1, ev.obj_id, &ov->_over);
			}
							  break;
			case EV_REVIVE: {
				if (!is_pc(ev.obj_id)) {
					//std::cout << ev.obj_id << " 몬스터가 부활함\n";
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

