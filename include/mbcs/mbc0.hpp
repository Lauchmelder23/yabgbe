#pragma once

#include "Imbc.hpp"

class MBC0 : public IMBC
{
public:
	MBC0(WORD ramBanks) : IMBC(2, ramBanks, 8) {}

	virtual bool GetMappedRead(WORD address, DWORD& mappedAddress) override;
	virtual bool GetMappedWrite(WORD address, BYTE val, DWORD& mappedAddress) override;
private:
};

inline bool MBC0::GetMappedRead(WORD address, DWORD& mappedAddress)
{
	mappedAddress = address;
	return (address < 0x8000);
}

inline bool MBC0::GetMappedWrite(WORD address, BYTE val, DWORD& mappedAddress)
{
	if (ramBanks && 0xA000 <= address && address < 0xC000)
	{
		mappedAddress = address;
		return true;
	}
	return false;
}
