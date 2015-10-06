#include <ctime>

#ifndef I_GAME_H
#define I_GAME_H

namespace Tmpl8 {

#define MAXP1			20			// increase to test your optimized code
#define MAXP2			(4 * MAXP1)	// because the player is smarter than the AI
#define TANKS			MAXP1+MAXP2
#define MAXBULLET		200000
#define GRID_WIDTH		5000		// the number of cells
#define GRID_HEIGHT		5000
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
//	void UpdateGrid();
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
	std::vector<Tank*> tanks[GRID_WIDTH][GRID_HEIGHT];

	Grid()
	{
		
	}

	std::pair<int, int> GetIndices(vec2 pos) const
	{
		return std::pair<int, int>((int)(pos.x / GRID_CELL_SIZE), (int)(pos.y / GRID_CELL_SIZE));
	}

	// returns (tank, force) for each tank within range
	std::vector<std::pair<Tank*, vec2>> TanksWithinInfluence(Tank* tank)
	{
		std::vector<std::pair<Tank*, vec2>> result;

		std::pair<int, int> indices = GetIndices(tank->pos);
		
		for (int j = MAX(0, indices.second - 1); j < MIN(GRID_HEIGHT, indices.second + 1); j++)
		{
			for (int i = MAX(0, indices.first - 1); i < MIN(GRID_WIDTH, indices.first + 1); i++)
			{
				for (Tank* current : tanks[i][j])
				{
					if (current == tank) // we don't want our tank
						continue;

					vec2 d = tank->pos - current->pos; // distance
					float length2 = d.x * d.x + d.y * d.y; // squared length
					if (length2 < 256) // 16 * 16
					{
						float mul = length2 < 64 ? 2.0f : 0.4f;				// force factor based on distance
						result.emplace_back(current,						// the tank
							d * (mul / sqrtf(length2)));	// the force vector
					}
				}
			}
		}
		return result;
	}

	Tank* ActiveTankWithinRange(vec2 pos, float r) // assumes r < GRID_CELL_SIZE
	{
		std::pair<int, int> indices = GetIndices(pos);
		for (Tank* tank : tanks[indices.first][indices.second]) // check own cell first
		{
			if (tank->flags & Tank::ACTIVE &&
				pos.x > tank->pos.x - r &&
				pos.y > tank->pos.y - r &&
				pos.x < tank->pos.x + r &&
				pos.y < tank->pos.y + r)
				return tank;
		}

		for (int j = -1; j <= 1; j++) // look in neighbouring cells as well
		{
			int y = indices.second + j;
			if (y < 0 || y >= GRID_HEIGHT)
				continue;

			for (int i = -1; i <= 1; i++)
			{
				int x = indices.first + i;
				if (x < 0 || x >= GRID_WIDTH)
					continue;

				if (i == 0 && j == 0)
					continue;

				for (Tank* tank : tanks[x][y])
				{
					if (tank->flags & Tank::ACTIVE &&
						pos.x > tank->pos.x - r &&
						pos.y > tank->pos.y - r &&
						pos.x < tank->pos.x + r &&
						pos.y < tank->pos.y + r)
						return tank;
				}
			}
		}
		return nullptr;
	}

	Tank* ActiveTankWithinRangeAndDirection(vec2 pos, float r, vec2 dir)
	{
		float r2 = r * r;
		std::pair<int, int> indices = GetIndices(pos);

		const int n = 7; // ceil(100 / GRID_CELL_SIZE)
		int minX = MAX(0, MIN(indices.first, indices.first + (dir.x < 0 ? -n : n))); // smallest x of cell to check
		int maxX = MIN(GRID_WIDTH - 1, MAX(indices.first, indices.first + (dir.x < 0 ? -n : n))); // largest x
		int minY = MAX(0, MIN(indices.second, indices.second + (dir.y < 0 ? -n : n)));
		int maxY = MIN(GRID_HEIGHT - 1, MAX(indices.second, indices.second + (dir.y < 0 ? -n : n)));

		for (int y = minY; y <= maxY; y++)
			for (int x = minX; x <= maxX; x++)
			{
			for (Tank* tank : tanks[x][y])
				{
					if (tank->flags & Tank::ACTIVE) // only active tanks
					{
						vec2 diff = tank->pos - pos;
						float dist2 = dot(diff, diff);
						if (dist2 < r2) // squared distance check
						{
							float dist = sqrtf(dist2);
							if (dist < r) // distance check
							{
								diff /= dist;
								if (dot(diff, dir) > 0.99999f) // direction check
									return tank; // all conditions met!
							}
						}
					}
				}
			}

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
	void RefreshVectorGrid();
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