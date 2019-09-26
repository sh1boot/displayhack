typedef unsigned long uint32;
typedef unsigned short uint16;
typedef unsigned char uint8;

struct mul_t
{
	unsigned char mul[3][4];
}			scale1, scale2;
typedef struct
{
	unsigned int		off;
	struct mul_t	       *sptr;
} fdata;

static unsigned int carryfwd;

static uint8 funmul(struct mul_t *tab, uint8 *line1, uint8 *line2, uint8 *line3)
{
	unsigned int sum = carryfwd;
	int i;

	if (tab->mul[1][0] != 0)	/* the table is only 4 bytes wide because of an MMX advantage */
		for (i = 0; i < 3; i++)
		{
			sum += tab->mul[0][i] * *line1++;
			sum += tab->mul[1][i] * *line2++;
			sum += tab->mul[2][i] * *line3++;
		}
	else
	{
		line1++, line2++, line3++;
		for (i = 0; i < 3; i++)
		{
			sum += tab->mul[0][i + 1] * *line1++;
			sum += tab->mul[1][i + 1] * *line2++;
			sum += tab->mul[2][i + 1] * *line3++;
		}
	}
	carryfwd = sum & 255;
	return sum >> 8;
}

void FlameRaw(uint8 *dest, uint8 *source, fdata *map, int pitch, int count)
{
	carryfwd = 128;
	while (count--)
	{
		uint8 *sptr;
		sptr = source + map->off;
		*dest++ = funmul(map->sptr, sptr, sptr + pitch, sptr + pitch + pitch);
		map++;
	}
}
