#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rmpd.h"

 /* //////////////////////////////////////////////////////////////////////// */

static double RandDisplacement(double Range)
{
	return rand() * Range / RAND_MAX - Range / 2;
}

static void NetSort(int *a, int *b)
{
	if (*a > *b)
	{
		int temp = *b;
		*b = *a;
		*a = temp;
	}
}

static int NewPoint(double Range, int p1, int p2, int p3, int p4)
{
	double			Average;

	Average = (p1 + p2 + p3 + p4) / 4;

#if 0
	NetSort(&p1, &p2);
	NetSort(&p3, &p4);
	NetSort(&p2, &p3);
	NetSort(&p1, &p2);
 	NetSort(&p3, &p4);

	Range = (p3 - p1) * 1.1;
#endif

	Average += RandDisplacement(Range);
	if (Average < 0) Average = 0;
	if (Average > 65535) Average = 65535;

	return Average;
}

static int NewEdge(double Range, int p1, int p2, int p3)
{
	double			Average;

	Average = (p1 + p2 + p3) / 3;

	NetSort(&p1, &p2);
	NetSort(&p2, &p3);
	NetSort(&p1, &p2);

#if 0
	Range = (p3 - p1) * 1.1;
#endif

	Average += RandDisplacement(Range);
	if (Average < 0) Average = 0;
	if (Average > 65535) Average = 65535;

	return Average;
}

static void MidpointSquare(int MemWidth, int BufWidth, int BufHeight, int Step, unsigned short *Buffer)
{
	double			Range = Step * 1.414214 * 256;
	unsigned short	       *UpperLine,
			       *MidLine,
			       *LowerLine;
	int			HalfStep,
				x1,
				y1;


	HalfStep = Step / 2;
	UpperLine = Buffer;
	for (y1 = 0; y1 < BufHeight; y1 += Step)
	{
		MidLine = UpperLine + MemWidth * HalfStep + HalfStep;
		LowerLine = UpperLine + MemWidth * Step;

		for (x1 = 0; x1 < BufWidth; x1 += Step)
			MidLine[x1] = NewPoint(Range, UpperLine[x1], UpperLine[x1 + Step], LowerLine[x1], LowerLine[x1 + Step]);

		UpperLine = LowerLine;
	}
}

static void MidpointDiamond(int MemWidth, int BufWidth, int BufHeight, int Step, unsigned short *Buffer)
{
	double			Range = Step * 256;
	unsigned short	       *UpperLine,
			       *MidLine,
			       *LowerLine;
	int			HalfStep,
				x1,
				y1;


	HalfStep = Step / 2;
	UpperLine = Buffer;
	for (y1 = 0; y1 < BufHeight; y1 += Step)
	{
		MidLine = UpperLine + MemWidth * HalfStep;
		LowerLine = UpperLine + MemWidth * Step;

		MidLine[0] = NewEdge(Range, UpperLine[0], LowerLine[0], MidLine[HalfStep]);

		for (x1 = Step; x1 < BufWidth; x1 += Step)
			MidLine[x1] = NewPoint(Range, UpperLine[x1], LowerLine[x1], MidLine[x1 - HalfStep], MidLine[x1 + HalfStep]);

		MidLine[x1] = NewEdge(Range, UpperLine[x1], LowerLine[x1], MidLine[x1 - HalfStep]);

		UpperLine = LowerLine;
	}

	UpperLine = Buffer - MemWidth * HalfStep + HalfStep;
	MidLine = UpperLine + MemWidth * HalfStep - HalfStep;
	LowerLine = UpperLine + MemWidth * Step;

	for (x1 = 0; x1 < BufWidth; x1 += Step)
		MidLine[x1 + HalfStep] = NewEdge(Range, MidLine[x1], MidLine[x1 + Step], LowerLine[x1]);

	UpperLine = LowerLine;

	for (y1 = Step; y1 < BufHeight; y1 += Step)
	{
		MidLine = UpperLine + MemWidth * HalfStep - HalfStep;
		LowerLine = UpperLine + MemWidth * Step;

		for (x1 = 0; x1 < BufWidth; x1 += Step)
			MidLine[x1 + HalfStep] = NewPoint(Range, UpperLine[x1], LowerLine[x1], MidLine[x1], MidLine[x1 + Step]);

		UpperLine = LowerLine;
	}

	MidLine = UpperLine + MemWidth * HalfStep - HalfStep;
	LowerLine = UpperLine + MemWidth * Step;

	for (x1 = 0; x1 < BufWidth; x1 += Step)
		MidLine[x1 + HalfStep] = NewEdge(Range, UpperLine[x1], MidLine[x1], MidLine[x1 + Step]);
}

void RMPD(unsigned short *Buffer, int Width, int Height, int Pitch)
{
	int			WorkingWidth,
				MemWidth,
				WorkingHeight,
				Step,
				x,
				y;
	unsigned short	       *Workspace,
			       *Source;


	WorkingWidth = ((Width + 127 + 2) & ~127);
	WorkingHeight = ((Height + 127 + 2) & ~127);
	MemWidth = WorkingWidth + 1;

	Workspace = calloc(MemWidth * (WorkingHeight + 1), sizeof(short));
	if (Workspace == NULL)
		return;

	Step = 128;
	for (y = 0; y < (WorkingHeight + 1); y += Step)
		for (x = 0; x < MemWidth; x += Step)
			Workspace[y * MemWidth + x] = rand();

	while (Step > 1)
	{
		MidpointSquare(MemWidth, WorkingWidth, WorkingHeight, Step, Workspace);
		MidpointDiamond(MemWidth, WorkingWidth, WorkingHeight, Step, Workspace);
		Step /= 2;
	}

	Source = Workspace;

	while (Height--)
	{
		memcpy(Buffer, Source, Width * sizeof(Buffer[0]));
		Buffer += Pitch;
		Source += MemWidth;
	}

	free(Workspace);
}

