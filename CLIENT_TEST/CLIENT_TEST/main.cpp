#include "stdafx.h"
#include "protocol.h"



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
sf::Text Login_ID_Text;
sf::Text Login_PW_Text;
bool on_chat = false;

char m_mess[CHAT_SIZE]{ "" };
char m_login_string[2][NAME_SIZE]{ "" };
unsigned short chat_length = 0;
unsigned short login_state = 0;

sf::Sprite* game_logo;
sf::Texture* logo_image;

sf::Sprite* background;
sf::Texture* background_image;
void ConvertCharArrayToWideCharArray(const char* source, size_t sourceSize, wchar_t* destination, size_t destinationSize)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
	std::wstring wideString = converter.from_bytes(source, source + sourceSize);

	wcsncpy(destination, wideString.c_str(), destinationSize - 1);
	destination[destinationSize - 1] = L'\0'; 
}

class CHATBOX {
private:
	sf::Sprite m_sprite;
	vector<sf::Text> m_chats;
	int m_maxChatCount; 
public:
	bool m_showing;
	CHATBOX(sf::Texture& t, int x, int y, int x2, int y2) {
		m_showing = false;
		m_sprite.setTexture(t);
		m_sprite.setTextureRect(sf::IntRect(x, y, x2, y2));
		m_maxChatCount = 10;
	}
	CHATBOX() {
		m_showing = false;
		m_maxChatCount = 10;
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
		refreshChatPositions();
	}

	void a_draw() {
		g_window->draw(m_sprite);
	}

	void t_draw() {
		for (const auto& text : m_chats)
			g_window->draw(text);
	}

	void draw() {
		if (false == m_showing) return;
		a_draw();
		t_draw();		
		//float rx = (m_x - g_left_x) * 50.f;
		//float ry = (m_y - g_top_y) * 50.f;
		//m_sprite.setPosition(rx, ry);
		//g_window->draw(m_sprite);
		//auto size = m_name.getGlobalBounds();
		//if (m_mess_end_time < chrono::system_clock::now()) {
		//}
		//else {
		//	m_chat.setPosition(rx + 32 - size.width / 2, ry - 10);
		//	g_window->draw(m_chat);
		//}
	}

	void add_chat(const char str[]) {
		sf::Text m_chat;
		m_chat.setFont(g_font);
		m_chat.setString(str);
		m_chat.setFillColor(sf::Color(255, 255, 255));
		m_chat.setStyle(sf::Text::Bold);
		m_chat.setCharacterSize(35);

		m_chats.push_back(m_chat);

		if (m_chats.size() > m_maxChatCount) 
			m_chats.erase(m_chats.begin());
		refreshChatPositions();
	}

	void refreshChatPositions() {
		int x = m_sprite.getPosition().x;
		int y = m_sprite.getPosition().y;

		for (std::size_t i = 0; i < m_chats.size(); ++i) {
			m_chats[i].setPosition(x, y + 50 * i);
		}
	}
};

class OBJECT {
private:
	bool m_showing;

	sf::Text m_name;
	sf::Text m_chat;
	//chrono::system_clock::time_point m_mess_end_time;
public:
	sf::Sprite m_sprite;
	int		id;
	int		m_x, m_y;
	short	HP = 0;
	short	MAX_HP = 0;
	int		Exp = 0;
	int		Level = 0;

	short left = 0;
	short top = 0;

	//chrono::system_clock::time_point m_lastFrameTime;
	char name[NAME_SIZE];
	OBJECT(sf::Texture& t, int x, int y, int x2, int y2) {
		m_showing = false;
		m_sprite.setTexture(t);
		m_sprite.setTextureRect(sf::IntRect(x, y, x2, y2));

		left = x;
		top = y;
		set_name("NONAME");
		//m_lastFrameTime = chrono::system_clock::now();
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
		//auto size = m_name.getGlobalBounds();
		//if (m_mess_end_time < chrono::system_clock::now()) {
		//}
		//else {
		//	m_chat.setPosition(rx + 32 - size.width / 2, ry - 10);
		//	g_window->draw(m_chat);
		//}
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
		//m_mess_end_time = chrono::system_clock::now() + chrono::seconds(3);
	}

	void animate()
	{
		//auto now = chrono::system_clock::now();
		//auto elapsed = now - m_lastFrameTime;
		//if (elapsed >= chrono::seconds(1))
		//{
		auto rect = m_sprite.getTextureRect();
		rect.left = (rect.left + 50) % 150 + left;
		m_sprite.setTextureRect(rect);
		//m_lastFrameTime = now;
		//}
	}
};

OBJECT avatar;
unordered_map <int, OBJECT> players;
array<array<bool, W_WIDTH>, W_WIDTH> GridMap;
OBJECT white_tile;
OBJECT black_tile;
OBJECT HPBAR[2];
OBJECT Level;
CHATBOX Chat_Box;

#pragma region LOGIN_SCENE
OBJECT* LOGIN_BAR[2];
OBJECT* Login_UI;
#pragma endregion

sf::Texture* board;
sf::Texture* pieces;
sf::Texture* monsters;
sf::Texture* hp_bar;
sf::Texture* lv;
sf::Texture* login_ui;
sf::Texture* chat_box;

void client_initialize()
{
	game_logo = new sf::Sprite;
	background = new sf::Sprite;

	board = new sf::Texture;
	pieces = new sf::Texture;
	monsters = new sf::Texture;
	lv = new sf::Texture;
	hp_bar = new sf::Texture;
	login_ui = new sf::Texture;
	logo_image = new sf::Texture;
	chat_box = new sf::Texture;
	background_image = new sf::Texture;

	board->loadFromFile("tilemap.bmp");
	pieces->loadFromFile("character.png");
	monsters->loadFromFile("skeleton.png");
	lv->loadFromFile("Lv.png");
	hp_bar->loadFromFile("HP_ON.png");
	login_ui->loadFromFile("loginUI.png");
	logo_image->loadFromFile("logo.png");
	chat_box->loadFromFile("chat_box.png");
	background_image->loadFromFile("bg.png");
	if (false == g_font.loadFromFile("cour.ttf")) {
		cout << "Font Loading Error!\n";
		exit(-1);
	}
	white_tile = OBJECT{ *board, 13, 112, TILE_WIDTH, TILE_WIDTH };
	black_tile = OBJECT{ *board, 13, 8, TILE_WIDTH, TILE_WIDTH };

	Level = OBJECT{ *lv, 0, 0, 100, 100 };

	Chat_Box = CHATBOX{ *chat_box, 0, 0, 500, 350 };
	Chat_Box.a_move(0, 650);

	for (int i = 0; i < 2; ++i) {
		LOGIN_BAR[i] = new OBJECT{ *chat_box, 0, 0, 400, 100 };
		LOGIN_BAR[i]->a_move(250, 300 + i * 300);
	}
	Login_UI = new OBJECT{ *login_ui, 0, 0, 300, 80 };
	Login_UI->a_move(350, 800);

	game_logo->setTexture(*logo_image);
	game_logo->setPosition(0, 100);

	background->setTexture(*background_image);
	background->setPosition(0, 0);

	Login_ID_Text.setFont(g_font);
	Login_ID_Text.setCharacterSize(60);
	Login_ID_Text.setFillColor(sf::Color(0, 0, 0));
	Login_ID_Text.setPosition(250, 300);

	Login_PW_Text.setFont(g_font);
	Login_PW_Text.setCharacterSize(60);
	Login_PW_Text.setFillColor(sf::Color(0, 0, 0));
	Login_PW_Text.setPosition(250, 600);


	Level_Text.setFont(g_font);
	Level_Text.setCharacterSize(60);
	Level_Text.setFillColor(sf::Color(255, 0, 0));
	Level_Text.setPosition(30, 5);


	HPBAR[0] = OBJECT{ *hp_bar, 0, 0, avatar.HP, 50};
	HPBAR[0].a_move(100, 0);
	HPBAR[1] = OBJECT{ *hp_bar, 0, 0, -1 * (avatar.MAX_HP - avatar.HP), 50 };
	HPBAR[1].a_move(100 + avatar.HP, 0);
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

	avatar = OBJECT{ *pieces, 0, 200, 50, 50 };
}

void ProcessPacket(char* ptr)
{
	switch (ptr[2])
	{
	case SC_LOGIN_INFO:
	{
		SC_LOGIN_INFO_PACKET* packet = reinterpret_cast<SC_LOGIN_INFO_PACKET*>(ptr);
		g_myid = packet->id;
		avatar.id = g_myid;
		avatar.HP = packet->hp;
		avatar.MAX_HP = packet->max_hp;
		cout << avatar.HP << "/" << avatar.MAX_HP << endl;
		HPBAR[0].a_resize(0, 0, avatar.HP, 50);
		HPBAR[1].a_resize(0, 0, -1 * (avatar.MAX_HP - avatar.HP), 50);
		HPBAR[1].a_move(100 + avatar.HP, 0);
		avatar.Exp = packet->exp;
		avatar.Level = packet->level;
		avatar.move(packet->point.x, packet->point.y);
		g_left_x = packet->point.x - SCREEN_WIDTH / 2;
		g_top_y = packet->point.y - SCREEN_HEIGHT / 2;
		strcpy_s(avatar.name, sizeof(avatar.name), packet->name);
		char buf[8];
		sprintf_s(buf, "%d", avatar.Level);
		Level_Text.setString(buf);
		avatar.show();
		login_state = 3;
	}
	break;
	case SC_LOGIN_FAIL:
	{
		//login_state = 0;
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
			strcpy_s(players[id].name, sizeof(players[id].name), my_packet->name);
			players[id].move(my_packet->point.x, my_packet->point.y);
			players[id].set_name(my_packet->name);
			players[id].show();
		}
		else { // id % 4로 몬스터 타입이 결정됨
			int type = (int)my_packet->monster_type % 4;
			players[id] = OBJECT{ *monsters, (type / 2) * 150,  (type % 2) * 200, 50, 50 };
			players[id].id = id;
			players[id].move(my_packet->point.x, my_packet->point.y);
			//players[id].set_name(my_packet->name);
			players[id].show();
		}
		break;
	}
	case SC_MOVE_OBJECT:
	{
		SC_MOVE_OBJECT_PACKET* my_packet = reinterpret_cast<SC_MOVE_OBJECT_PACKET*>(ptr);
		int other_id = my_packet->id;
		if (other_id == g_myid) {
			cout << my_packet->point.x << ", " << my_packet->point.y << endl;
			avatar.move(my_packet->point.x, my_packet->point.y);
			g_left_x = my_packet->point.x - SCREEN_WIDTH / 2;
			g_top_y = my_packet->point.y - SCREEN_HEIGHT / 2;
			auto rect = avatar.m_sprite.getTextureRect();
			rect.top = avatar.top + my_packet->direction * 50;
			avatar.m_sprite.setTextureRect(rect);
			avatar.animate();
			//avatar.a_resize(0, my_packet->direction * 50, 50, 50);
		}
		else {
			players[other_id].move(my_packet->point.x, my_packet->point.y);	
			auto rect = players[other_id].m_sprite.getTextureRect();
			rect.top = players[other_id].top + my_packet->direction * 50;
			players[other_id].m_sprite.setTextureRect(rect);
			players[other_id].animate();
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
		char result[100]{};  
		if (my_packet->id == g_myid) strcpy(result, avatar.name);  
		else strcpy(result, players[my_packet->id].name);  
		strcat(result, " : ");  
		strcat(result, my_packet->mess);  
		Chat_Box.add_chat(result);

		break;
	}
	case SC_STAT_CHANGE:
	{
		SC_STAT_CHANGE_PACKET* my_packet = reinterpret_cast<SC_STAT_CHANGE_PACKET*>(ptr);
		avatar.HP = my_packet->hp;
		avatar.Level = my_packet->level;
		avatar.MAX_HP = my_packet->max_hp;
		avatar.Exp = my_packet->exp;
		HPBAR[0].a_resize(0, 0, avatar.HP, 50);
		HPBAR[1].a_resize(0, 0, -1 * (avatar.MAX_HP - avatar.HP), 50);
		cout << avatar.HP << " / " << avatar.MAX_HP << endl;
		HPBAR[1].a_move(100 + avatar.HP, 0);
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

void login_scene()
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



	//for (int i = 0; i < SCREEN_WIDTH; ++i)
	//	for (int j = 0; j < SCREEN_HEIGHT; ++j)
	//	{
	//		white_tile.a_move(TILE_WIDTH * i, TILE_WIDTH * j);
	//		white_tile.a_draw();
	//	}
	g_window->draw(*background);

	
	Login_ID_Text.setString(m_login_string[0]);
	Login_PW_Text.setString(m_login_string[1]);

	for (int i = 0; i < 2; ++i) {
		LOGIN_BAR[i]->a_draw();
	}
	g_window->draw(Login_ID_Text);
	g_window->draw(Login_PW_Text);
	g_window->draw(*game_logo);
	Login_UI->a_draw();
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
	avatar.draw();
	for (auto& pl : players) {
		pl.second.draw();
	}
	HPBAR[0].a_draw();
	HPBAR[1].a_draw();
	Level.a_draw();
	Chat_Box.draw();
	g_window->draw(Level_Text);
}

void send_packet(void* packet)
{
	unsigned char* p = reinterpret_cast<unsigned char*>(packet);
	size_t sent = 0;
	s_socket.send(packet, p[0], sent);
}

void client_finish()
{
	CS_LOGOUT_PACKET p;
	p.size = sizeof(p);
	p.type = CS_LOGOUT;
	send_packet(&p);
	players.clear();
	delete board;
	delete pieces;
	delete monsters;
	delete hp_bar;
	delete lv;
	delete login_ui;
	delete logo_image;
	delete chat_box;
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


	sf::RenderWindow window(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "2D CLIENT");
	g_window = &window;
	client_initialize();

	while (window.isOpen())
	{
		if (login_state > 2) break;
		sf::Event event;
		while (window.pollEvent(event))
		{
			if (event.type == sf::Event::Closed)
				window.close();

			if (event.type == sf::Event::MouseButtonPressed && login_state == 2) {
				int mouseX = event.mouseButton.x;
				int mouseY = event.mouseButton.y;
				if (mouseX >= 350 && mouseX <= 650) {
					if (mouseY >= 800 && mouseY < 840) {
						wchar_t* wcharArray = new wchar_t[NAME_SIZE] {L""};
						CS_LOGIN_PACKET p;
						p.size = sizeof(p);
						p.type = CS_LOGIN;

						ConvertCharArrayToWideCharArray(m_login_string[0], sizeof(m_login_string[0]), wcharArray, sizeof(wcharArray));
						wcscpy_s(p.id, sizeof(p.id) / sizeof(p.id[0]), wcharArray);

						memset(wcharArray, 0, NAME_SIZE * sizeof(wchar_t));

						ConvertCharArrayToWideCharArray(m_login_string[1], sizeof(m_login_string[1]), wcharArray, sizeof(wcharArray));
						wcscpy_s(p.password, sizeof(p.password) / sizeof(p.password[0]), wcharArray);


						send_packet(&p);
						for (int i = 0; i < 2; ++i) {
							memset(m_login_string[i], 0, NAME_SIZE);
						}
						delete[] wcharArray;
						login_state = 0;
					}
					else if (mouseY >= 840 && mouseY <= 880) {
						wchar_t* wcharArray = new wchar_t[NAME_SIZE] {L""};
						CS_LOGIN_PACKET p;
						p.size = sizeof(p);
						p.type = CS_SIGNUP;

						ConvertCharArrayToWideCharArray(m_login_string[0], sizeof(m_login_string[0]), wcharArray, sizeof(wcharArray));
						wcscpy_s(p.id, sizeof(p.id) / sizeof(p.id[0]), wcharArray);

						memset(wcharArray, 0, NAME_SIZE * sizeof(wchar_t));

						ConvertCharArrayToWideCharArray(m_login_string[1], sizeof(m_login_string[1]), wcharArray, sizeof(wcharArray));
						wcscpy_s(p.password, sizeof(p.password) / sizeof(p.password[0]), wcharArray);


						send_packet(&p);
						for (int i = 0; i < 2; ++i) {
							memset(m_login_string[i], 0, NAME_SIZE);
						}
						delete[] wcharArray;
						login_state = 0;
					}
				}
			}
			if (event.type == sf::Event::KeyPressed && login_state < 2) {

				if (event.key.code >= sf::Keyboard::A && event.key.code <= sf::Keyboard::Z)
				{
					char inputChar = static_cast<char>('a' + (event.key.code - sf::Keyboard::A));
					m_login_string[login_state][chat_length++] = inputChar;
				}
				else if (event.key.code >= sf::Keyboard::Num0 && event.key.code <= sf::Keyboard::Num9)
				{
					char inputChar = static_cast<char>('0' + (event.key.code - sf::Keyboard::Num0));
					m_login_string[login_state][chat_length++] = inputChar;
				}

				else if (event.key.code == sf::Keyboard::Backspace)
				{
					if (chat_length) {
						m_login_string[login_state][--chat_length] = '\0';
					}
				}
				else if (event.key.code == sf::Keyboard::Enter)
				{
					login_state++;
					chat_length = 0;
				}
			}
		}
		window.clear();
		login_scene();
		window.display();
	}

	delete logo_image;
	delete game_logo;
	delete background_image;
	delete background;
	delete Login_UI;
	for (int i = 0; i < 2; ++i)
		delete LOGIN_BAR[i];

	while (window.isOpen())
	{
		sf::Event event;
		while (window.pollEvent(event))
		{
			if (event.type == sf::Event::Closed)
				window.close();

			if (event.type == sf::Event::TextEntered && on_chat) {
				//if (event.key.code >= sf::Keyboard::A && event.key.code <= sf::Keyboard::Z)
				if (event.text.unicode < 128)
				{
					char inputChar = static_cast<char>(event.text.unicode);
					// 알파벳 키 입력 처리 로직
					m_mess[chat_length++] = inputChar;
				}
				else
				{
					// ASCII 문자 이외의 문자 (한글 등)
					wchar_t inputChar = static_cast<wchar_t>(event.text.unicode);
					// 입력된 문자 처리 로직
					m_mess[chat_length++] = inputChar;
				}
				break;
			}
			if (event.type == sf::Event::KeyPressed) {
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
					if (on_chat) {
						CS_CHAT_PACKET p;
						p.size = sizeof(p);
						p.type = CS_CHAT;
						strcpy_s(p.mess, sizeof(m_mess), m_mess);
						send_packet(&p);
						on_chat = false;
						chat_length = 0;
						memset(m_mess, 0, CHAT_SIZE);
					}
					else {
						on_chat = true;
					}
					break;
				case sf::Keyboard::Comma: {
					if (Chat_Box.m_showing) Chat_Box.hide();
					else Chat_Box.show();
				}
					break;
				case sf::Keyboard::Backspace:
					if (chat_length && on_chat) {
						m_mess[--chat_length] = '\0';
					}
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