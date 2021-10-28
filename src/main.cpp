#include "bus.hpp"

#include <iostream>

#include <SDL.h>
#include <glad.h>
#define IMGUI_IMPL_OPENGL_LOADER_GLAD
#include <imgui_impl_opengl3.h>
#include <imgui_sdl.h>

static BYTE colormap[4] = { 0b00000000, 0b00100101, 0b01001010, 0b10010011 };

#undef main

int main(int argc, char** argv)
{
	// Calculate the size of the window? This is literally random lol
	int width = (512 + 384) * 2 + 10;
	int height = 256 * 4;

	SDL_Init(SDL_INIT_VIDEO);

	if(!SDL_WasInit(SDL_INIT_VIDEO))
	{
		std::cerr << "Failed to initialize SDL:\n" << SDL_GetError() << std::endl;
		return -1;
	}

	SDL_Window* window = SDL_CreateWindow("Gameboy Emulator", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_SHOWN);
	{
		std::cerr << "Failed to create window:\n" << SDL_GetError() << std::endl;
		return -1;
	}

	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	if(renderer == nullptr)
	{
		std::cerr << "Failed to create accelerated rendering device:\n" << SDL_GetError() << std::endl;
		return -1;
	}

	if(!gladLoadGL())
	{
		std::cerr << "Failed to load GL" << std::endl;
	}

	SDL_Event e;

	// Initialize ImGui
	// I use ImGui to display lots of useful info, mainly memory and registers
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiSDL::Initialize(renderer, width, height);

	ImGui::StyleColorsDark();

	// To avoid some ImGui weirdness, we need to clear our screen using a texture
	SDL_Texture* clearScreen = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 1, 1);
	{
		SDL_SetRenderTarget(renderer, clearScreen);
		SDL_SetRenderDrawColor(renderer, 100, 0, 100, 255);
		SDL_RenderClear(renderer);
		SDL_SetRenderTarget(renderer, NULL);
	}

	// All the textures to old the info we'll be displaying
	SDL_Texture* ramScreen		= SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB332, SDL_TEXTUREACCESS_STREAMING, 128, 64);
	SDL_Texture* vramScreen		= SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB332, SDL_TEXTUREACCESS_STREAMING, 128, 64);
	SDL_Texture* tilemapRaw1	= SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB332, SDL_TEXTUREACCESS_STREAMING, 32, 32);
	SDL_Texture* tilemapRaw2	= SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB332, SDL_TEXTUREACCESS_STREAMING, 32, 32);
	SDL_Texture* tilemap1		= SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB332, SDL_TEXTUREACCESS_STREAMING, 256, 256);
	SDL_Texture* tilemap2		= SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB332, SDL_TEXTUREACCESS_STREAMING, 256, 256);
	SDL_Texture* hramScreen		= SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB332, SDL_TEXTUREACCESS_STREAMING, 16, 8);
	SDL_Texture* gameboyScreen	= SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB332, SDL_TEXTUREACCESS_STREAMING, 160, 144);

	// aspect ratios for the non-square windows
	float ramAR = 128.f / 64.f;
	float vramAR = 128.f / 64.f;
	float hramAR = 16.f / 8.f;
	float gbAR = 160.f / 144.f;

	// Initialize the gameboy
	Bus bus;
	CPU cpu;
	LCD lcd;

	bus.AttachCPU(cpu);
	bus.AttachLCD(lcd);

	// Load the rom
	if (argc != 2)
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Failed to load ROM", "Usage: gbemu <ROM>\nOr drag and drop a ROM onto the executable.", window);
		exit(-1);
	}

	FILE* f = fopen(argv[1], "rb");
	ROM rom(f);
	fclose(f);

	bus.InsertROM(rom);

	cpu.Powerup();

	// Placeholder vars for pixel arrays used to calcualte the rendered tilemaps
	BYTE* tilemappixels1;
	int tilemappitch1;
	BYTE* tilemappixels2;
	int tilemappitch2;

	// This is literally irrelevant since I'm using a texture to clear anyways? lol
	SDL_SetRenderDrawColor(renderer, 100, 0, 100, 255);

	// The main program loop
	bool done = false;
	while (!done)
	{
		// If the emulator hasn't shit itself yet we can run the Gameboy for one frame
		if (!bus.invalid)
			bus.Frame();

		// Also give ImGui all the inputs that happened
		ImGuiIO& io = ImGui::GetIO();
		int wheel = 0;

		// Poll for events
		while (SDL_PollEvent(&e))
		{
			// Boring ImGui stuff
			if (e.type == SDL_QUIT) done = true;
			else if (e.type == SDL_WINDOWEVENT)
			{
				if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
				{
					io.DisplaySize.x = static_cast<float>(e.window.data1);
					io.DisplaySize.y = static_cast<float>(e.window.data2);
				}
				else if(e.window.event == SDL_WINDOWEVENT_CLOSE)
				{
					done = true;
				}
			}
			else if (e.type == SDL_MOUSEWHEEL)
			{
				wheel = e.wheel.y;
			}

			// Input for the Joypad. Look at how ugly it is
			else if (e.type == SDL_KEYUP)
			{
				switch (e.key.keysym.sym)
				{
				case SDLK_s:		bus.joypad.a = true; break;
				case SDLK_a:		bus.joypad.b = true; break;
				case SDLK_UP:		bus.joypad.up = true; break;
				case SDLK_DOWN:		bus.joypad.down = true; break;
				case SDLK_LEFT:		bus.joypad.left = true; break;
				case SDLK_RIGHT:	bus.joypad.right = true; break;
				case SDLK_LSHIFT:	bus.joypad.select = true; break;
				case SDLK_RETURN:	bus.joypad.start = true; break;
				}
			}

			else if (e.type == SDL_KEYDOWN)
			{
				switch (e.key.keysym.sym)
				{
				case SDLK_s:		bus.joypad.a = false; cpu.interruptFlag.flags.joypad = 1; break;
				case SDLK_a:		bus.joypad.b = false; cpu.interruptFlag.flags.joypad = 1; break;
				case SDLK_UP:		bus.joypad.up = false; cpu.interruptFlag.flags.joypad = 1; break;
				case SDLK_DOWN:		bus.joypad.down = false; cpu.interruptFlag.flags.joypad = 1; break;
				case SDLK_LEFT:		bus.joypad.left = false; cpu.interruptFlag.flags.joypad = 1; break;
				case SDLK_RIGHT:	bus.joypad.right = false; cpu.interruptFlag.flags.joypad = 1; break;
				case SDLK_LSHIFT:	bus.joypad.select = false; cpu.interruptFlag.flags.joypad = 1; break;
				case SDLK_RETURN:	bus.joypad.start = false; cpu.interruptFlag.flags.joypad = 1; break;
				}
				bus.cpu->stopped = false;
			}
		}

		// Send all the things that changed to ImGui
		int mouseX, mouseY;
		const int buttons = SDL_GetMouseState(&mouseX, &mouseY);

		io.DeltaTime = 1.0f / 60.0f;
		io.MousePos = ImVec2(static_cast<float>(mouseX), static_cast<float>(mouseY));
		io.MouseDown[0] = buttons & SDL_BUTTON(SDL_BUTTON_LEFT);
		io.MouseDown[1] = buttons & SDL_BUTTON(SDL_BUTTON_RIGHT);
		io.MouseWheel = static_cast<float>(wheel);

		ImGui::NewFrame();

		// Update the memory maps by literally just copying the raw data of the arrays to the texture
		SDL_UpdateTexture(ramScreen, NULL, bus.wram.data(), 128);
		SDL_UpdateTexture(vramScreen, NULL, lcd.vram.data(), 128);

		SDL_UpdateTexture(hramScreen, NULL, bus.hram.data(), 16);

		SDL_UpdateTexture(tilemapRaw1, NULL, lcd.vram.data() + 0x1800, 32);
		SDL_UpdateTexture(tilemapRaw2, NULL, lcd.vram.data() + 0x1C00, 32);

		SDL_UpdateTexture(gameboyScreen, NULL, lcd.display.data(), 160);

		// Just for the rendered tilemap we need to be a bit more elaborate
		SDL_LockTexture(tilemap1, NULL, (void**)&tilemappixels1, &tilemappitch1);
		SDL_LockTexture(tilemap2, NULL, (void**)&tilemappixels2, &tilemappitch2);

		// basically this is a quick and dirty PPU background renderer
		// read about it in the wiki if you wanna know more, i cba to 
		// explain it in a comment here
		for (int tileY = 0; tileY < 32; tileY++)
		{
			for (int tileX = 0; tileX < 32; tileX++)
			{
				BYTE tile1ID = lcd.vram[0x1800 + (tileY * 32) + tileX];
				BYTE tile2ID = lcd.vram[0x1C00 + (tileY * 32) + tileX];

				WORD baseAddr = (lcd.lcdc.w.tiledata ? 0x0000 : 0x0800);
				WORD addr1 = baseAddr + (tile1ID * 16);
				WORD addr2 = baseAddr + (tile2ID * 16);

				for (int y = 0; y < 8; y++)
				{
					BYTE lo1 = lcd.vram[addr1 + 2 * y];
					BYTE hi1 = lcd.vram[addr1 + 2 * y + 1];

					BYTE lo2 = lcd.vram[addr2 + 2 * y];
					BYTE hi2 = lcd.vram[addr2 + 2 * y + 1];
					for (int x = 0; x < 8; x++)
					{
						BYTE loVal1 = (lo1 & (0x80 >> x)) >> (7 - x);
						BYTE hiVal1 = (hi1 & (0x80 >> x)) >> (7 - x);

						BYTE loVal2 = (lo2 & (0x80 >> x)) >> (7 - x);
						BYTE hiVal2 = (hi2 & (0x80 >> x)) >> (7 - x);

						tilemappixels1[(tileX * 8 + x) + (32 * 8) * (tileY * 8 + y)] = colormap[loVal1 + (hiVal1 << 1)];
						tilemappixels2[(tileX * 8 + x) + (32 * 8) * (tileY * 8 + y)] = colormap[loVal2 + (hiVal2 << 2)];
					}
				}
			}
		}

		SDL_UnlockTexture(tilemap2);
		SDL_UnlockTexture(tilemap1);


		// Put all the textures in their appropriate ImGui menu
		ImGui::Begin("WRAM");
		ImGui::Image(ramScreen, ImVec2(ImGui::GetWindowContentRegionWidth(), ImGui::GetWindowContentRegionWidth() / ramAR));
		ImGui::End();

		ImGui::Begin("VRAM");
		ImGui::Text("Raw");
		ImGui::Image(vramScreen, ImVec2(ImGui::GetWindowContentRegionWidth(), ImGui::GetWindowContentRegionWidth() / vramAR));

		ImGui::Separator();
		ImGui::Text("Raw Tilemaps");
		if (ImGui::BeginTable("tilemaps", 2))
		{
			ImGui::TableNextColumn(); ImGui::Image(tilemapRaw1, ImVec2(ImGui::GetWindowContentRegionWidth() / 2, ImGui::GetWindowContentRegionWidth() / 2));
			ImGui::TableNextColumn(); ImGui::Image(tilemapRaw2, ImVec2(ImGui::GetWindowContentRegionWidth() / 2, ImGui::GetWindowContentRegionWidth() / 2));
			ImGui::EndTable();
		}

		ImGui::Separator();
		ImGui::Text("Rendered Tilemaps");
		if (ImGui::BeginTable("rawtilemaps", 2))
		{
			ImGui::TableNextColumn(); ImGui::Image(tilemap1, ImVec2(ImGui::GetWindowContentRegionWidth() / 2, ImGui::GetWindowContentRegionWidth() / 2));
			ImGui::TableNextColumn(); ImGui::Image(tilemap2, ImVec2(ImGui::GetWindowContentRegionWidth() / 2, ImGui::GetWindowContentRegionWidth() / 2));
			ImGui::EndTable();
		}
		ImGui::End();

		ImGui::Begin("HRAM");
		ImGui::Image(hramScreen, ImVec2(ImGui::GetWindowContentRegionWidth(), ImGui::GetWindowContentRegionWidth() / hramAR));
		ImGui::End();

		ImGui::Begin("CPU");
		ImGui::Text("-- Registers --");
		if (ImGui::BeginTable("Registers", 6))
		{
			ImGui::TableNextColumn();	ImGui::Text("AF");
			ImGui::TableNextColumn();	ImGui::Text("BC");
			ImGui::TableNextColumn();	ImGui::Text("DE");
			ImGui::TableNextColumn();	ImGui::Text("HL");
			ImGui::TableNextColumn();	ImGui::Text("SP");
			ImGui::TableNextColumn();	ImGui::Text("PC");

			ImGui::TableNextColumn();	ImGui::Text("%04x", cpu.AF.w);
			ImGui::TableNextColumn();	ImGui::Text("%04x", cpu.BC.w);
			ImGui::TableNextColumn();	ImGui::Text("%04x", cpu.DE.w);
			ImGui::TableNextColumn();	ImGui::Text("%04x", cpu.HL.w);
			ImGui::TableNextColumn();	ImGui::Text("%04x", cpu.SP.w);
			ImGui::TableNextColumn();	ImGui::Text("%04x", cpu.PC.w);

			ImGui::EndTable();
		}
		ImGui::Separator();
		ImGui::Text("-- Status Flags --");
		if (ImGui::BeginTable("Flags", 4))
		{
			ImGui::TableNextColumn();	ImGui::Text("Z");
			ImGui::TableNextColumn();	ImGui::Text("N");
			ImGui::TableNextColumn();	ImGui::Text("H");
			ImGui::TableNextColumn();	ImGui::Text("C");

			ImGui::TableNextColumn();	ImGui::Text("%u", cpu.flag->f.zero);
			ImGui::TableNextColumn();	ImGui::Text("%u", cpu.flag->f.negative);
			ImGui::TableNextColumn();	ImGui::Text("%u", cpu.flag->f.halfCarry);
			ImGui::TableNextColumn();	ImGui::Text("%u", cpu.flag->f.carry);

			ImGui::EndTable();
		}
		ImGui::Separator();
		ImGui::Text("-- Interrupts --");
		ImGui::Text("Interrupts: %s", (cpu.ime ? "ON" : "OFF"));
		if (ImGui::BeginTable("Interrupts", 6))
		{
			ImGui::TableNextColumn();	ImGui::Text("");
			ImGui::TableNextColumn();	ImGui::Text("V-Blank");
			ImGui::TableNextColumn();	ImGui::Text("LCD STAT");
			ImGui::TableNextColumn();	ImGui::Text("Timer");
			ImGui::TableNextColumn();	ImGui::Text("Serial");
			ImGui::TableNextColumn();	ImGui::Text("Joypad");

			ImGui::TableNextColumn();	ImGui::Text("IE");
			ImGui::TableNextColumn();	ImGui::Text("%u", cpu.interruptEnable.flags.vblank);
			ImGui::TableNextColumn();	ImGui::Text("%u", cpu.interruptEnable.flags.lcd_stat);
			ImGui::TableNextColumn();	ImGui::Text("%u", cpu.interruptEnable.flags.timer);
			ImGui::TableNextColumn();	ImGui::Text("%u", cpu.interruptEnable.flags.serial);
			ImGui::TableNextColumn();	ImGui::Text("%u", cpu.interruptEnable.flags.joypad);

			ImGui::TableNextColumn();	ImGui::Text("IF");
			ImGui::TableNextColumn();	ImGui::Text("%u", cpu.interruptFlag.flags.vblank);
			ImGui::TableNextColumn();	ImGui::Text("%u", cpu.interruptFlag.flags.lcd_stat);
			ImGui::TableNextColumn();	ImGui::Text("%u", cpu.interruptFlag.flags.timer);
			ImGui::TableNextColumn();	ImGui::Text("%u", cpu.interruptFlag.flags.serial);
			ImGui::TableNextColumn();	ImGui::Text("%u", cpu.interruptFlag.flags.joypad);

			ImGui::EndTable();
		}

		if (bus.cpu->stopped)
		{
			ImGui::Separator();
			ImGui::TextColored(ImVec4(255, 0, 0, 255), "STOPPED");
		}

		if (bus.cpu->halted)
		{
			ImGui::Separator();
			ImGui::TextColored(ImVec4(255, 0, 0, 255), "HALTED");
		}
		ImGui::End();

		ImGui::Begin("OAM");
		if (ImGui::BeginTable("OAM", 5))
		{
			ImGui::TableNextColumn();	ImGui::Text("$");
			ImGui::TableNextColumn();	ImGui::Text("Y");
			ImGui::TableNextColumn();	ImGui::Text("X");
			ImGui::TableNextColumn();	ImGui::Text("#");
			ImGui::TableNextColumn();	ImGui::Text("F");

			for (int i = 0; i < 40; i++)
			{
				WORD addr = i * 4;
				ImGui::TableNextColumn();	ImGui::Text("%02x", 0xFE00 + addr);
				ImGui::TableNextColumn();	ImGui::Text("%03u", lcd.oam[addr + 0x0]);
				ImGui::TableNextColumn();	ImGui::Text("%03u", lcd.oam[addr + 0x1]);
				ImGui::TableNextColumn();	ImGui::Text("%02x", lcd.oam[addr + 0x2]);
				ImGui::TableNextColumn();	ImGui::Text("%02x", lcd.oam[addr + 0x3]);
			}

			ImGui::EndTable();
		}
		ImGui::End();

		ImGui::Begin("Gameboy");
		ImGui::Image(gameboyScreen, ImVec2(ImGui::GetWindowContentRegionWidth(), ImGui::GetWindowContentRegionWidth() / gbAR));
		ImGui::End();

		// Clear screen and render ImGui
		SDL_RenderClear(renderer);
		SDL_RenderCopy(renderer, clearScreen, NULL, NULL);

		ImGui::Render();
		ImGuiSDL::Render(ImGui::GetDrawData());

		SDL_RenderPresent(renderer);
	}

	// Free memory & cleanup
	ImGuiSDL::Deinitialize();
	ImGui::DestroyContext();

	SDL_DestroyTexture(gameboyScreen);
	SDL_DestroyTexture(hramScreen);
	SDL_DestroyTexture(tilemap2);
	SDL_DestroyTexture(tilemap1);
	SDL_DestroyTexture(tilemapRaw2);
	SDL_DestroyTexture(tilemapRaw1);
	SDL_DestroyTexture(vramScreen);
	SDL_DestroyTexture(ramScreen);

	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);

	SDL_Quit();

	return 0;
}
