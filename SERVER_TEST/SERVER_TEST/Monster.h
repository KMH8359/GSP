#pragma once
#include "CHARACTER.h"

short Heuristic(const TILEPOINT& start, const TILEPOINT& dest);
stack<TILEPOINT> ReconstructPath(A_star_Node* node);

array<array<bool, W_WIDTH>, W_HEIGHT> GridMap;

TILEPOINT vec[4]{
	TILEPOINT(-1,0),
	TILEPOINT(1,0),
	TILEPOINT(0,1),
	TILEPOINT(0,-1),
};

enum ATTACK_TYPE { PEACE, AGRO };	// PEACE : 선빵 맞기 전까지 대기,	AGRO : 시야에 보이면 바로 추격
enum MOVE_TYPE{ LOCKED, ROAMING };	// LOCKED : 이동 못함
class MONSTER : public CHARACTER {
public:
	atomic_bool	_is_active;
	int target_id = -1;
	ATTACK_TYPE a_type;
	MOVE_TYPE m_type;
	std::stack<TILEPOINT> path;
public:
	AStar_Pool* m_pool;
	MONSTER()
	{
		m_pool = new AStar_Pool(300);
		HP = MAX_HP = 200;
	}

	~MONSTER() 
	{
		delete m_pool;
	}

	void set_direction(int dx, int dy)
	{
		if (dx == 0 && dy == 1)
			direction = 0;  // 상
		else if (dx == -1 && dy == 0)
			direction = 1;  // 좌
		else if (dx == 0 && dy == -1)
			direction = 2;  // 하
		else if (dx == 1 && dy == 0)
			direction = 3;  // 우
	}

	void move();


	stack<TILEPOINT> Trace_Player(const TILEPOINT start, const TILEPOINT dest)
	{
		if (start == dest || !GridMap[dest.x][dest.y]) return stack<TILEPOINT>();

		unordered_set<TILEPOINT, PointHash, PointEqual> closelist{};
		unordered_map<TILEPOINT, A_star_Node*, PointHash, PointEqual> openlist;
		priority_queue<A_star_Node*, vector<A_star_Node*>, CompareNodes> pq;

		openlist.reserve(300);
		A_star_Node* startNode = m_pool->GetMemory(start, 0.f, Heuristic(start, dest), nullptr);
		openlist.insert(make_pair(start, startNode));
		pq.push(startNode);

		while (!pq.empty())
		{
			A_star_Node* current = pq.top();
			pq.pop();
			if (current == nullptr) continue;

			if (current->Pos == dest) {
				stack<TILEPOINT> path = ReconstructPath(current);
				for (const auto& pair : openlist) {
					m_pool->ReturnMemory(pair.second);  // 노드 반환
				}
				return path;
			}

			closelist.insert(current->Pos);
			int neighborG = current->G + 1;

			for (int i = 0; i < 4; ++i) {
				TILEPOINT neighborPos = current->Pos + vec[i];

				if (GridMap[neighborPos.x][neighborPos.y] == false || closelist.count(neighborPos) > 0)
					continue;

				auto neighborIter = openlist.find(neighborPos);
				if (neighborIter == openlist.end()) {
					float neighborH = Heuristic(neighborPos, dest);
					A_star_Node* neighborNode = m_pool->GetMemory(neighborPos, neighborG, neighborH, current);
					openlist.insert(make_pair(neighborPos, neighborNode));
					pq.push(neighborNode);
				}
				else if (neighborG < neighborIter->second->G)
				{
					float neighborH = Heuristic(neighborPos, dest);
					neighborIter->second->G = neighborG;
					neighborIter->second->H = neighborH;
					neighborIter->second->F = neighborG + neighborH;
					neighborIter->second->parent = current;
				}
			}
		}

		m_pool->ReturnMemory(startNode);
		for (const auto& pair : openlist) {
			m_pool->ReturnMemory(pair.second);
		}
		cout << "FAILED\n";

		return stack<TILEPOINT>();
	}
};

short Heuristic(const TILEPOINT& start, const TILEPOINT& dest) {
	return abs(dest.y - start.y) + abs(dest.x - start.x);
}

stack<TILEPOINT> ReconstructPath(A_star_Node* node) {
	stack<TILEPOINT> path;
	while (node->parent) {
		path.push(node->Pos);
		node = node->parent;
	}
	return path;
}