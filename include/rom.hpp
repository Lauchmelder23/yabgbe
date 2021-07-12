#pragma once

#include <vector>
#include <memory>
#include "util.hpp"

#include "mbcs/Imbc.hpp"

class Bus;

// Cartridge
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