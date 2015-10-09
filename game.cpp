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

	unsigned int start = 0, end = MAXP1;
	if (flags & P1)
	{
		start = MAXP1;
		end = MAXP1 + MAXP2;
	}

	//for ( unsigned int i = start; i < end; i++ ) // check all opponents
	//{
	//	Tank* t = game->m_Tank[i];
	//	if (!((t->flags & Tank::ACTIVE) &&
	//		(pos.x > (t->pos.x - 2)) &&
	//		(pos.y > (t->pos.y - 2)) &&
	//		(pos.x < (t->pos.x + 2)) &&
	//		(pos.y < (t->pos.y + 2))))
	//		continue;

	//	if (t->flags & Tank::P1)
	//		aliveP1--;
	//	else
	//		aliveP2--; // update counters

	//	t->flags &= Tank::P1 | Tank::P2; // kill tank
	//	flags = 0;						 // destroy bullet
	//	break;
	//}

	Grid& grid = flags & P1 ? game->gridP2 : game->gridP1;
	Tank* tank = grid.BulletCollision(pos);

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
		bullet[i].speed = speed;
		break;
	}
}

// Tank::Tick - update single tank
void Tank::Tick()
{
	if (!(flags & ACTIVE)) // dead tank
	{
		smoke.xpos = (int)pos.x;
		smoke.ypos = (int)pos.y;
		return smoke.Tick();
	}

	vec2 force = normalize( target - pos );
	// evade mountain peaks
	force += game->EvadeMountainPeaks(this);

	// evade P1 tanks
	force += game->gridP1.TankForces(this);

	// evade P2 tanks
	force += game->gridP2.TankForces(this);

	// evade user dragged line
	if ((flags & P1) && (game->m_LButton))
	{
		float x1 = (float)game->m_DStartX, y1 = (float)game->m_DStartY;
		float x2 = (float)game->m_MouseX, y2 = (float)game->m_MouseY;

		vec2 N = normalize( vec2( y2 - y1, x1 - x2 ) );
		float dist = dot( N, pos ) - dot( N, vec2( x1, y1 ) );

		if (fabs( dist ) < 10)
			if (dist > 0)
				force += 20.0f * N;
			else
				force -= 20.0f * N;
	}

	// update speed using accumulated force
	speed += force;
	speed = normalize(speed);
	pos += speed * maxspeed * 0.5f;

	// shoot, if reloading completed
	if (--reloading >= 0)
		return;

	Tank* target;
	if (flags & P1)
		target = game->gridP2.FindTarget(pos, speed);
	else
		target = game->gridP1.FindTarget(pos, speed);

	if (target == nullptr)
		return;

	Fire(flags & (P1 | P2), pos, speed); // shoot
	reloading = 200; // and wait before next shot is ready
}

void Tank::UpdateGridSmall(std::pair<int, int> ind, Grid& grid, Tank* cell)
{
	// remove from old cell
	if (prev[0] != nullptr)
		prev[0]->next[0] = next[0];
	if (next[0] != nullptr)
		next[0]->prev[0] = prev[0];

	// update grid if necessary
	if (cell == this)
		grid.smallCells[gridCell[0].first][gridCell[0].second] = next[0];

	prev[0] = next[0] = nullptr;

	// add to new cell
	gridCell[0] = ind;
	cell = grid.smallCells[gridCell[0].first][gridCell[0].second]; // new cell

	// empty cell
	if (cell == nullptr)
	{
		grid.smallCells[gridCell[0].first][gridCell[0].second] = this;
	}
	else
	{
		// insert at front

		next[0] = cell;
		cell->prev[0] = this;
		grid.smallCells[gridCell[0].first][gridCell[0].second] = this;
	}	
}

void Tank::UpdateGridLarge(std::pair<int, int> ind, Grid& grid, Tank* cell)
{
	// remove from old cell
	if (prev[1] != nullptr)
		prev[1]->next[1] = next[1];
	if (next[1] != nullptr)
		next[1]->prev[1] = prev[1];

	// update grid if necessary
	if (cell == this)
		grid.largeCells[gridCell[1].first][gridCell[1].second] = next[1];

	prev[1] = next[1] = nullptr;

	// add to new cell
	gridCell[1] = ind;
	cell = grid.largeCells[gridCell[1].first][gridCell[1].second]; // new cell

	// empty cell
	if (cell == nullptr)
	{
		grid.largeCells[gridCell[1].first][gridCell[1].second] = this;
	}
	else
	{
		// insert at front

		next[1] = cell;
		cell->prev[1] = this;
		grid.largeCells[gridCell[1].first][gridCell[1].second] = this;
	}
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

	std::pair<int, int> initgridcell = { -1, -1 };
	// create blue tanks
	for ( unsigned int i = 0; i < MAXP1; i++ )
	{
		Tank* t = m_Tank[i] = new Tank();
		t->pos = vec2( (float)((i % 5) * 20), (float)((i / 5) * 20 + 50) );
		t->target = vec2( SCRWIDTH, SCRHEIGHT ); // initially move to bottom right corner
		t->speed = vec2( 0, 0 ), t->flags = Tank::ACTIVE | Tank::P1, t->maxspeed = (i < (MAXP1 / 2)) ? 0.65f : 0.45f;
		t->gridCell[0] = initgridcell, t->gridCell[1] = initgridcell;
		//t->UpdateGrid();
	}

	// create red tanks
	for ( unsigned int i = 0; i < MAXP2; i++ )
	{
		Tank* t = m_Tank[i + MAXP1] = new Tank();
		t->pos = vec2( (float)((i % 12) * 20 + 900), (float)((i / 12) * 20 + 600) );
		t->target = vec2( 424, 336 ); // move to player base
		t->speed = vec2( 0, 0 ), t->flags = Tank::ACTIVE | Tank::P2, t->maxspeed = 0.3f;
		t->gridCell[0] = initgridcell, t->gridCell[1] = initgridcell;
		//t->UpdateGrid();
	}
	UpdateGrid();
	m_LButton = m_PrevButton = false;

	last = std::clock();

	// set tables
	for (int j = 0; j < 720; j++)
	{
		sintable[j] = sinf((float)j * PI / 360.0f);
		costable[j] = cosf((float)j * PI / 360.0f);
	}
}

// Game::DrawTanks - draw the tanks
void Game::DrawTanks()
{
	// p1
	int asd = SCRWIDTH / GRID_CELL_SIZE2;
	for (int yi = 0; yi <= SCRHEIGHT / GRID_CELL_SIZE2; ++yi)
	{
		for (int xi = 0; xi <= SCRWIDTH / GRID_CELL_SIZE2; ++xi)
		{
			Tank* t = gridP1.largeCells[xi][yi];
			while (t != nullptr)
			{
				float x = t->pos.x, y = t->pos.y;
				vec2 p1(x + 70 * t->speed.x + 22 * t->speed.y, y + 70 * t->speed.y - 22 * t->speed.x);
				vec2 p2(x + 70 * t->speed.x - 22 * t->speed.y, y + 70 * t->speed.y + 22 * t->speed.x);

				if (!(t->flags & Tank::ACTIVE))
					m_PXSprite->Draw((int)x - 4, (int)y - 4, m_Surface); // draw dead tank
				else // draw blue tank
				{
					m_P1Sprite->Draw((int)x - 4, (int)y - 4, m_Surface);
					m_Surface->Line(x, y, x + 8 * t->speed.x, y + 8 * t->speed.y, 0x4444ff);
				}			

				if ((x >= 0) && (x < SCRWIDTH) && (y >= 0) && (y < SCRHEIGHT))
					m_Backdrop->GetBuffer()[(int)x + (int)y * SCRWIDTH] = SubBlend(m_Backdrop->GetBuffer()[(int)x + (int)y * SCRWIDTH], 0x030303); // tracks

				t = t->next[1];
			}
		}
	}

	//p2
	for (int yi = 0; yi <= SCRHEIGHT / GRID_CELL_SIZE2; ++yi)
	{
		for (int xi = 0; xi <= SCRWIDTH / GRID_CELL_SIZE2; ++xi)
		{
			Tank* t = gridP2.largeCells[xi][yi];
			while (t != nullptr)
			{
				float x = t->pos.x, y = t->pos.y;
				vec2 p1(x + 70 * t->speed.x + 22 * t->speed.y, y + 70 * t->speed.y - 22 * t->speed.x);
				vec2 p2(x + 70 * t->speed.x - 22 * t->speed.y, y + 70 * t->speed.y + 22 * t->speed.x);

				if (!(t->flags & Tank::ACTIVE))
					m_PXSprite->Draw((int)x - 4, (int)y - 4, m_Surface); // draw dead tank
					else // draw red tank
				{
					m_P2Sprite->Draw((int)x - 4, (int)y - 4, m_Surface);
					m_Surface->Line(x, y, x + 8 * t->speed.x, y + 8 * t->speed.y, 0xff4444);
				}

				if ((x >= 0) && (x < SCRWIDTH) && (y >= 0) && (y < SCRHEIGHT))
					m_Backdrop->GetBuffer()[(int)x + (int)y * SCRWIDTH] = SubBlend(m_Backdrop->GetBuffer()[(int)x + (int)y * SCRWIDTH], 0x030303); // tracks

				t = t->next[1];
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
			for ( unsigned int i = 0; i < MAXP1; i++ )
				m_Tank[i]->target = vec2( (float)m_MouseX, (float)m_MouseY );

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

void Game::UpdateGrid()
{
//	counter++;
	for (int i = 0; i < MAXP1; ++i)
	{
		Tank* tank = m_Tank[i];

		// Small grid
		Tank* oldcell = gridP1.smallCells[MAX(tank->gridCell[0].first, 0)][MAX(tank->gridCell[0].second, 0)]; // oldcell small grid
		std::pair<int,int> newIndices = gridP1.GetIndices(tank->pos);
		if (tank->gridCell[0] == newIndices) // check if cell has changed in small grid
			continue; // tank can stay in the cell

		tank->UpdateGridSmall(newIndices,gridP1, oldcell); //remove from old cell + add to new cell from the small grid

		//Large grid
		oldcell = gridP1.largeCells[MAX(tank->gridCell[1].first, 0)][MAX(tank->gridCell[1].second, 0)]; //oldcell large grid
		newIndices.first /= 8, newIndices.second/= 8;
		if (tank->gridCell[1] == newIndices) // check if cell has changed in large grid
			continue; // tank can stay in the cell

		tank->UpdateGridLarge(newIndices,gridP1, oldcell); //remove from old cell + add to new cell from the large grid
	}

	for (int i = MAXP1; i < MAXP1+MAXP2; ++i)
	{
		Tank* tank = m_Tank[i];

		// Small grid
		Tank* oldcell = gridP2.smallCells[MAX(tank->gridCell[0].first, 0)][MAX(tank->gridCell[0].second, 0)]; // oldcell small grid
		std::pair<int, int> newIndices = gridP2.GetIndices(tank->pos);
		if (tank->gridCell[0] == newIndices) // check if cell has changed in small grid
			continue; // tank can stay in the cell

		tank->UpdateGridSmall(newIndices, gridP2, oldcell); //remove from old cell + add to new cell from the small grid

		//Large grid
		oldcell = gridP2.largeCells[MAX(tank->gridCell[1].first, 0)][MAX(tank->gridCell[1].second, 0)]; //oldcell large grid
		newIndices.first /= 8, newIndices.second /= 8;
		if (tank->gridCell[1] == newIndices) // check if cell has changed in large grid
			continue; // tank can stay in the cell

		tank->UpdateGridLarge(newIndices, gridP2, oldcell); //remove from old cell + add to new cell from the large grid
	}

}

vec2 Game::EvadeMountainPeaks(Tank*t)
{
	vec2 force;
	for (unsigned int i = 0; i < 16; i++)
	{
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
	return force;
}