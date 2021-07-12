#pragma once

#include "util.hpp"

class IMBC
{
public:
	IMBC(WORD romBanks, WORD ramBanks, WORD ramSize) :
		romBanks(romBanks), ramBanks(ramBanks), ramSize(ramSize)
	{ }

	virtual ~IMBC() {}

	virtual bool GetMappedRead(WORD address, DWORD& mappedAddr) = 0;
	virtual bool GetMappedWrite(WORD address, BYTE val, DWORD& mappedAddr) = 0;

protected:
	WORD romBanks, ramBanks, ramSize;
};