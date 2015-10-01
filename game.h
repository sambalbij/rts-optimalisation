#include <ctime>
#include <list>

#ifndef I_GAME_H
#define I_GAME_H

namespace Tmpl8 {

#define MAXP1			80			// increase to test your optimized code
#define MAXP2			(4 * MAXP1)	// because the player is smarter than the AI
#define MAXBULLET		200
#define GRID_WIDTH		100			// the number of cells
#define GRID_HEIGHT		100
#define GRID_CELL_SIZE	16			// the size of each cell, in pixels

class Smoke
{
public:
	struct Puff { int x, y, vy, life; };
	Smoke() : active( false ), frame( 0 ) {};
	void Tick();
	Puff puff[8];
	bool active;
	int frame, xpos, ypos;
};

class Tank
{
public:
	enum { ACTIVE = 1, P1 = 2, P2 = 4 };
	Tank() : pos( vec2( 0, 0 ) ), speed( vec2( 0, 0 ) ), target( vec2( 0, 0 ) ), reloading( 0 ), prev(nullptr), next(nullptr) {};
	~Tank();
	void Fire( unsigned int party, vec2& pos, vec2& dir );
	void Tick();
	void UpdateGrid();
	vec2 pos, speed, target;
	float maxspeed;
	int flags, reloading;
	Smoke smoke;
	Tank* prev, *next;
	std::pair<int, int> gridCell;
};

class Bullet
{
public:
	enum { ACTIVE = 1, P1 = 2, P2 = 4 };
	Bullet() : flags( 0 ) {};
	void Tick();
	vec2 pos, speed;
	int flags;
};

class Grid
{
public:
	Tank* cells[GRID_WIDTH][GRID_HEIGHT];

	Grid()
	{
		for (int j = 0; j < GRID_HEIGHT; j++)
			for (int i = 0; i < GRID_WIDTH; i++)
				cells[i][j] = nullptr;
	}

	std::pair<int, int> GetIndices(vec2 pos) const
	{
		return std::pair<int, int>((int)(pos.x / GRID_CELL_SIZE), (int)(pos.y / GRID_CELL_SIZE));
	}

	Tank* ActiveTankWithinRange(vec2 pos, float r) // assumes r < GRID_CELL_SIZE
	{
		std::pair<int, int> indices = GetIndices(pos);
		Tank* tank = cells[indices.first][indices.second];

		while (tank != nullptr)
		{
			if (tank->flags & Tank::ACTIVE &&
				pos.x > tank->pos.x - r &&
				pos.y > tank->pos.y - r &&
				pos.x < tank->pos.x + r &&
				pos.y < tank->pos.y + r)
				return tank;

			tank = tank->next;
		}

		// TODO: boundary cases
		if (fmod(pos.x, GRID_CELL_SIZE) < r)
			;
		else if (fmod(pos.x, GRID_CELL_SIZE) > GRID_CELL_SIZE - r)
			;
		if (fmod(pos.y, GRID_CELL_SIZE) < r)
			;
		else if (fmod(pos.y, GRID_CELL_SIZE) > GRID_CELL_SIZE - r)
			;

		return nullptr;
	}
};

class Surface;
class Surface8;
class Sprite;
class Game
{
public:
	void SetTarget( Surface* a_Surface ) { m_Surface = a_Surface; }
	void MouseMove( int x, int y ) { m_MouseX = x; m_MouseY = y; }
	void MouseButton( bool b ) { m_LButton = b; }
	void Init();
	void UpdateTanks();
	void UpdateBullets();
	void DrawTanks();
	void PlayerInput();
	void Tick( float a_DT );
	Surface* m_Surface, *m_Backdrop, *m_Heights, *m_Grid;
	Sprite* m_P1Sprite, *m_P2Sprite, *m_PXSprite, *m_Smoke;
	int m_ActiveP1, m_ActiveP2;
	int m_MouseX, m_MouseY, m_DStartX, m_DStartY, m_DFrames;
	bool m_LButton, m_PrevButton;
	Tank** m_Tank;
	std::clock_t last;
	Grid gridP1, gridP2;
};

}; // namespace Templ8

#endif