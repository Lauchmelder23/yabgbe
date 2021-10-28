#pragma once

#include <array>
#include <string>
#include "util.hpp"

class Bus;

// Structure to represent a register (register = 16 bits, but split into 2 "sub registers" of 8 bits).
// I also store the names of the regs for debug purposes
struct Register
{
	union
	{
		WORD w;
		struct
		{
			BYTE lo, hi;
		} b;
	};

	char name[3];
};

// Convenience structure for the Interrupts
typedef union {
	BYTE b;
	struct
	{
		BYTE vblank : 1;
		BYTE lcd_stat : 1;
		BYTE timer : 1;
		BYTE serial : 1;
		BYTE joypad : 1;
		BYTE padding : 3;
	} flags;
} Interrupt;

typedef union
{
	BYTE b;
	struct
	{
		BYTE unused : 4;
		BYTE carry : 1;
		BYTE halfCarry : 1;
		BYTE negative : 1;
		BYTE zero : 1;
	} f;
} StatusFlag;

// Read about opcode decoding, the link is in the cpu.cpp file
typedef union
{
	BYTE b;

	struct
	{
		BYTE z : 3;
		BYTE y : 3;
		BYTE x : 2;
	} xyz;

	struct
	{
		BYTE padding1 : 3;
		BYTE q : 1;
		BYTE p : 2;
		BYTE padding2 : 2;
	} pq;
} Opcode;


// Contains everything related to the CPU
class CPU
{
public:
	void Powerup();
	void Tick();

	friend class Bus;

public:
	Interrupt interruptEnable;
	Interrupt interruptFlag;

	size_t totalCycles;
	BYTE cycles;

	Register AF;	// Acc & Flags
	Register BC;
	Register DE;
	Register HL;
	Register SP;		// Stack pointer
	Register PC;		// Program counter
	StatusFlag* flag;

	Opcode opcode;

	std::array<Register*, 4> rp;
	std::array<Register*, 4> rp2;

	BYTE ime;

	Bus* bus;

	bool stopped;
	bool halted;
	bool justHaltedWithDI;		// I don't even know

private:
	void WriteToRegister(BYTE reg, BYTE val);		// The cycles, the god DAMN CPU CYCLES
	BYTE ReadFromRegister(BYTE reg);

	void ALU(BYTE operation, BYTE operand);			// Handle any ALU related instructions
	void CBPrefixed();								// Handle all CB prefixed instructions
};
