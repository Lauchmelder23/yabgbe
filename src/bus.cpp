#include "bus.hpp"

#include <stdlib.h>
#include <stdio.h>

static WORD timerModuloLookup[4] = { 1024, 16, 64, 256 };

Bus::Bus()
{
	// These are some default initializatsrions? We dont *necessarily* need them but eh, who cares
	invalid = 0;
	dmg_rom = 0;
	tac.b = 0;
	joypadReg.b = 0xFF;

	joypad.a = true;
	joypad.b = true;
	joypad.down = true;
	joypad.up = true;
	joypad.right = true;
	joypad.left = true;
	joypad.start = true;
	joypad.select = true;
}

Bus::~Bus()
{
	
}

void Bus::AttachCPU(CPU& c)
{
	cpu = &c;
	c.bus = this;
}

void Bus::AttachLCD(LCD& l)
{
	lcd = &l;
	l.bus = this;

	lcd->Setup();
}

void Bus::InsertROM(ROM& r)
{
	rom = &r;
	r.bus = this;
}

bool Bus::Tick()
{
	// Increase internal counter (used for the divider/timer register)
	internalCounter++;

	// Tick the CPU forward one cycle if it isn't stopped
	if(!cpu->stopped)
		cpu->Tick();

	// LCD and CPU operate on the same clock (I think they do at least,
	// the gbdev wiki is incredibly inconsistent about the use of the terms
	// "cycles", "dots" and "clocks" so I just took a guess
	lcd->Tick();

	// The divider registers increases everytime the internal counter counts to 255
	if (!(internalCounter % 0xFF))
		div++;

	// If the timer is enabled and the internal counter hit some number we do some stuff
	if (tac.w.enable && !(internalCounter % timerModuloLookup[tac.w.select]))
	{
		// Like increase the timer register
		tima++;
		if (tima == 0x00)
		{
			// if the timer overflows set it to tma and issue an interrupt
			tima = tma;
			cpu->interruptFlag.flags.timer = 1;
		}
	}

	return true;
}

bool Bus::Execute()
{
	// just Tick for one CPU instruction
	while (cpu->cycles > 0)
	{
		Tick();
		if (invalid)
			return false;
	}

	Tick();
	return true;
}

bool Bus::Frame()
{
	// Just tick for one frame
	while (lcd->cycles > 0)
	{
		Tick();
		if (invalid)
			return false;
	}

	Tick();
	return true;
}

BYTE Bus::Read(WORD addr)
{
	// Read from bus
	BYTE returnVal;
	if (lcd->Read(addr, returnVal))		// If the address is in the LCD realm, then the PPU will handle it
	{
		return returnVal;
	}

	if ((addr >= 0x0000 && addr < 0x8000) || (addr >= 0xA000 && addr < 0xC000))		// If it is in ROM space, the ROM will handle it
	{
		return rom->Read(addr);
	}

	if (addr == 0xFF00)				// All other I/O regs are handled the same, except the joypad one because it's weird
	{
		if (!joypadReg.w.selectButtonKeys) {			// Serious go to the gbdev wiki and read about how this register works
			joypadReg.w.rightA = joypad.a;
			joypadReg.w.leftB = joypad.b;
			joypadReg.w.upSelect = joypad.select;
			joypadReg.w.downStart = joypad.start;
		}
		else if (!joypadReg.w.selectDirKeys)			// This is the best I could come up with because this is stupid
		{
			joypadReg.w.rightA = joypad.right;
			joypadReg.w.leftB = joypad.left;
			joypadReg.w.upSelect = joypad.up;
			joypadReg.w.downStart = joypad.down;
		}

		return joypadReg.b;								// That wasn't a joke, go read about register 0xFF00 in the gameboy
	}

	return GetReference(addr);			// If none of the devices above care about the address, then the bus handles it
}

BYTE Bus::Fetch(WORD addr)
{
	return Read(addr);		// told ya it's literally just Read() lol
}

void Bus::Write(WORD addr, BYTE val)
{
	if (lcd->Write(addr, val))		// If the address is in the LCD realm, then the PPU will handle it
		return;

	if ((addr >= 0x0000 && addr < 0x8000) || (addr >= 0xA000 && addr < 0xC000))		// If it is in ROM space, the ROM will handle it
	{
		rom->Write(addr, val);
		return;
	}

	GetReference(addr) = val;		// otherwise the bus will handle it
	undefined = 0xFF;
}

BYTE& Bus::GetReference(WORD addr)
{
	if (addr >= 0xC000 && addr < 0xFE00)	// Accessing WRAM / ECHO RAM
	{
		return wram[addr & 0x1FFF];
	}
	else if (addr >= 0xFEA0 && addr < 0xFF00)	// Accessing unusable area???
	{
		return undefined;
	}
	else if (addr >= 0xFF00 && addr < 0xFF80)	// Accessing I/O regs
	{
		switch (addr)
		{
		case 0xFF00:	return joypadReg.b;
		case 0xFF04:	return div;
		case 0xFF05:	return tima;
		case 0xFF06:	return tma;
		case 0xFF07:	return tac.b;
		case 0xFF0F:	return cpu->interruptFlag.b;
		case 0xFF50:	return dmg_rom;
		}
	}
	else if (addr >= 0xFF80 && addr < 0xFFFF)	// Accessing HRAM
	{
		return hram[addr & 0x7F];
	}
	else if (addr == 0xFFFF)
	{
		return cpu->interruptEnable.b;
	}

	return undefined;

}