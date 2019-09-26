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

static void movepoint_swing(struct xy_t *pptr, struct swing_t *sptr)
{
	pptr->x += sptr->xvel;
	pptr->y += sptr->yvel;
	sptr->xvel += sptr->xacc;
	sptr->yvel += sptr->yacc;

	if (rand() > (RAND_MAX - RAND_MAX / 12))
		sptr->xacc = -sptr->xacc;
	if (rand() > (RAND_MAX - RAND_MAX / 12))
		sptr->yacc = -sptr->yacc;

	if (pptr->x < -0x8000 + 2500 || sptr->xvel < -300)	sptr->xacc = 25;
	if (pptr->x >= 0x8000 - 2500 || sptr->xvel > 300)	sptr->xacc = -25;
	if (pptr->y < -0x8000 + 2500 || sptr->yvel < -300)	sptr->yacc = 25;
	if (pptr->y >= 0x8000 - 2500 || sptr->yvel > 300)	sptr->yacc = -25;
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

	for (ptr = Point; ptr < Point + (sizeof(Point) / sizeof(*Point)) - 1; ptr += 2)
	{
		DrawLine( ptr[0].point.x, ptr[0].point.y,
			  ptr[1].point.x, ptr[1].point.y, Target);
		DrawLine(-ptr[0].point.x, ptr[0].point.y,
			 -ptr[1].point.x, ptr[1].point.y, Target);

		DrawLine( ptr[0].point.x,-ptr[0].point.y,
			  ptr[1].point.x,-ptr[1].point.y, Target);
		DrawLine(-ptr[0].point.x,-ptr[0].point.y,
			 -ptr[1].point.x,-ptr[1].point.y, Target);
	}
}

void seed_Qix(pixel *Target)
{
	static int initialised = 0;

	if (Target == NULL || initialised != 1)
	{
		int i;
		for (i = 0; i < ENDPOINTS; i++)
		{
			Point[i].point.x = 0;
			Point[i].point.y = 0;
			Point[i].state.xacc = rand() < RAND_MAX / 2 ? -25 : 25;
			Point[i].state.yacc = rand() < RAND_MAX / 2 ? -25 : 25;
		}
		initialised = 1;
	}
	if (Target != NULL)
	{
		movepoints();
		drawpoints(Target);
	}
}

