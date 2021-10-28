#pragma once

#include "../util.hpp"

// The memory bank controller (MBC) needs to map addresses targeted at rom, to get the appropriate data from the ROM
class IMBC
{
public:
	IMBC(WORD romBanks, WORD ramBanks, WORD ramSize) :
		romBanks(romBanks), ramBanks(ramBanks), ramSize(ramSize)
	{ }

	virtual ~IMBC() {}

	virtual bool GetMappedRead(WORD address, DWORD& mappedAddr) = 0;				// Convert CPU address to ROM internal address
	virtual bool GetMappedWrite(WORD address, BYTE val, DWORD& mappedAddr) = 0;

protected:
	WORD romBanks, ramBanks, ramSize;
};
