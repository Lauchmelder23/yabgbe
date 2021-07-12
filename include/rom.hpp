#pragma once

#include <vector>
#include <memory>
#include "util.hpp"

#include "mbcs/Imbc.hpp"

class Bus;

struct MemoryBankController
{
	BYTE w;
	struct
	{
		BYTE ROMBankNumber : 5;
		BYTE RAMBankNumber : 2;
		BYTE Mode : 1;
	} b;
};

class ROM
{
public:
	ROM(FILE* f);

	BYTE Read(WORD addr);
	void Write(WORD addr, BYTE val);

	friend class Bus;

private:
	Bus* bus;
	std::unique_ptr<IMBC> mbc;

	std::vector<BYTE> data, ram;
};