#include "protocol.h"
#include "GameLogic.h"
#include "MyThread.h"

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

	for (int i = 1; i < LevelUp_Required_Experience.size(); ++i) {
		LevelUp_Required_Experience[i] = 100 * pow(2, i - 1);
	}


	h_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
	CreateIoCompletionPort((HANDLE)listenSocket, h_iocp, 9999, 0);
	g_c_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	g_a_over._comp_type = OP_ACCEPT;
	AcceptEx(listenSocket, g_c_socket, g_a_over._send_buf, 0, addr_size + 16, addr_size + 16, 0, &g_a_over._over);

	vector <thread> worker_threads;
	int num_threads = std::thread::hardware_concurrency();
	for (int i = 0; i < num_threads; ++i)
		worker_threads.emplace_back(worker_thread, h_iocp);
	thread timer_thread{ Timer_Thread };
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
