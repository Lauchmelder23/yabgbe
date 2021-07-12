#pragma once

#include "Imbc.hpp"

class MBC1 : public IMBC
{
public:
	MBC1(WORD romBanks, WORD ramBanks, WORD ramSize) : IMBC(romBanks, ramBanks, ramSize) {}

	virtual bool GetMappedRead(WORD address, DWORD& mappedAddr) override;
	virtual bool GetMappedWrite(WORD address, BYTE val, DWORD& mappedAddr) override;

private:
	BYTE RamEnable = 0x00;
	BYTE RomBankNumber = 0x01;
	BYTE RamBankNumber = 0x00;
	BYTE ModeSelect = 0x00;
};

inline bool MBC1::GetMappedRead(WORD address, DWORD& mappedAddr)
{
	if (address < 0x4000)
	{
		mappedAddr = address;
		return true;
	}
	else if(0x4000 <= address && address < 0x8000)
	{
		mappedAddr = ((DWORD)((RamBankNumber << (5 * !ModeSelect)) | RomBankNumber) * 0x4000) + (address & 0x3FFF);
		return true;
	}
	else if (0xA000 <= address && address < 0xC000)
	{
		mappedAddr = (DWORD)((RamBankNumber * ModeSelect) * 0x2000) + (address & 0x1FFF);
		return true;
	}

	return false;
}

inline bool MBC1::GetMappedWrite(WORD address, BYTE val, DWORD& mappedAddr)
{
	if (0x0000 <= address && address < 0x2000)
	{
		RamEnable = val;
		return false;
	}
	else if (0x2000 <= address && address < 0x4000)
	{
		RomBankNumber = val;
		if (RomBankNumber == 0x00 || RomBankNumber == 0x20 || RomBankNumber == 0x40 || RomBankNumber == 0x60)
			RomBankNumber += 1;
		return false;
	}
	else if (0x4000 <= address && address < 0x6000)
	{
		RamBankNumber = val;
		return false;
	}
	else if (0x6000 <= address && address < 0x8000)
	{
		ModeSelect = val;
		return false;
	}
	else if (ramBanks == 0 && 0xA000 <= address && address < 0xC000)
		return false;

	mappedAddr = (DWORD)((RamBankNumber * ModeSelect) * 0x2000) + (address & 0x1FFF);
	return true;
}
