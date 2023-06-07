#include <iostream>
#include <vector>
#include <random>
#include <fstream>

const int W_WIDTH = 2000;
enum ATTACK_TYPE { PEACE, AGRO };
enum MOVE_TYPE { LOCKED, ROAMING };

struct TILEPOINT
{
    short x;
    short y;

    TILEPOINT operator+(const TILEPOINT& other) const {
        TILEPOINT result;
        result.x = this->x + other.x;
        result.y = this->y + other.y;
        return result;
    }

    bool operator==(const TILEPOINT& other) const {
        return this->x == other.x && this->y == other.y;
    }
};

class CHARACTER {
public:
    int _id;
    int HP;
    int Level;
    int EXP;
    TILEPOINT point;
};

class MONSTER : public CHARACTER {
public:
    int target_id = -1;
    ATTACK_TYPE a_type;
    MOVE_TYPE m_type;
public:
    MONSTER() {}
};

// 몬스터 정보를 파일에 저장하는 함수
void saveMonstersToFile(const std::vector<MONSTER>& monsters, const std::string& filename) {
    int cnt = 0;
    std::ofstream file(filename);
    if (file.is_open()) {
        for (const auto& monster : monsters) {
            file << "x: " << monster.point.x << "\n";
            file << "y: " << monster.point.y << "\n";
            file << "HP: " << monster.HP << "\n";
            file << "Level: " << monster.Level << "\n";
            file << "EXP: " << monster.EXP << "\n";
            file << "Attack Type: " << monster.a_type << "\n";
            file << "Move Type: " << monster.m_type << "\n\n";
            cnt++;
        }
        file.close();
        std::cout << cnt << " Monsters saved to file: " << filename << std::endl;
    }
    else {
        std::cerr << "Failed to open file for writing." << std::endl;
    }
}

// 그리드맵을 랜덤하게 생성하는 함수
std::vector<std::vector<int>> createRandomGridMap(int rows, int cols, float obstacleProbability) {
    std::vector<std::vector<int>> gridMap(rows, std::vector<int>(cols, 0));
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 1.0);

    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            float randNum = dis(gen);
            if (randNum <= obstacleProbability) {
                gridMap[i][j] = 0;
            }
            else {
                gridMap[i][j] = 1;
            }
        }
    }

    return gridMap;
}

// 그리드맵을 파일로 저장하는 함수
void saveGridMapToFile(const std::vector<std::vector<int>>& gridMap, const std::string& filename) {
    std::ofstream file(filename);
    if (file.is_open()) {
        for (const auto& row : gridMap) {
            for (const auto& cell : row) {
                file << cell;
            }
            file << "\n";
        }
        file.close();
    }
    else {
        std::cerr << "Failed to open file for writing." << std::endl;
    }
}

// 파일에서 그리드맵을 읽어오는 함수
void initializeMap(std::vector<std::vector<int>>& gridMap, const std::string& filename) {
    std::ifstream file(filename);
    if (file.is_open()) {
        for (int i = 0; i < W_WIDTH; ++i) {
            for (int j = 0; j < W_WIDTH; ++j) {
                char value;
                file >> value;
                gridMap[i][j] = (value == '1');
            }
        }
        file.close();
    }
    else {
        std::cerr << "Failed to open file for reading." << std::endl;
    }
}

int main() {
    int rows = 2000;
    int cols = 2000;
    float obstacleProbability = 0.1; // 장애물 생성 확률 (0 ~ 1)

    std::vector<std::vector<int>> gridMap = createRandomGridMap(rows, cols, obstacleProbability);

    // 그리드맵을 파일에 저장
    saveGridMapToFile(gridMap, "map.txt");

    // 파일에서 그리드맵 읽어오기
    std::vector<std::vector<int>> loadedGridMap(rows, std::vector<int>(cols, 0));
    initializeMap(loadedGridMap, "map.txt");

    // 그리드맵 출력
    //for (int i = 0; i < rows; i++) {
    //    for (int j = 0; j < cols; j++) {
    //        std::cout << loadedGridMap[i][j] << " ";
    //    }
    //    std::cout << std::endl;
    //}



    int numMonsters = 200000; // 생성할 몬스터의 개수

    std::vector<MONSTER> monsters;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> posDist(0, 1999);
    std::uniform_int_distribution<int> levelDist(1, 10);
    std::uniform_int_distribution<int> attackTypeDist(0, 1);
    std::uniform_int_distribution<int> moveTypeDist(0, 1);

    for (int i = 0; i < numMonsters; i++) {
        MONSTER monster;
        while (1) {
            monster.point.x = posDist(gen);
            monster.point.y = posDist(gen);
            if (gridMap[monster.point.x][monster.point.y]) break;
        }
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