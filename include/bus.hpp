#pragma once

#include <array>

#include "util.hpp"
#include "cpu.hpp"
#include "lcd.hpp"
#include "rom.hpp"

typedef union
{
	BYTE b;
	struct
	{
		BYTE select : 2;
		BYTE enable : 1;
		BYTE padding : 5;
	} w;
} TimerControl;

typedef union
{
	BYTE b;
	struct
	{
		BYTE rightA : 1;
		BYTE leftB : 1;
		BYTE upSelect : 1;
		BYTE downStart : 1;
		BYTE selectDirKeys : 1;
		BYTE selectButtonKeys : 1;
		BYTE unused : 2;
	} w;
} JoypadReg;

struct Joypad
{
	bool a, b, up, down, left, right, start, select;
};

class Bus
{
public:
	Bus();
	~Bus();

	void AttachCPU(CPU& cpu);
	void AttachLCD(LCD& lcd);
	void InsertROM(ROM& rom);

	bool Tick();
	bool Execute();
	bool Frame();

	BYTE Read(WORD addr);
	void Write(WORD addr, BYTE val);
	BYTE Fetch(WORD addr);

private:
	BYTE& GetReference(WORD addr);

public:
	ROM* rom;
	CPU* cpu;
	LCD* lcd;

	BYTE invalid;
	BYTE div;
	BYTE tima;
	BYTE tma;
	BYTE dmg_rom;
	JoypadReg joypadReg;
	TimerControl tac;
	size_t internalCounter = 0;

	Joypad joypad;

	// std::array<BYTE, 0x2000> vram;
	std::array<BYTE, 0x2000> wram;
	std::array<BYTE, 0x80> hram;
};