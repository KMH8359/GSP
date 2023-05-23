#define SFML_STATIC 1
#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include <iostream>
#include <unordered_map>
#include <Windows.h>
#include <chrono>
#include <fstream>
#include <array>
#include "protocol.h"

using namespace std;

#pragma comment (lib, "opengl32.lib")
#pragma comment (lib, "winmm.lib")
#pragma comment (lib, "ws2_32.lib")



sf::TcpSocket s_socket;

constexpr auto SCREEN_WIDTH = 20;
constexpr auto SCREEN_HEIGHT = 20;

constexpr auto TILE_WIDTH = 50;
constexpr auto WINDOW_WIDTH = SCREEN_WIDTH * TILE_WIDTH;   // size of window
constexpr auto WINDOW_HEIGHT = SCREEN_WIDTH * TILE_WIDTH;

int g_left_x;
int g_top_y;
int g_myid;

sf::RenderWindow* g_window;
sf::Font g_font;
sf::Text Level_Text;
bool on_chat = false;
char m_mess[CHAT_SIZE]{ "" };
unsigned short chat_length = 0;

class OBJECT {
private:
	bool m_showing;
	sf::Sprite m_sprite;

	sf::Text m_name;
	sf::Text m_chat;
	chrono::system_clock::time_point m_mess_end_time;
public:
	int		id;
	int		m_x, m_y;
	short	HP = 200;
	int		Exp = 0;
	int		Level = 0;
	char name[NAME_SIZE];
	OBJECT(sf::Texture& t, int x, int y, int x2, int y2) {
		m_showing = false;
		m_sprite.setTexture(t);
		m_sprite.setTextureRect(sf::IntRect(x, y, x2, y2));

		set_name("NONAME");
		m_mess_end_time = chrono::system_clock::now();
	}
	OBJECT() {
		m_showing = false;
	}
	void show()
	{
		m_showing = true;
	}
	void hide()
	{
		m_showing = false;
	}

	void a_move(int x, int y) {
		m_sprite.setPosition((float)x, (float)y);
	}

	void a_draw() {
		g_window->draw(m_sprite);
	}

	void a_resize(short w_start, short h_start, short w_end, short h_end) {
		m_sprite.setTextureRect(sf::IntRect(w_start, h_start, w_end, h_end));
	}

	void t_draw() {
		g_window->draw(m_chat);
	}

	void move(int x, int y) {
		m_x = x;
		m_y = y;
	}
	void draw() {
		if (false == m_showing) return;
		float rx = (m_x - g_left_x) * 50.f;
		float ry = (m_y - g_top_y) * 50.f;
		m_sprite.setPosition(rx, ry);
		g_window->draw(m_sprite);
		auto size = m_name.getGlobalBounds();
		if (m_mess_end_time < chrono::system_clock::now()) {
			m_name.setPosition(rx + 32 - size.width / 2, ry - 10);
			g_window->draw(m_name);
		}
		else {
			m_chat.setPosition(rx + 32 - size.width / 2, ry - 10);
			g_window->draw(m_chat);
		}
	}
	void set_name(const char str[]) {
		m_name.setFont(g_font);
		m_name.setString(str);
		if (id < MAX_USER) m_name.setFillColor(sf::Color(255, 255, 255));
		else m_name.setFillColor(sf::Color(255, 255, 0));
		m_name.setStyle(sf::Text::Bold);
	}

	void set_chat(const char str[]) {
		m_chat.setFont(g_font);
		m_chat.setString(str);
		m_chat.setFillColor(sf::Color(255, 255, 255));
		m_chat.setStyle(sf::Text::Bold);
		m_mess_end_time = chrono::system_clock::now() + chrono::seconds(3);
	}
};

OBJECT avatar;
unordered_map <int, OBJECT> players;
array<array<bool, W_WIDTH>, W_WIDTH> GridMap;
OBJECT white_tile;
OBJECT black_tile;
OBJECT HPBAR[2];
OBJECT Level;

sf::Texture* board;
sf::Texture* pieces;
sf::Texture* monsters;
sf::Texture* hp_bar[2];
sf::Texture* lv;

void client_initialize()
{
	board = new sf::Texture;
	pieces = new sf::Texture;
	monsters = new sf::Texture;
	lv = new sf::Texture;
	hp_bar[0] = new sf::Texture;
	board->loadFromFile("tilemap.bmp");
	pieces->loadFromFile("character.png");
	monsters->loadFromFile("skeleton.png");
	lv->loadFromFile("L.png");
	hp_bar[0]->loadFromFile("HP_ON.png");

	if (false == g_font.loadFromFile("cour.ttf")) {
		cout << "Font Loading Error!\n";
		exit(-1);
	}
	white_tile = OBJECT{ *board, 13, 112, TILE_WIDTH, TILE_WIDTH };
	black_tile = OBJECT{ *board, 13, 8, TILE_WIDTH, TILE_WIDTH };

	Level = OBJECT{ *lv, 0, 0, 100, 100 };

	Level_Text.setFont(g_font);
	Level_Text.setCharacterSize(60);
	Level_Text.setFillColor(sf::Color(255, 0, 0));
	Level_Text.setPosition(30, 5);
	char buf[100];
	sprintf_s(buf, "%d", avatar.Level);
	Level_Text.setString(buf);


	HPBAR[0] = OBJECT{ *hp_bar[0], 0, 0, 200, 50};
	HPBAR[0].a_move(100, 0);
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

	avatar = OBJECT{ *pieces, 0, 0, 50, 50 };
}

void client_finish()
{
	players.clear();
	delete board;
	delete pieces;
	delete monsters;
	for (auto& img : hp_bar)
		delete img;
}

void ProcessPacket(char* ptr)
{
	static bool first_time = true;
	switch (ptr[1])
	{
	case SC_LOGIN_INFO:
	{
		SC_LOGIN_INFO_PACKET* packet = reinterpret_cast<SC_LOGIN_INFO_PACKET*>(ptr);
		g_myid = packet->id;
		avatar.id = g_myid;
		avatar.move(packet->point.x, packet->point.y);
		g_left_x = packet->point.x - SCREEN_WIDTH / 2;
		g_top_y = packet->point.y - SCREEN_HEIGHT / 2;
		avatar.show();
	}
	break;

	case SC_ADD_OBJECT:
	{
		SC_ADD_OBJECT_PACKET* my_packet = reinterpret_cast<SC_ADD_OBJECT_PACKET*>(ptr);
		int id = my_packet->id;
		if (id == g_myid) {
			avatar.move(my_packet->point.x, my_packet->point.y);
			g_left_x = my_packet->point.x - SCREEN_WIDTH / 2;
			g_top_y = my_packet->point.y - SCREEN_HEIGHT / 2;
			avatar.show();
		}
		else if (id < MAX_USER) {
			players[id] = OBJECT{ *pieces, 0, 0, 50, 50 };
			
			players[id].id = id;
			players[id].move(my_packet->point.x, my_packet->point.y);
			players[id].set_name(my_packet->name);
			players[id].show();
		}
		else {
			players[id] = OBJECT{ *monsters, 0, 200, 50, 50 };
			players[id].id = id;
			players[id].move(my_packet->point.x, my_packet->point.y);
			players[id].set_name(my_packet->name);
			players[id].show();
		}
		break;
	}
	case SC_MOVE_OBJECT:
	{
		SC_MOVE_OBJECT_PACKET* my_packet = reinterpret_cast<SC_MOVE_OBJECT_PACKET*>(ptr);
		int other_id = my_packet->id;
		if (other_id == g_myid) {
			avatar.move(my_packet->point.x, my_packet->point.y);
			g_left_x = my_packet->point.x - SCREEN_WIDTH / 2;
			g_top_y = my_packet->point.y - SCREEN_HEIGHT / 2;
			avatar.a_resize(0, my_packet->direction * 50, 50, 50);
			//HPBAR[0].a_resize(0, 0, avatar.HP, 50);
		}
		else {
			players[other_id].move(my_packet->point.x, my_packet->point.y);
			players[other_id].a_resize(0, my_packet->direction * 50, 50, 50);
		}
		break;
	}

	case SC_REMOVE_OBJECT:
	{
		SC_REMOVE_OBJECT_PACKET* my_packet = reinterpret_cast<SC_REMOVE_OBJECT_PACKET*>(ptr);
		int other_id = my_packet->id;
		if (other_id == g_myid) {
			avatar.hide();
		}
		else {
			players.erase(other_id);
		}
		break;
	}
	case SC_CHAT:
	{
		SC_CHAT_PACKET* my_packet = reinterpret_cast<SC_CHAT_PACKET*>(ptr);
		int other_id = my_packet->id;
		if (other_id == g_myid) {
			avatar.set_chat(my_packet->mess);
		}
		else {
			players[other_id].set_chat(my_packet->mess);
		}

		break;
	}
	default:
		printf("Unknown PACKET type [%d]\n", ptr[1]);
	}
}

void process_data(char* net_buf, size_t io_byte)
{
	char* ptr = net_buf;
	static size_t in_packet_size = 0;
	static size_t saved_packet_size = 0;
	static char packet_buffer[BUF_SIZE];

	while (0 != io_byte) {
		if (0 == in_packet_size) in_packet_size = ptr[0];
		if (io_byte + saved_packet_size >= in_packet_size) {
			memcpy(packet_buffer + saved_packet_size, ptr, in_packet_size - saved_packet_size);
			ProcessPacket(packet_buffer);
			ptr += in_packet_size - saved_packet_size;
			io_byte -= in_packet_size - saved_packet_size;
			in_packet_size = 0;
			saved_packet_size = 0;
		}
		else {
			memcpy(packet_buffer + saved_packet_size, ptr, io_byte);
			saved_packet_size += io_byte;
			io_byte = 0;
		}
	}
}

void client_main()
{
	char net_buf[BUF_SIZE];
	size_t	received;

	auto recv_result = s_socket.receive(net_buf, BUF_SIZE, received);
	if (recv_result == sf::Socket::Error)
	{
		wcout << L"Recv ����!";
		exit(-1);
	}
	if (recv_result == sf::Socket::Disconnected) {
		wcout << L"Disconnected\n";
		exit(-1);
	}
	if (recv_result != sf::Socket::NotReady)
		if (received > 0) process_data(net_buf, received);

	for (int i = 0; i < SCREEN_WIDTH; ++i)
		for (int j = 0; j < SCREEN_HEIGHT; ++j)
		{
			int tile_x = i + g_left_x;
			int tile_y = j + g_top_y;
			if ((tile_x < 0) || (tile_y < 0)) continue;
			if (GridMap[tile_x][tile_y]) {
				white_tile.a_move(TILE_WIDTH * i, TILE_WIDTH * j);
				white_tile.a_draw();
			}
			else
			{
				black_tile.a_move(TILE_WIDTH * i, TILE_WIDTH * j);
				black_tile.a_draw();
			}
		}
	HPBAR[0].a_draw();
	Level.a_draw();
	avatar.draw();
	for (auto& pl : players) pl.second.draw();
	g_window->draw(Level_Text);
}

void send_packet(void* packet)
{
	unsigned char* p = reinterpret_cast<unsigned char*>(packet);
	size_t sent = 0;
	s_socket.send(packet, p[0], sent);
}

int main()
{
	wcout.imbue(locale("korean"));
	sf::Socket::Status status = s_socket.connect("127.0.0.1", PORT_NUM);
	s_socket.setBlocking(false);

	if (status != sf::Socket::Done) {
		wcout << L"Connect Failed.\n";
		exit(-1);
	}

	client_initialize();
	CS_LOGIN_PACKET p;
	p.size = sizeof(p);
	p.type = CS_LOGIN;

	string player_name{ "P" };
	player_name += to_string(GetCurrentProcessId());

	strcpy_s(p.name, player_name.c_str());
	send_packet(&p);
	avatar.set_name(p.name);

	sf::RenderWindow window(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "2D CLIENT");
	g_window = &window;

	while (window.isOpen())
	{
		break;
	}

	while (window.isOpen())
	{
		sf::Event event;
		while (window.pollEvent(event))
		{
			if (event.type == sf::Event::Closed)
				window.close();
			if (event.type == sf::Event::KeyPressed) {
				if (on_chat) {
					if (event.key.code >= sf::Keyboard::A && event.key.code <= sf::Keyboard::Z)
					{
						char inputChar = static_cast<char>('A' + (event.key.code - sf::Keyboard::A));
						// 알파벳 키 입력 처리 로직
						m_mess[chat_length++] = inputChar;
					}
					else if (event.key.code >= sf::Keyboard::Num0 && event.key.code <= sf::Keyboard::Num9)
					{
						char inputChar = static_cast<char>('0' + (event.key.code - sf::Keyboard::Num0));
						// 숫자 키 입력 처리 로직
						m_mess[chat_length++] = inputChar;
					}
					else if (event.key.code == sf::Keyboard::Backspace)
					{
						chat_length--;
						m_mess[chat_length] = '\0';
					}
					else if (event.key.code == sf::Keyboard::Enter)
					{
						on_chat = false;
						chat_length = 0;
						memset(m_mess, 0, CHAT_SIZE);
					}
					avatar.set_chat(m_mess);
					break;
				}
				int direction = -1;
				switch (event.key.code) {
				case sf::Keyboard::Down:
					direction = 0;
					break;
				case sf::Keyboard::Left:
					direction = 1;
					break;
				case sf::Keyboard::Up:
					direction = 2;
					break;
				case sf::Keyboard::Right:
					direction = 3;
					break;
				case sf::Keyboard::Return:
					on_chat = true;
					break;
				case sf::Keyboard::Escape:
					window.close();
					break;
				case sf::Keyboard::Space:
					CS_ATTACK_PACKET p;
					p.size = sizeof(p);
					p.type = CS_ATTACK;
					send_packet(&p);
					continue;
				}
				if (-1 != direction) {
					CS_MOVE_PACKET p;
					p.size = sizeof(p);
					p.type = CS_MOVE;
					p.direction = direction;
					send_packet(&p);
					continue;
				}
			}
		}

		window.clear();
		client_main();
		window.display();
	}
	client_finish();

	return 0;
}