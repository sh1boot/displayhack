#include <stdlib.h>
#include <string.h>
#include "seeds.h"

#include "fixwin32.h"

#define VELOCITY			500
#define SPARKS				5
#define RADIUS				600
#define ALTER_CHANCE			100

#define one_chance_in(x)		(rand() > (RAND_MAX - RAND_MAX / (x)))

struct xy_t dirtab[8] =
{
	{ VELOCITY, 0 },
	{ (int)(VELOCITY * 0.7071), (int)(VELOCITY * 0.7071) },
	{ 0, VELOCITY },
	{ (int)(-VELOCITY * 0.7071), (int)(VELOCITY * 0.7071) },
	{ -VELOCITY, 0 },
	{ (int)(-VELOCITY * 0.7071), (int)(-VELOCITY * 0.7071) },
	{ 0, -VELOCITY },
	{ (int)(VELOCITY * 0.7071), (int)(-VELOCITY * 0.7071) }
};

struct walker_t
{
	unsigned		run1 : 4,
				theta1 : 3,
				: 1,
				run2 : 4,
				theta2 : 3,
				set : 1;

	unsigned char		dir;
	signed char		phase;
	signed char		step1,
				step2;
};

static struct point_t
{
	struct xy_t		point;
	struct walker_t		state;
}			Point[SPARKS];

static void movepoint_walker(struct xy_t *pptr, struct walker_t *sptr)
{
	if (sptr->set == 0 || --(sptr->phase) < 0)
	{
		if (sptr->set == 0 || one_chance_in(ALTER_CHANCE))
		{
			sptr->run1 = (rand() >> 8) & 15;
			sptr->theta1 = (rand() >> 8) & 7;
			sptr->run2 = (rand() >> 8) & 15;
			sptr->theta2 = (rand() >> 8) & 7;
			sptr->set = 1;
		}

		if (++(sptr->step1) >= (signed)sptr->run1)
		{
			sptr->dir = (sptr->dir + sptr->theta1) & 7;
			sptr->step1 = -3;
		}
		if (++(sptr->step2) >= (signed)sptr->run2)
		{
			sptr->dir = (sptr->dir + sptr->theta2) & 7;
			sptr->step2 = -3;
		}

		sptr->phase = 3;
	}

	pptr->x += dirtab[sptr->dir].x;
	pptr->y += dirtab[sptr->dir].y;
	{
#if 0
		int x = pptr->x >> 2, y = pptr->y >> 2, radius = (46341 + 3) >> 2;
		if (x * x + y * y > radius * radius)
		{
			int r = i_sqrt(x * x + y * y);
			int xlimit, ylimit;

			xlimit = abs((int)(x * (long long)radius / r));
			ylimit = abs((int)(y * (long long)radius / r));

			x = xlimit ? (x + xlimit) % (xlimit * 2) - xlimit : 0;
			y = ylimit ? (y + ylimit) % (ylimit * 2) - ylimit : 0;

			pptr->x = x << 2;
			pptr->y = y << 2;
		}
#else
		int x = pptr->x >> 2, y = pptr->y >> 2, radius = 49152 >> 2;
		if (x * x + y * y > radius * radius)
		{
			pptr->x = 0;
			pptr->y = 0;
		}
#endif
	}
}


static void movepoints()
{
	struct point_t	       *ptr;

	for (ptr = Point; ptr < Point + SPARKS; ptr++)
		movepoint_walker(&ptr->point, &ptr->state);
}

static void drawpoints(pixel *Target)
{
	int			i;

	for (i = 0; i < SPARKS; i++)
	{
		int			newx = Point[i].point.x,
					newy = Point[i].point.y;
		int			j;


		for (j = 0; j < 7; j++)
		{
			int temp;

			DrawBall((int)newx, (int)newy, RADIUS, Target);

			temp = (20 * newx + 25 * newy) / 32;
			newy = (20 * newy - 25 * newx) / 32;
			newx = temp;
		}
	}
}

void seed_Walker(pixel *Target)
{
	static int initialised = 0;

	if (Target == NULL || initialised != 1)
	{
		memset(Point, 0, sizeof(Point));
		initialised = 1;
	}
	if (Target != NULL)
	{
		movepoints();
		drawpoints(Target);
	}
}

