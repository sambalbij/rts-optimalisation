#include "precomp.h"

// global data (source scope)
static Game* game;

// mountain peaks (push player away)
static float peakx[16] = { 248, 537, 695, 867, 887, 213, 376, 480, 683, 984, 364, 77,  85, 522, 414, 856 };
static float peaky[16] = { 199, 223, 83, 374,  694, 639, 469, 368, 545, 145,  63, 41, 392, 285, 447, 352 };
static float peakh[16] = { 200, 150, 160, 255, 200, 255, 200, 300, 120, 100,  80, 80,  80, 160, 160, 160 };

// player, bullet and smoke data
static int aliveP1 = MAXP1, aliveP2 = MAXP2;
static Bullet bullet[MAXBULLET];

// smoke particle effect tick function
void Smoke::Tick()
{
	unsigned int p = frame >> 3;
	if (frame < 64)
		if (!(frame++ & 7))
		{
			puff[p].x = xpos;
			puff[p].y = ypos << 8;
			puff[p].vy = -450;
			puff[p].life = 63;
		}

	for ( unsigned int i = 0; i < p; i++ ) if ((frame < 64) || (i & 1))
	{
		puff[i].x++;
		puff[i].y += puff[i].vy;
		puff[i].vy += 3;

		int frame = (puff[i].life > 13) ? (9 - (puff[i].life - 14) / 5) : (puff[i].life / 2);
		game->m_Smoke->SetFrame( frame );
		game->m_Smoke->Draw( puff[i].x - 12, (puff[i].y >> 8) - 12, game->m_Surface );

		if (!--puff[i].life)
		{
			puff[i].x = xpos;
			puff[i].y = ypos << 8;
			puff[i].vy = -450;
			puff[i].life = 63;
		}
	}
}

// bullet Tick function
void Bullet::Tick()
{
	if (!(flags & Bullet::ACTIVE))
		return;

	vec2 prevpos = pos;
	pos += 1.5f * speed;
	prevpos -= pos - prevpos;
	game->m_Surface->AddLine( prevpos.x, prevpos.y, pos.x, pos.y, 0x555555 );

	if ((pos.x < 0) || (pos.x > (SCRWIDTH - 1)) || (pos.y < 0) || (pos.y > (SCRHEIGHT - 1)))
		flags = 0; // off-screen

	Tank* tank = game->grid.BulletCollision(pos, flags & P1 ? P2 : P1);

	if (tank == nullptr)
		return;

	if (tank->flags & Tank::P1)
		aliveP1--;
	else
		aliveP2--;

	tank->flags &= Tank::P1 | Tank::P2;
	flags = 0;
}

// Tank::Fire - spawns a bullet
void Tank::Fire( unsigned int party, vec2& pos, vec2& dir )
{
	for ( unsigned int i = 0; i < MAXBULLET; i++ ) if (!(bullet[i].flags & Bullet::ACTIVE))
	{
		bullet[i].flags |= Bullet::ACTIVE + party; // set owner, set active
		bullet[i].pos = pos;
		bullet[i].speed = vec2(game->tankVelX[index], game->tankVelY[index]);
		break;
	}
}

// Tank::Tick - update single tank
void Tank::Tick()
{
	vec2 pos(game->tankPosX[index], game->tankPosY[index]);
	vec2 target(game->tankTarX[index], game->tankTarY[index]);

	if (!(flags & ACTIVE)) // dead tank
	{
		smoke.xpos = (int)pos.x;
		smoke.ypos = (int)pos.y;
		return smoke.Tick();
	}

	vec2 force = normalize(target - pos);

	// apply external forces
	force += vec2(game->tankForX[index], game->tankForY[index]);
	force += game->grid.TankForces(this);

	game->tankForX[index] = 0;
	game->tankForY[index] = 0;

	// evade user dragged line
	if ((flags & P1) && (game->m_LButton))
	{
		float x1 = (float)game->m_DStartX, y1 = (float)game->m_DStartY;
		float x2 = (float)game->m_MouseX, y2 = (float)game->m_MouseY;

		vec2 N = normalize(vec2(y2 - y1, x1 - x2));
		float dist = dot(N, pos) - dot(N, vec2(x1, y1));

		if (fabs(dist) < 10)
			if (dist > 0)
				force += 20.0f * N;
			else
				force -= 20.0f * N;
	}

	// update speed using accumulated force
	vec2 speed(game->tankVelX[index], game->tankVelY[index]);
	speed += force;
	speed = normalize(speed);
	pos += speed * maxspeed * 0.5f;

	game->tankVelX[index] = speed.x;
	game->tankVelY[index] = speed.y;

	game->tankPosX[index] = pos.x;
	game->tankPosY[index] = pos.y;

	cellPos = game->grid.GetIndices(pos);

	// shoot, if reloading completed
	if (--reloading >= 0)
		return;

	Tank* t;
	if (flags & P1)
		t = game->aimP1.FindTarget(this);
	else
		t = game->aimP2.FindTarget(this);

	if (t == nullptr)
		return;

	Fire(flags & (P1 | P2), pos, speed); // shoot
	reloading = 200; // and wait before next shot is ready
}

// Game::Init - Load data, setup playfield
void Game::Init()
{
	game = this; // for global reference

	m_Heights = new Surface("testdata/heightmap.png");
	m_Backdrop = new Surface(1024, 768);
	m_Grid = new Surface(1024, 768);

	Pixel* a1 = m_Grid->GetBuffer(), *a2 = m_Backdrop->GetBuffer(), *a3 = m_Heights->GetBuffer();
	for ( int y = 0; y < 768; y++ )
		for ( int idx = y * 1024, x = 0; x < 1024; x++, idx++ )
			a1[idx] = (((x & 31) == 0) | ((y & 31) == 0)) ? 0x6600 : 0;

	for ( int y = 0; y < 767; y++ )
		for ( int idx = y * 1024, x = 0; x < 1023; x++, idx++ ) 
		{
			vec3 N = normalize(vec3((float)(a3[idx + 1] & 255) - (a3[idx] & 255),
									1.5f,
									(float)(a3[idx + 1024] & 255) - (a3[idx] & 255)));
			vec3 L( 1, 4, 2.5f );

			float h = (float)(a3[x + y * 1024] & 255) * 0.0005f, dx = x - 512.f, dy = y - 384.f, d = sqrtf( dx * dx + dy * dy ), dt = dot( N, normalize( L ) );
			int u = max( 0, min( 1023, (int)(x - dx * h) ) ), v = max( 0, min( 767, (int)(y - dy * h) ) ), r = (int)Rand( 255 );
			a2[idx] = AddBlend( a1[u + v * 1024], ScaleColor( ScaleColor( 0x33aa11, r ) + ScaleColor( 0xffff00, (255 - r) ), (int)(max( 0.0f, dt ) * 80.0f) + 10 ) );
		}

	m_Tank = new Tank*[MAXP1 + MAXP2];
	m_P1Sprite = new Sprite( new Surface( "testdata/p1tank.tga" ), 1, Sprite::FLARE );
	m_P2Sprite = new Sprite( new Surface( "testdata/p2tank.tga" ), 1, Sprite::FLARE );
	m_PXSprite = new Sprite( new Surface( "testdata/deadtank.tga" ), 1, Sprite::BLACKFLARE );
	m_Smoke = new Sprite( new Surface( "testdata/smoke.tga" ), 10, Sprite::FLARE );

	// create blue tanks
	for (unsigned int i = 0; i < MAXP1; i++)
	{
		Tank* t = m_Tank[i] = new Tank();
		tankPosX[i] = (float)((i % 5) * 20);
		tankPosY[i] = (float)((i / 5) * 20 + 50);
		tankTarX[i] = SCRWIDTH; // initially move to bottom right corner
		tankTarY[i] = SCRHEIGHT;
		tankVelX[i] = 0;
		tankVelY[i] = 0;
		t->flags = Tank::ACTIVE | Tank::P1, t->maxspeed = (i < (MAXP1 / 2)) ? 0.65f : 0.45f;
		t->cellPos = grid.GetIndices(vec2(tankPosX[i], tankPosY[i]));
		t->index = i;
	}

	// create red tanks
	for (unsigned int i = 0; i < MAXP2; i++)
	{
		int k = i + MAXP1;
		Tank* t = m_Tank[k] = new Tank();
		tankPosX[k] = (float)((i % 12) * 20 + 900);
		tankPosY[k] = (float)((i / 12) * 20 + 600);
		tankTarX[k] = 424; // move to player base
		tankTarY[k] = 336;
		tankVelX[k] = 0;
		tankVelY[k] = 0;
		t->flags = Tank::ACTIVE | Tank::P2, t->maxspeed = 0.3f;
		t->cellPos = grid.GetIndices(vec2(tankPosX[k], tankPosY[k]));
		t->index = k;
	}

	UpdateGrid();
	m_LButton = m_PrevButton = false;

	last = std::clock();


	// init the mountain"grid"
	for (unsigned int i = 0; i < 16; ++i)
	{
		for (int x = 0; x < GRID_WIDTH; ++x)
			for (int y = 0; y < GRID_HEIGHT; ++y)
				nearMountainPeak[i][x][y] = false;
		std::pair<int, int> indices = grid.GetIndices(vec2(peakx[i], peaky[i]));
		int minx = MAX(0, indices.first - 6); // 6 * 16 < sqrt(7500) < 87
		int maxx = MIN(GRID_WIDTH, indices.first + 6);
		int miny = MAX(0, indices.second - 6);
		int maxy = MIN(GRID_HEIGHT, indices.second + 6);
		for (int x = minx; x <= maxx;++x)
			for (int y = miny; y <= maxy; ++y)	
				nearMountainPeak[i][x][y] = true;
	}

	// set tables
	for (int j = 0; j < 720; j++)
	{
		sintable[j] = sinf((float)j * PI / 360.0f);
		costable[j] = cosf((float)j * PI / 360.0f);
	}
	EvadeMountainPeaks();
}

// Game::DrawTanks - draw the tanks
void Game::DrawTanks()
{
	// p1
	int asd = SCRWIDTH / GRID_CELL_SIZE_LARGE;
	for (int yi = 0; yi <= SCRHEIGHT / GRID_CELL_SIZE_LARGE; ++yi)
	{
		for (int xi = 0; xi <= SCRWIDTH / GRID_CELL_SIZE_LARGE; ++xi)
		{
			std::vector<int>& cell = aimP2.cells[xi][yi];
			for (int k : cell)
			{
				Tank* t = m_Tank[k];
				vec2 tpos(game->tankPosX[t->index], game->tankPosY[t->index]);
				vec2 tspeed(game->tankVelX[t->index], game->tankVelY[t->index]);

				float x = tpos.x, y = tpos.y;
				vec2 p1(x + 70 * tspeed.x + 22 * tspeed.y, y + 70 * tspeed.y - 22 * tspeed.x);
				vec2 p2(x + 70 * tspeed.x - 22 * tspeed.y, y + 70 * tspeed.y + 22 * tspeed.x);

				if (!(t->flags & Tank::ACTIVE))
					m_PXSprite->Draw((int)x - 4, (int)y - 4, m_Surface); // draw dead tank
				else // draw blue tank
				{
					m_P1Sprite->Draw((int)x - 4, (int)y - 4, m_Surface);
					m_Surface->Line(x, y, x + 8 * tspeed.x, y + 8 * tspeed.y, 0x4444ff);
				}

				if ((x >= 0) && (x < SCRWIDTH) && (y >= 0) && (y < SCRHEIGHT))
					m_Backdrop->GetBuffer()[(int)x + (int)y * SCRWIDTH] = SubBlend(m_Backdrop->GetBuffer()[(int)x + (int)y * SCRWIDTH], 0x030303); // tracks
			}
		}
	}

	//p2
	for (int yi = 0; yi <= SCRHEIGHT / GRID_CELL_SIZE_LARGE; ++yi)
	{
		for (int xi = 0; xi <= SCRWIDTH / GRID_CELL_SIZE_LARGE; ++xi)
		{
			std::vector<int>& cell = aimP1.cells[xi][yi];
			for (int k : cell)
			{
				Tank* t = m_Tank[k];
				vec2 tpos(game->tankPosX[t->index], game->tankPosY[t->index]);
				vec2 tspeed(game->tankVelX[t->index], game->tankVelY[t->index]);

				float x = tpos.x, y = tpos.y;
				vec2 p1(x + 70 * tspeed.x + 22 * tspeed.y, y + 70 * tspeed.y - 22 * tspeed.x);
				vec2 p2(x + 70 * tspeed.x - 22 * tspeed.y, y + 70 * tspeed.y + 22 * tspeed.x);

				if (!(t->flags & Tank::ACTIVE))
					m_PXSprite->Draw((int)x - 4, (int)y - 4, m_Surface); // draw dead tank
				else // draw red tank
				{
					m_P2Sprite->Draw((int)x - 4, (int)y - 4, m_Surface);
					m_Surface->Line(x, y, x + 8 * tspeed.x, y + 8 * tspeed.y, 0xff4444);
				}

				if ((x >= 0) && (x < SCRWIDTH) && (y >= 0) && (y < SCRHEIGHT))
					m_Backdrop->GetBuffer()[(int)x + (int)y * SCRWIDTH] = SubBlend(m_Backdrop->GetBuffer()[(int)x + (int)y * SCRWIDTH], 0x030303); // tracks
			}
		}
	}

}

// Game::PlayerInput - handle player input
void Game::PlayerInput()
{
	if (m_LButton)
	{
		if (!m_PrevButton)
		{
			m_DStartX = m_MouseX;
			m_DStartY = m_MouseY;
			m_DFrames = 0; // start line
		}

		m_Surface->ThickLine( m_DStartX, m_DStartY, m_MouseX, m_MouseY, 0xffffff );
		m_DFrames++;
	}
	else
	{
		if ((m_PrevButton) && (m_DFrames < 15)) // new target location
			for (unsigned int i = 0; i < MAXP1; i++)
			{
				tankTarX[i] = (float)m_MouseX;
				tankTarY[i] = (float)m_MouseY;
			}

		m_Surface->Line( 0, (float)m_MouseY, SCRWIDTH - 1, (float)m_MouseY, 0xffffff );
		m_Surface->Line( (float)m_MouseX, 0, (float)m_MouseX, SCRHEIGHT - 1, 0xffffff );
	}

	m_PrevButton = m_LButton;
}

// Game::Tick - main game loop
void Game::Tick( float a_DT )
{
	POINT p;
	GetCursorPos( &p );
	ScreenToClient( FindWindow( NULL, "Template" ), &p );
	m_LButton = (GetAsyncKeyState( VK_LBUTTON ) != 0), m_MouseX = p.x, m_MouseY = p.y;
	m_Backdrop->CopyTo( m_Surface, 0, 0 );

	for ( unsigned int i = 0; i < (MAXP1 + MAXP2); i++ )
		m_Tank[i]->Tick();

	UpdateGrid();
	EvadeMountainPeaks();

	for ( unsigned int i = 0; i < MAXBULLET; i++ )
		bullet[i].Tick();

	DrawTanks();
	PlayerInput();

	std::clock_t now = std::clock();
	float dt = (now - last) / (float)CLOCKS_PER_SEC;
	last = now;

	char buffer[128];
	sprintf(buffer, "%02i fps", (int)(1.0f / dt));
	m_Surface->Print(buffer, 965, 10, 0xffff00);

	if ((aliveP1 > 0) && (aliveP2 > 0))
	{
		sprintf( buffer, "blue army: %03i  red army: %03i", aliveP1, aliveP2 );
		return m_Surface->Print( buffer, 10, 10, 0xffff00 );
	}

	if (aliveP1 == 0) 
	{
		sprintf( buffer, "sad, you lose... red left: %i", aliveP2 );
		return m_Surface->Print( buffer, 200, 370, 0xffff00 );
	}

	sprintf( buffer, "nice, you win! blue left: %i", aliveP1 );
	m_Surface->Print( buffer, 200, 370, 0xffff00 );
}

void Game::ClearGrid()
{
	for (const std::pair<int, int>& ind : nonEmptyCells) // clear the grids
	{
		int x = ind.first, y = ind.second;
		grid.cells[x][y].clear();
		aimP1.cells[x >> 3][y >> 3].clear(); // we can do this because the large grids are
		aimP2.cells[x >> 3][y >> 3].clear(); // exactly 8 times larger than the small grid
	}

	nonEmptyCells.clear();
}

void Game::UpdateGrid()
{
	ClearGrid();

	for (int i = 0; i < MAXP1 + MAXP2; i++)
	{
		// add to small grid
		Tank* tank = m_Tank[i];
		std::pair<int, int> ind = tank->cellPos;

		int x = ind.first, y = ind.second;
		if (grid.cells[x][y].empty())
			nonEmptyCells.push_back(ind);
		grid.cells[x][y].push_back(i);

		// add to large grid
		LargeGrid& lg = tank->flags & Tank::P1 ? aimP2 : aimP1;
		lg.cells[x >> 3][y >> 3].push_back(i);
	}
}

void Game::EvadeMountainPeaks()
{
	for (unsigned int i = 0; i < 16; ++i)
	{
		std::pair<int, int> indices = grid.GetIndices(vec2(peakx[i], peaky[i]));
		int minx = MAX(0, indices.first - 6); // 6 * 16 < sqrt(7500) < 87
		int maxx = MIN(GRID_WIDTH, indices.first + 6);
		int miny = MAX(0, indices.second - 6);
		int maxy = MIN(GRID_HEIGHT, indices.second + 6);
		for (int x = minx; x <= maxx; ++x) for (int y = miny; y <= maxy; ++y)
		{
			std::vector<int>& cell = grid.cells[x][y];
			for (int k : cell)
			{
				Tank* current = m_Tank[k];
				vec2 tpos(game->tankPosX[current->index], game->tankPosY[current->index]);

				vec2 d(tpos.x - peakx[i], tpos.y - peaky[i]);
				float sd = (d.x * d.x + d.y * d.y);// *0.2f;
				if (sd < 7500)//1500)*5
				{
					d *= 0.15f * (peakh[i] / sd);//force += d * 0.03f * (peakh[i] / sd);
					game->tankForX[current->index] += d.x;
					game->tankForY[current->index] += d.y;

					float r = sqrtf(sd*0.2);
					for (int j = 0; j < 720; j++)
					{
						float x = peakx[i] + r * sintable[j];
						float y = peaky[i] + r * costable[j];
						game->m_Surface->AddPlot((int)x, (int)y, 0x000500);
					}
				}
			} // end while loop

		} // end loop over nearby gridcells
	} // end loop over peaks
	/*vec2 force;
	for (unsigned int i = 0; i < 16; i++)
	{
		std::pair<int, int> ind = t->cellPos;
		if (!nearMountainPeak[i][ind.first][ind.second])
			continue;
		vec2 d(t->pos.x - peakx[i], t->pos.y - peaky[i]);
		float sd = (d.x * d.x + d.y * d.y);// *0.2f;
		if (sd < 7500)//1500)*5
		{
			force += d*0.15f*(peakh[i] / sd);//force += d * 0.03f * (peakh[i] / sd);
			float r = sqrtf(sd*0.2);
			for (int j = 0; j < 720; j++)
			{
				float x = peakx[i] + r * sintable[j];
				float y = peaky[i] + r * costable[j];
				game->m_Surface->AddPlot((int)x, (int)y, 0x000500);
			}
		}
	}
	return force;*/
}

SmallGrid::SmallGrid()
{
	for (int j = 0; j < GRID_HEIGHT; j++)
		for (int i = 0; i < GRID_WIDTH; i++)
			cells[i][j] = std::vector<int>();
}

std::pair<int, int> SmallGrid::GetIndices(vec2 pos) const
{
	return std::pair<int, int>((int)(pos.x / GRID_CELL_SIZE_SMALL), (int)(pos.y / GRID_CELL_SIZE_SMALL));
}

// returns total accumulated tank forces
vec2 SmallGrid::TankForces(Tank* tank)
{
	vec2 result;
	vec2 pos(game->tankPosX[tank->index], game->tankPosY[tank->index]);

	std::pair<int, int> ind = tank->cellPos;
	int x = ind.first, y = ind.second; // our tank's cell
	for (int j = MAX(0, y - 1); j <= MIN(GRID_HEIGHT - 1, y + 1); j++) // look in neighbouring cells as well
		for (int i = MAX(0, x - 1); i <= MIN(GRID_WIDTH - 1, x + 1); i++)
		{
			std::vector<int>& cell = cells[i][j];
			for (int k : cell) // loop over all tanks in cell
			{
				if (k <= tank->index) // we don't want our tank or those already processed
					continue;

				Tank* current = game->m_Tank[k];
				vec2 tpos(game->tankPosX[current->index], game->tankPosY[current->index]);

				vec2 d = pos - tpos; // distance
				float length2 = d.x * d.x + d.y * d.y; // squared length
				if (length2 < 256) // 16 * 16
				{
					float mul = length2 < 64 ? 2.0f : 0.4f;	// force factor based on distance
					d *= (mul / sqrtf(length2));
					result += d; // apply force to tank
					game->tankForX[current->index] -= d.x; // apply symmetric force to other tank
					game->tankForY[current->index] -= d.y;
				}
			}
		}

	return result;
}

Tank* SmallGrid::BulletCollision(vec2 pos, int team)
{
	std::pair<int, int> indices = GetIndices(pos);
	std::vector<int>& cell = cells[indices.first][indices.second];
	for (int k : cell) // check own cell first
	{
		Tank* tank = game->m_Tank[k];
		vec2 tpos(game->tankPosX[tank->index], game->tankPosY[tank->index]);

		if (tank->flags & Tank::ACTIVE &&
			tank->flags & team &&
			pos.x > tpos.x - 2 &&
			pos.y > tpos.y - 2 &&
			pos.x < tpos.x + 2 &&
			pos.y < tpos.y + 2)
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

			std::vector<int>& cell2 = cells[x][y];
			for (int k : cell)
			{
				Tank* tank = game->m_Tank[k];
				vec2 tpos(game->tankPosX[tank->index], game->tankPosY[tank->index]);

				if (tank->flags & Tank::ACTIVE &&
					tank->flags & team &&
					pos.x > tpos.x - 2 &&
					pos.y > tpos.y - 2 &&
					pos.x < tpos.x + 2 &&
					pos.y < tpos.y + 2)
					return tank;
			}
		}
	}

	return nullptr;
}

LargeGrid::LargeGrid()
{
	for (int j = 0; j < GRID_HEIGHT / 8; j++)
		for (int i = 0; i < GRID_WIDTH / 8; i++)
			cells[i][j] = std::vector<int>();
}

std::pair<int, int> LargeGrid::GetIndices(vec2 pos) const
{
	return std::pair<int, int>((int)(pos.x / GRID_CELL_SIZE_LARGE), (int)(pos.y / GRID_CELL_SIZE_LARGE));
}

Tank* LargeGrid::FindTarget(Tank* source)
{
	const float r2 = 100 * 100;
	std::pair<int, int> indices = source->cellPos;
	vec2 dir(game->tankTarX[source->index], game->tankTarY[source->index]);
	vec2 pos(game->tankPosX[source->index], game->tankPosY[source->index]);
	int x = indices.first >> 3;
	int y = indices.second >> 3;

	const int n = 1; // ceil(100 / GRID_CELL_SIZE_LARGE)
	int minX = CLAMP(x, 0, x + (dir.x < 0 ? -n : n)); // smallest x of cell to check
	int maxX = CLAMP(x, x + (dir.x < 0 ? -n : n), (GRID_WIDTH / 8) - 1); // largest x
	int minY = CLAMP(y, 0, y + (dir.y < 0 ? -n : n));
	int maxY = CLAMP(y, y + (dir.y < 0 ? -n : n), (GRID_HEIGHT / 8) - 1);

	for (int y = minY; y <= maxY; y++)
		for (int x = minX; x <= maxX; x++)
		{
			std::vector<int>& cell = cells[x][y];
			for (int k : cell) // all tanks in cell
			{
				Tank* tank = game->m_Tank[k];

				if (tank->flags & Tank::ACTIVE) // only active tanks
				{
					vec2 tpos(game->tankPosX[tank->index], game->tankPosY[tank->index]);

					vec2 diff = tpos - pos;
					float dist2 = dot(diff, diff);
					if (dist2 < r2) // squared distance check
					{
						float dist = sqrtf(dist2);
						if (dist < 100) // distance check
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