#include <ctime>
#include <set>

#ifndef I_GAME_H
#define I_GAME_H

namespace Tmpl8 {

#define MAXP1					40000		// increase to test your optimized code
#define MAXP2					(4 * MAXP1)	// because the player is smarter than the AI
#define MAXBULLET				200
#define GRID_WIDTH				128			// the number of cells
#define GRID_HEIGHT				((MAXP1 / 4) + 10)
#define GRID_CELL_SIZE_SMALL	16			// the size of each cell, in pixels
#define GRID_CELL_SIZE_LARGE	128	
#define MAX_CELL_COUNT_SMALL	16
#define MAX_CELL_COUNT_LARGE	256

class Grid;
class Smoke
{
public:
	struct Puff { int x, y, vy, life; };
	Smoke() : active(false), frame(0) {};
	void Tick();
	Puff puff[8];
	bool active;
	int frame, xpos, ypos;
};

class Tank
{
public:
	enum { ACTIVE = 1, P1 = 2, P2 = 4 };
	Tank() : pos(vec2(0, 0)), speed(vec2(0, 0)), target(vec2(0, 0)), reloading(0) {};
	~Tank();
	void Fire(unsigned int party, vec2& pos, vec2& dir);
	void Tick();
	vec2 pos, speed, target;
	float maxspeed;
	int flags, reloading;
	Smoke smoke;
	vec2 peakForce;
	std::pair<int, int> cellPos;
};

class Bullet
{
public:
	enum { ACTIVE = 1, P1 = 2, P2 = 4 };
	Bullet() : flags(0) {};
	void Tick();
	vec2 pos, speed;
	int flags;
};

class SmallGrid
{
public:
	int cells[GRID_WIDTH][GRID_HEIGHT][MAX_CELL_COUNT_SMALL];
	int cellCounts[GRID_WIDTH][GRID_HEIGHT];
	
	SmallGrid();
	std::pair<int, int> GetIndices(vec2 pos) const;
	vec2 TankForces(Tank* tank);
	Tank* BulletCollision(vec2 pos, int team);
};

class LargeGrid
{
public:
	int cells[GRID_WIDTH / 8][GRID_HEIGHT / 8][MAX_CELL_COUNT_LARGE];
	int cellCounts[GRID_WIDTH / 8][GRID_HEIGHT / 8];

	LargeGrid();
	std::pair<int, int> GetIndices(vec2 pos) const;
	Tank* FindTarget(Tank* source);
};

class Surface;
class Surface8;
class Sprite;
class Game
{
public:
	void EvadeMountainPeaks();
	void SetTarget(Surface* a_Surface) { m_Surface = a_Surface; }
	void MouseMove(int x, int y) { m_MouseX = x; m_MouseY = y; }
	void MouseButton(bool b) { m_LButton = b; }
	void Init();
	void UpdateForces();
	void UpdateTanks();
	void UpdateGrid();
	void UpdateBullets();
	void ClearGrid();
	void DrawTanks();
	void PlayerInput();
	void Tick(float a_DT);
	Surface* m_Surface, *m_Backdrop, *m_Heights, *m_Grid;
	Sprite* m_P1Sprite, *m_P2Sprite, *m_PXSprite, *m_Smoke;
	int m_ActiveP1, m_ActiveP2;
	int m_MouseX, m_MouseY, m_DStartX, m_DStartY, m_DFrames;
	bool m_LButton, m_PrevButton;
	Tank** m_Tank;
	std::clock_t last;
	SmallGrid grid;
	LargeGrid aimP1, aimP2;
	float costable[720], sintable[720];
	std::vector<std::pair<int, int>> nonEmptyCells;
	bool nearMountainPeak[16][GRID_WIDTH][GRID_HEIGHT];
};

}; // namespace Templ8

#endif