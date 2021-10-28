#pragma once

#include <array>

#include "util.hpp"
#include "cpu.hpp"
#include "lcd.hpp"
#include "rom.hpp"

// Why is this typedef? Lol
// This was originally a C project believe it or not. I thought getting
// this tiny performance increase was worth the trouble, but I guess that
// eventually I ran into a problem that couldn't be easily solved in less 
// than 5 lines of C code. At least I thought so, but anyways, I decided
// to rewrite the emulator in C++, that's why all this code looks so
// fucked up and messy.
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


// JoypadReg = The register in the gameboy
// Joypad = A struct to keep the physical keyboard input
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

// The Bus class contains all the stuff that I didn't know where else to put
class Bus
{
public:
	Bus();
	~Bus();

	// Used to conn
	void AttachCPU(CPU& cpu);
	void AttachLCD(LCD& lcd);
	void InsertROM(ROM& rom);

	bool Tick();		// Execute ONE machine cycle (why would you do that lol)
	bool Execute();		// Execute ONE CPU instruction (better but still why)
	bool Frame();		// Execute CPU instructions until we rendered one full frame (there we go)

	BYTE Read(WORD addr);				// Read from the bus
	void Write(WORD addr, BYTE val);	// Write to the bus
	BYTE Fetch(WORD addr);				// This is literally the same as Read(). Like literally. the. exact. same. 
										// But I use it a lot in the CPU class so I'm too lazy/afraid to remove it

private:
	BYTE& GetReference(WORD addr);		// Leftovers of a really really really bad idea, but again it's used in a few places so I'm too scared to remove it

public:
	// Connected devices
	ROM* rom;
	CPU* cpu;
	LCD* lcd;

	// These are I/O registers :)
	BYTE invalid;
	BYTE div;
	BYTE tima;
	BYTE tma;
	BYTE dmg_rom;
	JoypadReg joypadReg;
	TimerControl tac;
	size_t internalCounter = 0;

	Joypad joypad;

	std::array<BYTE, 0x2000> wram;
	std::array<BYTE, 0x80> hram;		// <-- This should be in the CPU class but who cares
};