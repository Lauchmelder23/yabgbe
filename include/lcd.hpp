#pragma once

#include <array>
#include "util.hpp"

class Bus;

// bunch of registers or smthn
typedef union
{
	BYTE b;
	struct
	{
		BYTE mode : 2;
		BYTE coincidence : 1;
		BYTE mode0 : 1;
		BYTE mode1 : 1;
		BYTE mode2 : 1;
		BYTE lyc : 1;
	} w;
} STAT;

typedef union
{
	BYTE b;
	struct
	{
		BYTE priority : 1;
		BYTE obj_enable : 1;
		BYTE obj_size : 1;
		BYTE bg_tilemap : 1;
		BYTE tiledata : 1;
		BYTE window : 1;
		BYTE window_tilemap : 1;
		BYTE enable : 1;	
	} w;
} LCDC;

typedef union
{
	WORD w;

	struct
	{
		BYTE lo, hi;
	} b;
} LCDRegister;

typedef struct
{
	WORD spritePalette;
	WORD sprite;
	WORD highByte, lowByte;
	WORD full;
} PixelFIFO;

typedef struct
{
	WORD tile;
	BYTE cycle;
	BYTE x, y;
	BYTE lo, hi;
} PixelFetcher;

typedef union
{
	QWORD q;

	struct
	{
		BYTE y;
		BYTE x;
		BYTE idx;

		struct
		{
			BYTE padding : 4;
			BYTE palette : 1;
			BYTE xFlip : 1;
			BYTE yFlip : 1;
			BYTE bgPriority : 1;
		} attr;
	} b;
} OAMEntry;

typedef union
{
	BYTE b;

	struct
	{
		BYTE idx0 : 2;
		BYTE idx1 : 2;
		BYTE idx2 : 2;
		BYTE idx3 : 2;
	} colors;
} Palette;

// The screen. With emphasis on ree
class LCD
{
public:
	void Setup();
	void Tick();

	bool Read(WORD addr, BYTE& val);
	bool Write(WORD addr, BYTE val);

	DWORD cycles;
	WORD scanlineCycles;

	friend class Bus;
	friend class CPU;

public:
	std::array<BYTE, 160 * 144> display;
	std::array<BYTE, 0x2000> vram;
	std::array<BYTE, 0xA0> oam;

public:
	Bus* bus;

	// Registers
	LCDC lcdc;
	STAT stat;
	BYTE scy;
	BYTE scx;
	BYTE ly;
	BYTE lyc;
	BYTE wy;
	BYTE wx;
	Palette bgp;
	Palette obp0;
	Palette obp1;
	BYTE dma;

	PixelFetcher	fetcher;
	PixelFIFO		bgFIFO;
	PixelFIFO		spriteFIFO;

	BYTE x;
	BYTE dmaCycles;
	bool windowMode;
};
