#include <stdlib.h>
#include "seeds.h"


#define ENDPOINTS			6

struct swing_t
{
	int			xacc,
				yacc,
				xvel,
				yvel;
};

static struct point_t
{
	struct xy_t		point;
	struct swing_t		state;
}			Point[ENDPOINTS];

static int		initialised = 0;

static void movepoint_swing(struct xy_t *pptr, struct swing_t *sptr)
{
	pptr->x += sptr->xvel;
	pptr->y += sptr->yvel;
	sptr->xvel += sptr->xacc;
	sptr->yvel += sptr->yacc;

	if (rand() > (RAND_MAX - RAND_MAX / 15))
		sptr->xacc = -sptr->xacc;
	if (rand() > (RAND_MAX - RAND_MAX / 15))
		sptr->yacc = -sptr->yacc;

	if (pptr->x < -0x8000 + 10000 || sptr->xvel < -1000)	sptr->xacc = 87;
	if (pptr->x >= 0x8000 - 10000 || sptr->xvel > 1000)	sptr->xacc = -87;
	if (pptr->y < -0x8000 + 10000 || sptr->yvel < -1000)	sptr->yacc = 87;
	if (pptr->y >= 0x8000 - 10000 || sptr->yvel > 1000)	sptr->yacc = -87;
}


static void movepoints(void)
{
	struct point_t	       *ptr = NULL;

	for (ptr = Point; ptr < Point + (sizeof(Point) / sizeof(*Point)); ptr++)
	{
		movepoint_swing(&ptr->point, &ptr->state);
	}

	for (ptr = Point; ptr < Point + (sizeof(Point) / sizeof(*Point)) - 1; ptr += 2)
	{
		ptr[0].point.x += (ptr[1].point.x - ptr[0].point.x) / 16;
		ptr[0].point.y += (ptr[1].point.y - ptr[0].point.y) / 16;
	}
}

static void drawpoints(pixel *Target)
{
	struct point_t	       *ptr;

	for (ptr = Point; ptr < Point + (sizeof(Point) / sizeof(*Point)); ptr++)
	{
		DrawBall( ptr[0].point.x, ptr[0].point.y, 800, Target);
	}
}

void seed_Balls(pixel *Target)
{
	if (Target == NULL || initialised != 1)
	{
		int i;
		for (i = 0; i < ENDPOINTS; i++)
		{
			Point[i].point.x = 0;
			Point[i].point.y = 0;
			Point[i].state.xacc = rand() < RAND_MAX / 2 ? -87 : 87;
			Point[i].state.yacc = rand() < RAND_MAX / 2 ? -87 : 87;
		}
		initialised = 1;
	}
	if (Target != NULL)
	{
		movepoints();
		drawpoints(Target);
	}
}

