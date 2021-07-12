#include "bus.hpp"

#include <assert.h>
#include <string>

#ifndef NDEBUG
	#ifndef NO_LOG
		#define DBG_MSG(fmt, ...) if(!disablePrint) printf(fmt, __VA_ARGS__)
	#else
		#define DBG_MSG(fmt, ...) do {} while(0)
	#endif
#else
	#define DBG_MSG(fmt, ...) do {} while(0)
#endif

// Macros are cool amirite
#define PUSH(x)		bus->Write(--(SP.w), (x))		// Push to stack
#define POP()		bus->Read((SP.w)++)				// Pop from stack
#define REG(z)		(cbRegisterTable[z])			// Not used even once anywhere

#define HALF_CARRY_ADD(x, y) (((((x) & 0xF) + ((y) & 0xF)) & 0x10) == 0x10)		// Copied from SO
#define HALF_CARRY_SUB(x, y) (((((x) & 0xF) - ((y) & 0xF)) & 0x10) == 0x10)

// Sets up register fully automatically, assigning the strings using the variable name. kinda cool eh?
#define SETUP_REGISTER(x) {	\
	x.w = 0x0000;			\
	strcpy(x.name,	#x);	\
}
#define REGNAME(x) x->name					// Not
#define REGNAME_LO(x) x->name[0]			// used
#define REGNAME_HI(x) x->name[1]			// anywhere

#define XZ_ID(op)	(op.xyz.x * 8 + op.xyz.z)		// what?

#ifndef NDEBUG
	#ifndef NO_LOG
static const char* operandNames[8] = { "B", "C", "D", "E", "H", "L", "(HL)", "A" };
static int disablePrint = 1;
	#endif
#endif

static const WORD interruptVectors[5] = { 0x0040, 0x0048, 0x0050, 0x0058, 0x0060 };

void CPU::Powerup()
{
	// Some basic setup (Is this even necessary?)
	ime = 0;
	flag = (StatusFlag*)(&(AF.b.lo));

	PC.w = 0x0000;

	// Setup opcode decoding lookups
	rp = { &BC, &DE, &HL, &SP };
	rp2 = { &BC, &DE, &HL, &AF };

#ifndef NDEBUG
	// Setup register names
	SETUP_REGISTER(AF);
	SETUP_REGISTER(BC);
	SETUP_REGISTER(DE);
	SETUP_REGISTER(HL);
	SETUP_REGISTER(SP);
#endif

	// Enable BIOS
	bus->Write(0xFF50, 0x00);

	// Reset cycles
	cycles = 0;
	totalCycles = 0;

	stopped = false;
	halted = false;
	justHaltedWithDI = false;
}

void CPU::Tick()
{
	// If halted, then we have to pray to the gods an interrupt occurs to free us from this cursed existence
	if (halted)
	{
		if (interruptEnable.b & interruptFlag.b)
			halted = false;
		else
			return;
	}

	// If we still have cycles left, then come back later and try again
	totalCycles++;
	if (cycles != 0)
	{
		cycles--;
		return;
	}

	// Check for interrupts
	if (ime)
	{
		// mask the interrupts and check which ones need to be handled
		BYTE interruptMask = interruptEnable.b & interruptFlag.b;
		int interruptType = 0;
		while (!(interruptMask & 0x1) && interruptMask)
		{
			interruptType++;
			interruptMask >>= 1;
		}

		if (interruptMask)
		{
			// reset interrupt flag
			interruptFlag.b &= ~(0x1 << interruptType);
			ime = 0;

			// jump to interrupt vector
			PUSH(PC.b.hi);
			PUSH(PC.b.lo);
			PC.w = interruptVectors[interruptType];

			// Will take 24 machine cycles
			cycles = 24;
			return;
		}
	}

#ifndef NDEBUG
	#ifndef NO_LOG
	// if (PC.w == 0x0100) disablePrint = 0;
	// disablePrint = 0;
	#endif 
#endif

	// Fetch
	DBG_MSG("[%10zu] $%04x\t", totalCycles, PC.w);
	opcode.b = bus->Fetch(PC.w++);
	cycles = 4;

	if (justHaltedWithDI)
	{
		PC.w--;
		justHaltedWithDI = false;
	}

	/*
		In the following switch statement the given opcode is decoded
		according to this:
		https://gb-archive.github.io/salvage/decoding_gbz80_opcodes/Decoding%20Gamboy%20Z80%20Opcodes.html
		It might look ugly but it's gonna make debugging a lot easier I hope
	*/

	switch (opcode.xyz.x)
	{
		/////////////// X = 0 ///////////////
	case 0:
	{
		switch (opcode.xyz.z)
		{
			/////////////// RELATIVE JUMPS & ASSORTED OPS ///////////////
		case 0:
		{
			bool condition = true;
			char offset = 0x00;

			switch (opcode.xyz.y)
			{
			case 0:		// NOP
				DBG_MSG("NOP\t");
				break;

			case 1:		// LD (nn), SP
			{
				Register address;
				address.b.lo = bus->Fetch(PC.w++);
				address.b.hi = bus->Fetch(PC.w++);

				bus->Write(address.w, SP.b.lo);
				bus->Write(address.w + 1, SP.b.hi);

				cycles += 16;
				DBG_MSG("LD ($%04x), SP", address.w);
				break;
			}

			case 2:		// STOP
				stopped = true;
				DBG_MSG("STOP");
				break;

			case 3:		// JR
				DBG_MSG("JR ");

			relative_jump:
				offset = bus->Fetch(PC.w++);
				DBG_MSG("$%04x", PC.w + offset);

				PC.w += offset * condition;
				cycles += 4 + (4 * condition);
				break;

			case 4:		// JNZ
				condition = !flag->f.zero;
				DBG_MSG("JR NZ, ");
				goto relative_jump;

			case 5:		// JZ
				condition = flag->f.zero;
				DBG_MSG("JR Z, ");
				goto relative_jump;

			case 6:		// JNC
				condition = !flag->f.carry;
				DBG_MSG("JR NC, ");
				goto relative_jump;

			case 7:		// JC
				condition = flag->f.carry;
				DBG_MSG("JR C, ");
				goto relative_jump;
			}
			break;
		}

			/////////////// 16 BIT LOAD IMMEDIATE | ADD ///////////////
		case 1:
		{
			Register* operand = rp[opcode.pq.p];
			switch (opcode.pq.q)
			{
			case 0:		// LD rp[p], nn
				operand->b.lo = bus->Fetch(PC.w++);
				operand->b.hi = bus->Fetch(PC.w++);

				cycles += 8;
				DBG_MSG("LD %s, $%04x", REGNAME(operand), operand->w);
				break;

			case 1:		// ADD HL, rp[p]
				flag->f.halfCarry = ((((HL.w & 0xFFF) + (operand->w & 0xFFF)) & 0x1000) == 0x1000);
				flag->f.carry = (0xFFFF - operand->w < HL.w);
				flag->f.negative = 0;
				HL.w += operand->w;

				cycles += 4;
				DBG_MSG("ADD HL, %s", REGNAME(operand));
				break;
			}

			break;
		}

			/////////////// INDIRECT LOADING ///////////////
		case 2:
		{
			DBG_MSG("LD ");
#ifndef NDEBUG
			if (opcode.pq.q == 1)	DBG_MSG("A, ");
#endif
			WORD targetAddr = 0x0000;	// Dummy initialization
			switch (opcode.pq.p)
			{
			case 0:	targetAddr = BC.w;			DBG_MSG("(BC)");	break;
			case 1:	targetAddr = DE.w;			DBG_MSG("(DE)");	break;
			case 2:	targetAddr = HL.w++;		DBG_MSG("(HL+)");	break;
			case 3:	targetAddr = HL.w--;		DBG_MSG("(HL-)");	break;
			}

			if (opcode.pq.q == 0)
			{
				bus->Write(targetAddr, AF.b.hi);
				DBG_MSG(", A");
			}
			else
			{
				AF.b.hi = bus->Read(targetAddr);
			}

			cycles += 4;
			break;
		}

			/////////////// 16 BIT INC/DEC ///////////////
		case 3:
		{
			if (opcode.pq.q == 0)
			{
				rp[opcode.pq.p]->w++;
				DBG_MSG("INC %s\t", REGNAME(rp[opcode.pq.p]));
			}
			else
			{
				rp[opcode.pq.p]->w--;
				DBG_MSG("DEC %s\t", REGNAME(rp[opcode.pq.p]));
			}

			cycles += 4;
			break;
		}

			/////////////// 8 BIT INCREMENT ///////////////
		case 4:
		{
			BYTE operand = ReadFromRegister(opcode.xyz.y);
			flag->f.halfCarry = HALF_CARRY_ADD(operand, 1);
			operand++;

			flag->f.zero = !operand;
			flag->f.negative = 0;

			WriteToRegister(opcode.xyz.y, operand);
			DBG_MSG("INC %s\t", operandNames[opcode.xyz.y]);
			break;
		}

		/////////////// 8 BIT DECREMENT ///////////////
		case 5:
		{
			BYTE operand = ReadFromRegister(opcode.xyz.y);
			flag->f.halfCarry = HALF_CARRY_SUB(operand, 1);
			operand--;

			flag->f.zero = !operand;
			flag->f.negative = 1;

			WriteToRegister(opcode.xyz.y, operand);
			DBG_MSG("DEC %s\t", operandNames[opcode.xyz.y]);
			break;
		}
			
			/////////////// 8 BIT LOAD IMMEDIATE ///////////////
		case 6:	
		{
			BYTE immVal = bus->Fetch(PC.w++);
			WriteToRegister(opcode.xyz.y, immVal);

			cycles += 4;
			DBG_MSG("LD %s, $%02x", operandNames[opcode.xyz.y], immVal);
			break;
		}

			/////////////// ASSORTED OPS ON ACC ///////////////
		case 7:
		{
			switch (opcode.xyz.y)
			{
			case 0:		// RLCA
				flag->f.carry = (AF.b.hi & 0x80) >> 7;
				flag->f.negative = 0;
				flag->f.halfCarry = 0;

				AF.b.hi <<= 1;
				AF.b.hi ^= (-flag->f.carry ^ AF.b.hi) & 0x1;
				flag->f.zero = 0;

				DBG_MSG("RLCA\t");
				break;

			case 1:		// RRCA
				flag->f.carry = AF.b.hi & 0x01;
				flag->f.negative = 0;
				flag->f.halfCarry = 0;

				AF.b.hi >>= 1;
				AF.b.hi ^= (-flag->f.carry ^ AF.b.hi) & 0x80;
				flag->f.zero = 0;

				DBG_MSG("RRCA\t");
				break;
				break;

			case 2:		// RLA
			{
				BYTE oldCarry = flag->f.carry;
				flag->f.carry = (AF.b.hi & 0x80) >> 7;
				flag->f.negative = 0;
				flag->f.halfCarry = 0;

				AF.b.hi <<= 1;
				AF.b.hi ^= (-oldCarry ^ AF.b.hi) & 0x1;
				flag->f.zero = 0;

				DBG_MSG("RLA\t");
				break;
			}

			case 3:		// RRA
			{
				BYTE oldCarry = flag->f.carry;
				flag->f.carry = AF.b.hi & 0x01;
				flag->f.negative = 0;
				flag->f.halfCarry = 0;

				AF.b.hi >>= 1;
				AF.b.hi ^= (-oldCarry ^ AF.b.hi) & 0x80;
				flag->f.zero = 0;

				DBG_MSG("RRA\t");
				break;
			}

			case 4:		// DAA
			{
				BYTE correction = 0x00;

				if (flag->f.halfCarry || (!flag->f.negative && (AF.b.hi & 0xF) > 0x09))
					correction |= 0x6;

				if (flag->f.carry || (!flag->f.negative && AF.b.hi > 0x99)) {
					correction |= 0x60;
					flag->f.carry = 1;
				}

				AF.b.hi += flag->f.negative ? -correction : correction;

				flag->f.halfCarry = 0;
				flag->f.zero = !AF.b.hi;
				DBG_MSG("DAA");
				break;
			}

			case 5:		// CPL
				AF.b.hi = ~AF.b.hi;

				flag->f.negative = 1;
				flag->f.halfCarry = 1;

				DBG_MSG("CPL\t");
				break;

			case 6:		// SCF
				flag->f.carry = 1;
				flag->f.halfCarry = 0;
				flag->f.negative = 0;

				DBG_MSG("SCF\t");
				break;

			case 7:		// CCF
				flag->f.carry = !flag->f.carry;
				flag->f.halfCarry = 0;
				flag->f.negative = 0;

				DBG_MSG("CCF\t");
				break;
			}

			break;
		}

		default:
			EXIT_MSG("Unknown opcode x-z octal: %u-%u", opcode.xyz.x, opcode.xyz.z);
			bus->invalid = 1;
			break;
		}

		break;
	}

		/////////////// X = 1 ///////////////
	case 1:
	{
		if (opcode.xyz.y == 6 && opcode.xyz.z == 6)	// HALT
		{
			halted = true;
			justHaltedWithDI = true;

			DBG_MSG("HALT");
		}
		else					// LD r[y], r[z]
		{
			WriteToRegister(opcode.xyz.y, ReadFromRegister(opcode.xyz.z));

			DBG_MSG("LD %s, %s\t", operandNames[opcode.xyz.y], operandNames[opcode.xyz.z]);
		}

		break;
	}
	
	/////////////// X = 2 ///////////////
	case 2:
		ALU(opcode.xyz.y, opcode.xyz.z);
		break;

		/////////////// X = 3 ///////////////
	case 3:
	{
		switch (opcode.xyz.z)
		{
			///////////////	CONDITIONAL RETURN ///////////////
		case 0:
		{
			bool condition;

			switch (opcode.xyz.y)
			{
			case 0:		// RET NZ
				condition = !flag->f.zero;
				DBG_MSG("RET NZ\t");
				goto conditional_return;

			case 1:		// RET Z
				condition = flag->f.zero;
				DBG_MSG("RET Z\t");
				goto conditional_return;

			case 2:		// RET NC
				condition = !flag->f.carry;
				DBG_MSG("RET NC\t");
				goto conditional_return;

			case 3:		// RET C
				condition = flag->f.carry;
				DBG_MSG("RET C\t");

			conditional_return:
				if (condition)
				{
					PC.b.lo = POP();
					PC.b.hi = POP();
				}

				cycles += 4 + (12 * condition);
				break;

			case 4:
			{
				BYTE offset = bus->Fetch(PC.w++);
				bus->Write((WORD)0xFF00 + offset, AF.b.hi);

				cycles += 8;
				DBG_MSG("LD ($FF%02x), A", offset);
				break;
			}

			case 5:
			{
				char val = (char)(bus->Read(PC.w++));

				flag->f.halfCarry = HALF_CARRY_ADD(SP.b.lo, val);
				flag->f.carry = (((((SP.w) & 0xFF) + (val & 0xFF)) & 0x100) == 0x100);
				flag->f.negative = 0;
				flag->f.zero = 0;

				SP.w += val;
				cycles += 12;
				DBG_MSG("ADD SP, $%02x", val);
				break;
			}

			case 6:
			{
				BYTE offset = bus->Fetch(PC.w++);
				AF.b.hi = bus->Read((WORD)0xFF00 + offset);

				cycles += 8;
				DBG_MSG("LD A, ($FF%02x)", offset);
				break;
			}

			case 7:
			{
				char val = (char)bus->Fetch(PC.w++);

				flag->f.halfCarry = HALF_CARRY_ADD(SP.b.lo, val);
				flag->f.carry = (((((SP.w) & 0xFF) + (val & 0xFF)) & 0x100) == 0x100);
				flag->f.zero = 0;
				flag->f.negative = 0;

				HL.w = SP.w + val;
				cycles += 8;
				DBG_MSG("LD HL, SP+$%02x", val);
				break;
			}
			}
			break;
		}

			///////////////	POP & VARIOUS OPS ///////////////
		case 1:
		{
			if (opcode.pq.q == 0)
			{
				rp2[opcode.pq.p]->b.lo = POP() & (~((opcode.pq.p == 3) * 0x0F));		// If reg is AF, then F must be & with 0xF0
				rp2[opcode.pq.p]->b.hi = POP();

				cycles += 8;
				DBG_MSG("POP %s\t", rp2[opcode.pq.p]->name);
			}
			else
			{
				switch (opcode.pq.p)
				{
				case 0:		// RET
					PC.b.lo = POP();
					PC.b.hi = POP();
					
					cycles += 12;
					DBG_MSG("RET\t");
					break;

				case 1:		// RETI
					ime = 1;
					PC.b.lo = POP();
					PC.b.hi = POP();

					cycles += 12;
					DBG_MSG("RETI\t");
					break;

				case 2:		// JP (HL)
					PC.w = HL.w;

					DBG_MSG("JP (HL)\t");
					break;

				case 3:		// LD SP, HL
					SP.w = HL.w;

					DBG_MSG("LD SP, HL");
					cycles += 4;
					break;
				}
			}

			break;
		}

			///////////////	CONDITIONAL JUMPS ///////////////
		case 2:
		{
			bool condition;
			Register addr;
			addr.w = 0;

			switch (opcode.xyz.y)
			{
			case 0:		// JP NZ
				condition = !flag->f.zero;
				DBG_MSG("JP NZ, ");
				goto conditional_jump;

			case 1:		// JP Z
				condition = flag->f.zero;
				DBG_MSG("JP Z, ");
				goto conditional_jump;

			case 2:		// JP NC
				condition = !flag->f.carry;
				DBG_MSG("JP NC, ");
				goto conditional_jump;

			case 3:		// JP C
				condition = flag->f.carry;
				DBG_MSG("JP C, ");

			conditional_jump:
				addr.b.lo = bus->Fetch(PC.w++);
				addr.b.hi = bus->Fetch(PC.w++);

				if (condition) PC.w = addr.w;
				cycles += 8 + (4 * condition);
				DBG_MSG("$%04x", addr.w);
				break;

			case 4:
				bus->Write((WORD)0xFF00 + BC.b.lo, AF.b.hi);

				cycles += 4;
				DBG_MSG("LD ($FF%02x), A", BC.b.lo);
				break;

			case 5:
				addr.b.lo = bus->Fetch(PC.w++);
				addr.b.hi = bus->Fetch(PC.w++);
				bus->Write(addr.w, AF.b.hi);

				cycles += 12;
				DBG_MSG("LD ($%04x), A", addr.w);
				break;

			case 6:
				AF.b.hi = bus->Read((WORD)0xFF00 + BC.b.lo);

				cycles += 4;
				DBG_MSG("LD A, ($FF%02x)", BC.b.lo);
				break;

			case 7:
				addr.b.lo = bus->Fetch(PC.w++);
				addr.b.hi = bus->Fetch(PC.w++);
				AF.b.hi = bus->Read(addr.w);

				cycles += 12;
				DBG_MSG("LD A, ($%04x)", addr.w);
				break;
			}

			break;
		}

			/////////////// ASSORTED OPERATIONS ///////////////
		case 3:
		{
			switch (opcode.xyz.y)
			{
			case 0:	// JP nn
			{
				Register addr;
				addr.b.lo = bus->Read(PC.w++);
				addr.b.hi = bus->Read(PC.w++);

				PC.w = addr.w;

				cycles += 12;
				DBG_MSG("JP, $%04x", addr.w);
				break;
			}

				/////////////// CB PREFIXED ///////////////
			case 1:
				CBPrefixed();
				break;

				/////////////// INTERRUPTS ENABLE / DISABLE ///////////////
			case 6:
				ime = 0;
				DBG_MSG("DI\t");
				break;

			case 7:
				ime = 1;
				DBG_MSG("EI\t");
				break;

			default:
				EXIT_MSG("Unknown opcode x-z-y octal: %u-%u-%u", opcode.xyz.x, opcode.xyz.z, opcode.xyz.y);
				bus->invalid = 1;
				break;
			}

			break;
		}

			/////////////// CONDITION CALL ///////////////
		case 4:
		{
			bool condition = true;

			switch (opcode.xyz.y)
			{

			case 0:	
				condition = !flag->f.zero;	
				DBG_MSG("CALL NZ, ");
				goto conditional_call;

			case 1:	
				condition = flag->f.zero;	
				DBG_MSG("CALL Z, ");
				goto conditional_call;

			case 2:	
				condition = !flag->f.carry;	
				DBG_MSG("CALL NC, ");
				goto conditional_call;

			case 3:	
				condition = flag->f.carry;	
				DBG_MSG("CALL C, ");

			conditional_call:
				Register addr;
				addr.b.lo = bus->Fetch(PC.w++);
				addr.b.hi = bus->Fetch(PC.w++);

				if (condition)
				{
					PUSH(PC.b.hi);
					PUSH(PC.b.lo);

					PC.w = addr.w;
				}

				cycles += 8 + (12 * condition);
				DBG_MSG("$%04x", addr.w);
				break;
			}

			break;
		}

			///////////////	PUSH & VARIOUS OPS ///////////////
		case 5:
		{
			if (opcode.pq.q == 0)
			{
				PUSH(rp2[opcode.pq.p]->b.hi);
				PUSH(rp2[opcode.pq.p]->b.lo);

				cycles += 12;
				DBG_MSG("PUSH %s\t", rp2[opcode.pq.p]->name);
			}
			else
			{
				if (opcode.pq.p == 0)
				{
					Register addr;
					addr.b.lo = bus->Fetch(PC.w++);
					addr.b.hi = bus->Fetch(PC.w++);

					PUSH(PC.b.hi);
					PUSH(PC.b.lo);

					PC.w = addr.w;

					cycles += 20;
					DBG_MSG("CALL $%04x", addr.w);
				}
			}

			break;
		}

			/////////////// ALU ///////////////
		case 6:
			ALU(opcode.xyz.y, bus->Fetch(PC.w++));
			break;

			/////////////// RST ///////////////
		case 7:
			PUSH(PC.b.hi);
			PUSH(PC.b.lo);

			PC.b.hi = 0x00;
			PC.b.lo = 8 * opcode.xyz.y;

			cycles += 12;
			DBG_MSG("RST %02xh", PC.b.lo);
			break;

		default:
			EXIT_MSG("Unknown opcode x-z octal: %u-%u", opcode.xyz.x, opcode.xyz.z);
			bus->invalid = 1;
			break;
		}

		break;
	}

	default:
		EXIT_MSG("Unknown opcode octal x: %u", opcode.xyz.x);
		bus->invalid = 1;
		break;
	}

	DBG_MSG("\t\t AF: %04x  BC: %04x  DE: %04x  HL: %04x  SP: %04x  F: %u%u%u%u", AF.w, BC.w, DE.w, HL.w, SP.w, flag->f.zero, flag->f.negative, flag->f.halfCarry, flag->f.carry);
	DBG_MSG("\t (LY: %03u  SC: %03u  FC: %05u)\n", bus->lcd->ly, bus->lcd->scanlineCycles, bus->lcd->cycles);

}


inline void CPU::WriteToRegister(BYTE reg, BYTE val)
{
	switch (reg)
	{
	case 0:	BC.b.hi = val;	break;
	case 1:	BC.b.lo = val;	break;
	case 2: DE.b.hi = val;	break;
	case 3: DE.b.lo = val;	break;
	case 4: HL.b.hi = val;	break;
	case 5: HL.b.lo = val;	break;
	case 6:
		cycles += 4;
		bus->Write(HL.w, val);
		break;
	case 7: AF.b.hi = val;	break;
	}
}

inline BYTE CPU::ReadFromRegister(BYTE reg)
{
	switch (reg)
	{
	case 0:	return BC.b.hi;
	case 1:	return BC.b.lo;
	case 2: return DE.b.hi;
	case 3: return DE.b.lo;
	case 4: return HL.b.hi;
	case 5: return HL.b.lo;
	case 6:
		cycles += 4;
		return bus->Read(HL.w);
	case 7: return AF.b.hi;
	}

	return 0xFF;
}

inline void CPU::ALU(BYTE operation, BYTE operand)
{
	BYTE val = 0;
#ifndef NDEBUG
	char printString[10] = "";
	if (opcode.xyz.x == 2)
		sprintf(printString, "%s", operandNames[operand]);
	else
		sprintf(printString, "$%02x", operand);
#endif

	if (opcode.xyz.x == 2)
	{
		val = ReadFromRegister(operand);
	}
	else
	{
		val = operand;
		cycles += 4;
	}
	

	switch (operation)
	{
	case 0:			// ADD
		flag->f.halfCarry = HALF_CARRY_ADD(AF.b.hi, val);
		flag->f.carry = (0xFF - val < AF.b.hi);
		AF.b.hi += val;

		flag->f.zero = !AF.b.hi;
		flag->f.negative = 0;

		DBG_MSG("ADD A, %s", printString);
		break;

	case 1:		// ADC
	{
		WORD result = (WORD)AF.b.hi + val + flag->f.carry;
		flag->f.halfCarry = ((((AF.b.hi & val) | ((AF.b.hi ^ val) & ~(AF.b.hi + val + flag->f.carry))) & 0x08) == 0x08);
		flag->f.carry = (result & 0x100) == 0x100;
		AF.b.hi = (BYTE)result;

		flag->f.zero = !AF.b.hi;
		flag->f.negative = 0;

		DBG_MSG("ADC A, %s", printString);
		break;
	}

	case 2:		// SUB
		flag->f.halfCarry = HALF_CARRY_SUB(AF.b.hi, val);
		flag->f.carry = (AF.b.hi < val);
		AF.b.hi -= val;

		flag->f.zero = !AF.b.hi;
		flag->f.negative = 1;

		DBG_MSG("SUB A, %s", printString);
		break;

	case 3:		// SBC
	{
		int result = AF.b.hi - val - flag->f.carry;
		
		flag->f.halfCarry = (((AF.b.hi & 0x0F) - (val & 0x0F) - flag->f.carry) < 0);
		flag->f.carry = (result < 0);
		flag->f.negative = 1;

		AF.b.hi = (BYTE)result;
		flag->f.zero = !AF.b.hi;

		DBG_MSG("SBC A, %s", printString);
		break;
	}

	case 4:
		AF.b.hi &= val;

		flag->f.zero = !AF.b.hi;
		flag->f.negative = 0;
		flag->f.halfCarry = 1;
		flag->f.carry = 0;

		DBG_MSG("AND A, %s\t", printString);
		break;

	case 5:		// XOR
		AF.b.hi ^= val;

		flag->f.zero = !AF.b.hi;
		flag->f.negative = 0;
		flag->f.halfCarry = 0;
		flag->f.carry = 0;

		DBG_MSG("XOR A, %s", printString);
		break;

	case 6:		// OR
		AF.b.hi |= val;

		flag->f.zero = !AF.b.hi;
		flag->f.negative = 0;
		flag->f.halfCarry = 0;
		flag->f.carry = 0;

		DBG_MSG("OR A, %s\t", printString);
		break;

	case 7:		// CP
	{
		WORD result = AF.b.hi - val;

		flag->f.zero = !result;
		flag->f.negative = 1;
		flag->f.halfCarry = HALF_CARRY_SUB(AF.b.hi, val);
		flag->f.carry = (AF.b.hi < val);

		DBG_MSG("CP A, %s", printString);
		break;
	}

	default:
		EXIT_MSG("Unknown ALU operation: %u", operation);
		bus->invalid = true;
		break;
	}
}

inline void CPU::CBPrefixed()
{
	opcode.b = bus->Fetch(PC.w++);
	cycles += 8;
	BYTE val = ReadFromRegister(opcode.xyz.z);

	switch (opcode.xyz.x)
	{
	case 0:
	{
		switch (opcode.xyz.y)
		{
		case 0:	// RLC
			flag->f.carry = (val & 0x80) >> 7;
			flag->f.negative = 0;
			flag->f.halfCarry = 0;

			val <<= 1;
			val ^= (-flag->f.carry ^ val) & 0x1;
			flag->f.zero = !val;

			DBG_MSG("RLC %s\t", operandNames[opcode.xyz.z]);
			break;

		case 1:	// RLL
			flag->f.carry = val & 0x01;
			flag->f.negative = 0;
			flag->f.halfCarry = 0;

			val >>= 1;
			val ^= (-flag->f.carry ^ val) & 0x80;
			flag->f.zero = !val;

			DBG_MSG("RLL %s\t", operandNames[opcode.xyz.z]);
			break;

		case 2:	// RL
		{
			BYTE oldCarry = flag->f.carry;
			flag->f.carry = (val & 0x80) >> 7;
			flag->f.negative = 0;
			flag->f.halfCarry = 0;

			val <<= 1;
			val ^= (-oldCarry ^ val) & 0x1;
			flag->f.zero = !val;

			DBG_MSG("RL %s\t", operandNames[opcode.xyz.z]);
			break;
		}

		case 3:	// RR
		{
			BYTE oldCarry = flag->f.carry;
			flag->f.carry = val & 0x01;
			flag->f.negative = 0;
			flag->f.halfCarry = 0;

			val >>= 1;
			val ^= (-oldCarry ^ val) & 0x80;
			flag->f.zero = !val;

			DBG_MSG("RR %s\t", operandNames[opcode.xyz.z]);
			break;
		}

		case 4:	// SLA
		{
			flag->f.carry = (val & 0x80) >> 7;
			flag->f.negative = 0;
			flag->f.halfCarry = 0;

			val <<= 1;
			flag->f.zero = !val;

			DBG_MSG("SLA %s\t", operandNames[opcode.xyz.z]);
			break;
		}

		case 5:	// SRA
		{
			flag->f.carry = val & 0x01;
			flag->f.negative = 0;
			flag->f.halfCarry = 0;

			BYTE rMask = (val & 0x80);
			val = (val >> 1) | rMask;
			flag->f.zero = !val;

			DBG_MSG("SRA %s\t", operandNames[opcode.xyz.z]);
			break;
		}

		case 6:		// SWAP
		{
			BYTE loNibble = val & 0x0F;
			flag->f.carry = 0;
			flag->f.negative = 0;
			flag->f.halfCarry = 0;

			val = (val >> 4) | (loNibble << 4);
			flag->f.zero = !val;

			DBG_MSG("SWAP %s\t", operandNames[opcode.xyz.z]);
			break;
		}

		case 7:	// SRL
		{
			flag->f.carry = val & 0x01;
			flag->f.negative = 0;
			flag->f.halfCarry = 0;

			val >>= 1;
			flag->f.zero = !val;

			DBG_MSG("SRL %s\t", operandNames[opcode.xyz.z]);
			break;
		}

		}

		break;
	}

	case 1:		// BIT
		flag->f.zero = !((val & (0x1 << opcode.xyz.y)) >> opcode.xyz.y);
		flag->f.negative = 0;
		flag->f.halfCarry = 1;

		DBG_MSG("BIT %u, %s", opcode.xyz.y, operandNames[opcode.xyz.z]);
		return;
		break;

	case 2:		// RES
		val &= ~(0x1 << opcode.xyz.y);
		DBG_MSG("RES %x, %s", opcode.xyz.y, operandNames[opcode.xyz.z]);
		break;

	case 3:		// SET
		val |= 0x1 << opcode.xyz.y;
		DBG_MSG("SET %x, %s", opcode.xyz.y, operandNames[opcode.xyz.z]);
		break;


	default:
		EXIT_MSG("Unknown CB prefixed operation with x octal: %u", opcode.xyz.x);
		bus->invalid = true;
		break;
	}

	WriteToRegister(opcode.xyz.z, val);
}