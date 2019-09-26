CC			= clang
LD			= clang
STRIP			= strip
ASM			= nasm
ARCH			= c
#ARCH			= i386

CFLAGS			= -Wno-nullability-extension -Wno-nullability-completeness -I/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include
OPT_CFLAGS		= -O3 -fomit-frame-pointer -ffast-math
SYN_CFLAGS		= -Wall -pedantic

ASM_FLAGS		= -DPrefixUnderscore=1 -f elf

LIBS			= -lm
INCLUDES		=

SDL_CFLAGS		= `sdl-config --cflags`
SDL_LIBS		= `sdl-config --libs`

TARGET			= displayhack
BASE_OBJ		= displayhack.o warpers.o palettes.o display.o rmpd.o i_maths.o
#TVSEED_OBJ		= seed_tv.o grabber_null.o addsat_$(ARCH).o outline_$(ARCH).o
SEED_OBJ		= seeds.o seed_qix.o seed_walker.o seed_balls.o
BURN_OBJ		= burners.o flame_$(ARCH).o testburn.o

ALL_OBJECTS		= $(BASE_OBJ) $(SEED_OBJ) $(TVSEED_OBJ) $(BURN_OBJ)
ALL_CFLAGS		= $(CFLAGS) $(SYN_CFLAGS) $(OPT_CFLAGS) $(SDL_CFLAGS) $(INCLUDES)
ALL_LIBS		= $(LIBS) $(SDL_LIBS)
ALL_LDFLAGS		= $(LDFLAGS) $(ALL_LIBS)

all: $(TARGET)

.SUFFIXES: .asm

clean:
	rm -f *.o *.core

distclean: clean
	rm -f $(TARGET)

$(TARGET): $(ALL_OBJECTS)
	$(LD) -o $@ $(ALL_OBJECTS) $(ALL_LDFLAGS)
	$(STRIP) $@

.asm.o:
	$(ASM) $(ASM_FLAGS) -o $@ $<

.c.o:
	$(CC) $(ALL_CFLAGS) -c $<

.c.s:
	$(CC) $(ALL_CFLAGS) -S $<

