#include <iostream>
#include <fstream>
#include <vector>
#include <random>

enum ATTACK_TYPE { PEACE, AGRO };
enum MOVE_TYPE { LOCKED, ROAMING };

class CHARACTER {
public:
    int _id;
    int HP;
    int Level;
    int EXP;
};

class MONSTER : public CHARACTER {
public:
    int target_id = -1;
    ATTACK_TYPE a_type;
    MOVE_TYPE m_type;
public:
    MONSTER()  {}
};

// 몬스터 정보를 파일에 저장하는 함수
void saveMonstersToFile(const std::vector<MONSTER>& monsters, const std::string& filename) {
    std::ofstream file(filename);
    if (file.is_open()) {
        for (const auto& monster : monsters) {
            file << "HP: " << monster.HP << "\n";
            file << "Level: " << monster.Level << "\n";
            file << "EXP: " << monster.EXP << "\n";
            file << "Attack Type: " << monster.a_type << "\n";
            file << "Move Type: " << monster.m_type << "\n\n";
        }
        file.close();
        std::cout << "Monsters saved to file: " << filename << std::endl;
    }
    else {
        std::cerr << "Failed to open file for writing." << std::endl;
    }
}

int main() {
    int numMonsters = 200000; // 생성할 몬스터의 개수

    std::vector<MONSTER> monsters;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> levelDist(1, 10);
    std::uniform_int_distribution<int> attackTypeDist(0, 1);
    std::uniform_int_distribution<int> moveTypeDist(0, 1);

    for (int i = 0; i < numMonsters; i++) {
        MONSTER monster;
        monster.Level = levelDist(gen);
        monster.HP = monster.Level * 100;
        monster.a_type = static_cast<ATTACK_TYPE>(attackTypeDist(gen));
        monster.m_type = static_cast<MOVE_TYPE>(moveTypeDist(gen));
        monster.EXP = monster.Level * monster.Level * 2 * ((int)monster.a_type + 1) * ((int)monster.m_type + 1);

        monsters.push_back(monster);
    }

    std::string filename = "monsters.txt";
    saveMonstersToFile(monsters, filename);

    return 0;
}