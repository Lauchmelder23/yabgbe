#pragma once

#include <array>
#include <string>
#include "util.hpp"

class Bus;

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
	bool justHaltedWithDI;

private:
	void WriteToRegister(BYTE reg, BYTE val);
	BYTE ReadFromRegister(BYTE reg);
	void ALU(BYTE operation, BYTE operand);
	void CBPrefixed();
};
