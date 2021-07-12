#include "bus.hpp"

#include <stdlib.h>
#include <stdio.h>

static WORD timerModuloLookup[4] = { 1024, 16, 64, 256 };

Bus::Bus()
{
	// 8KB of VRAM
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
	internalCounter++;

	if(!cpu->stopped)
		cpu->Tick();

	lcd->Tick();

	if (!(internalCounter % 0xFF))
		div++;

	if (tac.w.enable && !(internalCounter % timerModuloLookup[tac.w.select]))
	{
		tima++;
		if (tima == 0x00)
		{
			tima = tma;
			cpu->interruptFlag.flags.timer = 1;
		}
	}

	return true;
}

bool Bus::Execute()
{
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
	BYTE returnVal;
	if (lcd->Read(addr, returnVal))
	{
		return returnVal;
	}

	if ((addr >= 0x0000 && addr < 0x8000) || (addr >= 0xA000 && addr < 0xC000))
	{
		return rom->Read(addr);
	}

	if (addr == 0xFF00)
	{
		if (!joypadReg.w.selectButtonKeys) {
			joypadReg.w.rightA = joypad.a;
			joypadReg.w.leftB = joypad.b;
			joypadReg.w.upSelect = joypad.select;
			joypadReg.w.downStart = joypad.start;
		}
		else if (!joypadReg.w.selectDirKeys)
		{
			joypadReg.w.rightA = joypad.right;
			joypadReg.w.leftB = joypad.left;
			joypadReg.w.upSelect = joypad.up;
			joypadReg.w.downStart = joypad.down;
		}

		return joypadReg.b;
	}

	return GetReference(addr);
}

BYTE Bus::Fetch(WORD addr)
{
	return Read(addr);
}

void Bus::Write(WORD addr, BYTE val)
{
	if (lcd->Write(addr, val))
		return;

	if ((addr >= 0x0000 && addr < 0x8000) || (addr >= 0xA000 && addr < 0xC000))
	{
		rom->Write(addr, val);
		return;
	}

	GetReference(addr) = val;
	undefined = 0xFF;
}

BYTE& Bus::GetReference(WORD addr)
{
	if (addr >= 0xA000 && addr < 0xC000)	// Accessing external RAM
	{
		return undefined;
	}
	else if (addr >= 0xC000 && addr < 0xFE00)	// Accessing WRAM / ECHO RAM
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