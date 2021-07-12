#include "lcd.hpp"

#include <assert.h>

#include "bus.hpp"

static BYTE colormap[4] = { 0b10010011, 0b01001010, 0b00100101, 0b00000000 };
static WORD lastX = 0xFFFF;

// Reverses a Byte (0111010 -> 0101110)
BYTE Reverse(BYTE b) {
	b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
	b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
	b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
	return b;
}

// initializes a bunch of variables
void LCD::Setup()
{
	lcdc.b = 0;
	stat.b = 0;
	scy	= 0;
	scx = 0;
	ly	= 0;
	lyc	= 0;
	wy	= 0;
	wx	= 0;

	cycles = 0;
	scanlineCycles = 0;

	fetcher.cycle = 0;
	fetcher.x = 0;	fetcher.y = -1;
	x = 0;
	dmaCycles = 0;

	bgFIFO.full = 0x00;
	spriteFIFO.full = 0x00;
	windowMode = false;
}

// One LCD tick. Or clock? cycles? who even knows, the wiki uses all of 
// those terms interchangeably while still insisting they're all different
void LCD::Tick()
{
	if (lcdc.w.window)
	{
		volatile int jdkds = 3;
	}

	// Update cycles
	scanlineCycles++;
	cycles++;

	// if we're 455 dots into this scanline we gotta wrap back around
	// and go to the next scanline
	if (scanlineCycles > 455)
	{
		fetcher.cycle = 0;
		scanlineCycles = 0;
		ly += 1;

		// if we reached the bottom then we gotta wrap
		// back up
		if (ly > 153)
		{
			cycles = 0;
			ly = 0;
		}
	}

	// Set modes
	stat.w.coincidence = (lyc == ly);

	// Send interrupts
	if (ly == 144 && scanlineCycles == 0)
	{
		bus->cpu->interruptFlag.flags.vblank = 1;
	}

	if (
		(stat.w.lyc		&& stat.w.coincidence) ||
		(stat.w.mode2	&& stat.w.mode == 2) ||
		(stat.w.mode1	&& stat.w.mode == 1) ||
		(stat.w.mode0	&& stat.w.mode == 0)
		)
	{
		bus->cpu->interruptFlag.flags.lcd_stat = 1;
	}
		
	// Screen
	if (ly >= 0 && ly < 144)
	{
		// If we just started this scanline then start the OAM search phase
		if (scanlineCycles == 0)
		{
			windowMode = false;
			stat.w.mode = 2;
		}

		// Else if we entered screen space, go to the rendering phase
		else if (scanlineCycles == 81) {
			stat.w.mode = 3;
			bgFIFO.full = 0x00;

			x = 0;
			fetcher.x = 0;
			fetcher.cycle = 0;

			fetcher.y = (fetcher.y + 1) % 8;
		}


		// OAM Search
		if (stat.w.mode == 2)
		{
			// Go through all entries in the OAM table and fetch all sprites that are on the current scanline
			// if there are more than 10 sprites on the scanline, discard the rest by setting y = 0
			OAMEntry* entry;
			int counter = 0;
			for (int i = 0; i < 40; i++)
			{
				entry = (OAMEntry*)(oam.data() + i * 4);

				if (entry->b.y == ly)
				{
					counter++;
					if (counter > 10)
						entry->b.y = 0;
				}
			}
		}
		// Pixel Fetcher (oh lord)
		else if (stat.w.mode == 3)
		{
			if (lcdc.w.obj_enable)		// IF sprite rendering is enabled
			{
				if (x != lastX)		// If we already checked this pixel then skip
				{
					lastX = x;
					OAMEntry* entry;
					for (int i = 0; i < 40; i++)		// Go through all sprites
					{
						entry = (OAMEntry*)(oam.data() + i * 4);

						if (x + 8 == entry->b.x && entry->b.y <= ly + 16 && ly + 16 < entry->b.y + 8 + (8 * lcdc.w.obj_size))		// and if the sprite is rendered on the current coordinate
						{
							if (entry->b.idx == 0x82)
								volatile int jdsfklsd = 4;

							// Fetch Sprite!
							WORD yOffset = (ly - entry->b.y + 16) * 2;		// offset of the tile data in vram
							if (entry->b.attr.yFlip)
							{
								yOffset = 16 * (1 + lcdc.w.obj_size) - 2 - yOffset;		// flip vertically by just doing this
							}

							BYTE lo = vram[yOffset + (entry->b.idx * 16 * (1 + lcdc.w.obj_size))];		// get lo and hi byte of tile data
							BYTE hi = vram[yOffset + (entry->b.idx * 16 * (1 + lcdc.w.obj_size)) + 1];

							if (entry->b.attr.xFlip)
							{
								lo = Reverse(lo);
								hi = Reverse(hi);
							}

							// Feed it all into the spriteFIFO
							spriteFIFO.lowByte = lo;
							spriteFIFO.highByte = hi;
							spriteFIFO.full = 0xFF;

							BYTE counter = 0;
							while (spriteFIFO.full)		// While theres data in the fifo
							{
								BYTE color = ((spriteFIFO.highByte & 0x80) >> 6) | ((spriteFIFO.lowByte & 0x80) >> 7);		// Get color of sprite at that pixel

								if (color != 0x00)		// if its not transparent
								{
									BYTE bgPriority = (bgFIFO.sprite & (0x80 >> counter)) >> 6;		// See if there is already a sprite rendered at this pixel (if yes, 
																									// then dont render over it because sprites with the LOWER x coord get priority)

									if (!bgPriority)
									{
										BYTE bgColor = ((bgFIFO.highByte & (0x80 >> counter)) >> 6) | ((bgFIFO.lowByte & (0x80 >> counter)) >> 7);		// Get the color of the background at this pixel
										

										
										if (entry->b.attr.bgPriority)			// If the background/window are supposed to have priority do this
										{
											if (bgColor == 0x00)
											{
												bgFIFO.highByte ^= ((-((spriteFIFO.highByte & 0x80) >> 7) ^ bgFIFO.highByte) & (0x80 >> counter));
												bgFIFO.lowByte ^= ((-((spriteFIFO.lowByte & 0x80) >> 7) ^ bgFIFO.lowByte) & (0x80 >> counter));
												bgFIFO.spritePalette ^= ((-entry->b.attr.palette ^ bgFIFO.lowByte) & (0x80 >> counter));
												bgFIFO.sprite |= (0x80 >> counter);
											}
										}
										else
										{
											bgFIFO.highByte ^= ((-((spriteFIFO.highByte & 0x80) >> 7) ^ bgFIFO.highByte) & (0x80 >> counter));
											bgFIFO.lowByte ^= ((-((spriteFIFO.lowByte & 0x80) >> 7) ^ bgFIFO.lowByte) & (0x80 >> counter));
											bgFIFO.spritePalette ^= ((-entry->b.attr.palette ^ bgFIFO.lowByte) & (0x80 >> counter));
											bgFIFO.sprite |= (0x80 >> counter);
										}
									}
								}

								// Advance FIFOs
								spriteFIFO.full <<= 1;
								spriteFIFO.highByte <<= 1;
								spriteFIFO.lowByte <<= 1;
								counter++;
							}
						}
					}
				}
			}

			// Okay we're back at rendering the background now
			switch (fetcher.cycle)
			{
			case 0:		// Get Tile
			{
				// TODO: Implement the window
				WORD baseAddr = 0x00;
				if(windowMode)
					baseAddr = (lcdc.w.window_tilemap ? 0x1C00 : 0x1800);
				else
					baseAddr = (lcdc.w.bg_tilemap ? 0x1C00 : 0x1800);

				BYTE fetcherX = ((scx + fetcher.x) & 0xFF) / 8;
				BYTE fetcherY = ((ly + scy) & 0xFF) / 8;
				fetcher.tile = vram[baseAddr + (0x20 * fetcherY) + fetcherX];
			}
			break;

			case 2:		// Get Tile Data Low
			{
				WORD baseAddr = (lcdc.w.tiledata ? 0x0000 : 0x0800);
				fetcher.lo = vram[baseAddr + 2 * ((fetcher.y + scy) % 8) + (fetcher.tile * 16)];
				fetcher.lo *= lcdc.w.enable;
			}
				break;
				
			case 4:		// Get Tile Data High
			{
				// TODO: Check LCDC.4
				WORD baseAddr = (lcdc.w.tiledata ? 0x0000 : 0x0800);
				fetcher.hi = vram[baseAddr + 2 * ((fetcher.y + scy) % 8) + (fetcher.tile * 16) + 1];
				fetcher.hi *= lcdc.w.enable;
			}
				break;

			case 8:		// Push
				if (!(bgFIFO.full & 0x00FF))
				{
					bgFIFO.lowByte |= fetcher.lo;
					bgFIFO.highByte |= fetcher.hi;
					bgFIFO.sprite |= 0x00;
					bgFIFO.full |= 0xFF;
					fetcher.cycle = -1;
				}
				else
				{
					fetcher.cycle--;
				}
				
				fetcher.x--;

				break;
			}

			fetcher.cycle++;
			fetcher.x++;

			// Draw pixels		
			if (lcdc.w.window && x + 7 == wx && ly >= wy)
			{
				bgFIFO.full = 0x00;
				fetcher.cycle = 0;
				fetcher.x = x;
				windowMode = true;
			}

			if (bgFIFO.full & 0x00FF)	// If Data in FIFO
			{

				// calculate color 
				BYTE color = ((bgFIFO.highByte & 0x80) >> 6) | ((bgFIFO.lowByte & 0x80) >> 7);
				Palette* p = &bgp;
				if (bgFIFO.sprite & 0x80)
					p = (bgFIFO.spritePalette & 0x80) ? &obp1 : &obp0;

				// set color (pretty easy huh)
				BYTE displayColor = 0x00;
				switch (color)
				{
				case 0x00:	displayColor = colormap[p->colors.idx0]; break;
				case 0x01:	displayColor = colormap[p->colors.idx1]; break;
				case 0x02:	displayColor = colormap[p->colors.idx2]; break;
				case 0x03:	displayColor = colormap[p->colors.idx3]; break;
				}

				if ((bgFIFO.sprite & 0x80) && color == 0x00)
					displayColor = 0x00;

				display[ly * 160 + x] = displayColor;
				x++;

				// advance fifo
				bgFIFO.highByte <<= 1;
				bgFIFO.lowByte <<= 1;
				bgFIFO.full <<= 1;
				bgFIFO.sprite <<= 1;
				bgFIFO.spritePalette <<= 1;
			}

			if (x == 160)	// if we reached the end of the scanline, enable hblank
			{
				stat.w.mode = 0;
			}
		}

	}
	else if (ly == 144)		// if we're at the end of the screen, enable the vblanking period
	{
		stat.w.mode = 1;
		fetcher.y = -1;
	}
}

bool LCD::Read(WORD addr, BYTE& val)
{
	if (0x8000 <= addr && addr < 0xA000)		// VRAM
	{
		if (stat.w.mode != 3 || !lcdc.w.enable)
			val = vram[addr & 0x1FFF];
		else
			val = undefined;

		return true;
	}
	else if (0xFE00 <= addr && addr < 0xFEA0)	// OAM
	{
		if (stat.w.mode == 0 || stat.w.mode == 1 || !lcdc.w.enable)
			val = oam[addr & 0x9F];
		else
			val = undefined;

		return true;
	}
	else if (0xFF00 <= addr && addr < 0xFF80)	// I/O
	{
		switch (addr)
		{
		case 0xFF40:	val = lcdc.b;	return true;
		case 0xFF41:	val = stat.b;	return true;
		case 0xFF42:	val = scy;		return true;
		case 0xFF43:	val = scx;		return true;
		case 0xFF44:	val = ly;		return true;
		case 0xFF45:	val = lyc;		return true;
		case 0xFF47:	val = bgp.b;	return true;
		case 0xFF48:	val = obp0.b;	return true;
		case 0xFF49:	val = obp1.b;	return true;
		case 0xFF4A:	val = wy;		return true;
		case 0xFF4B:	val = wx;		return true;

		case 0xFF46:	val = dma;		return true;
		}
	}

	return false;
}

bool LCD::Write(WORD addr, BYTE val)
{
	if (0x8000 <= addr && addr < 0xA000)		// VRAM
	{
		if (stat.w.mode != 3 || !lcdc.w.enable)
			vram[addr & 0x1FFF] = val;

		return true;
	}
	else if (0xFE00 <= addr && addr < 0xFEA0)	// OAM
	{
		if (stat.w.mode == 0 || stat.w.mode == 1 || !lcdc.w.enable)
			oam[addr & 0x9F] = val;

		return true;
	}
	else if (0xFF00 <= addr && addr < 0xFF80)	// I/O
	{
		switch (addr)
		{
		case 0xFF40:	lcdc.b = val;	return true;
		case 0xFF41:	stat.b = val;	return true;
		case 0xFF42:	scy = val;		return true;
		case 0xFF43:	scx = val;		return true;
		case 0xFF44:	ly = val;		return true;
		case 0xFF45:	lyc = val;		return true;
		case 0xFF47:	bgp.b = val;	return true;
		case 0xFF48:	obp0.b = val;	return true;
		case 0xFF49:	obp1.b = val;	return true;
		case 0xFF4A:	wy = val;		return true;
		case 0xFF4B:	wx = val;		return true;

		case 0xFF46:	dma = val;		dmaCycles = 160;
		}

		while (dmaCycles != 0)
		{
			dmaCycles--;
			oam[dmaCycles] = bus->Read(((WORD)dma) << 8 | dmaCycles);
		}
	}

	return false;
}