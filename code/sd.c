/* 
TODO: THIS IS NOT A FINAL SDL LAYER YET

- Saved game location
- Getting a handle to our executable file
- Asset location path
- Threading (launch a thread)
- Raw Input (Support for multiple keyboard)
- Sleep/timeBeginPeriod
- Clipcursor (Multi monitor support)
- Fullscreen support
- WM_SetCursor (game cursor visibility) 
- QueryCancelAutoplay
- WM_ActivateApp (for when we are not the active application)
- Blit speed improvements (BitBlt)
- Hardware Acceleration (OpenGL or Direct3D or both)
- GetKeyBoardLayout (for french keyboard, international WASD support)

JUST A PARTIAL LIST
 */
#include <SDL.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <dlfcn.h>

#include "hm.h"

typedef struct {
	char *Beginning;
	u32 Size;
	u32 PlayCursor;
	u32 WriteCursor;
} sdl_ring_buffer;

typedef struct {
	void *GameCode;
	struct timespec ModTime;
	game_initialize_state *InitializeState;
	game_update_and_render *UpdateAndRender;
} sdl_game_code;

#define Max_File_Path_Length  300

internal_function void DEBUG_DrawVerticalLine(game_video_buffer *Buffer, u16 X, u16 Y, u16 Length, u32 Color)
{
	u32 *Pixel = (u32 *)Buffer->Beginning + Y * Buffer->Width + X; 
	for (int j = 0; j < Length; ++j) {// j: index of a pixel in a line
		*Pixel = Color; 
		Pixel += Buffer->Width;
	}
}

internal_function void ReadFile(int FD, void *Memory, size_t Size)
{
	while (Size != 0) {
		ssize_t NewRead = read(FD, Memory, Size);
		int errsv = errno;
		if (NewRead == -1) {
			printf("Read File Error: %s\n", strerror(errsv));
			Assert(!"Stop");
		}
		else {
			Size -= NewRead;
		}
	}
}

internal_function void WriteFile(int FD, void *Memory, size_t Size)
{
	while (Size != 0) {
		ssize_t NewWrite = write(FD, Memory, Size);
		int errsv = errno;
		if (NewWrite == -1) {
			printf("Write File Error: %s\n", strerror(errsv));
			Assert(!"Stop");
		}
		else {
			Size -= NewWrite;
		}
	}
}

internal_function void CatString(char *A, char *B, char *Out)
{ // this works for strings that end in '\0'

	u8 Index = 0;
	for (int i = 0; A[i] != '\0'; ++i) {
		Out[Index] = A[i];
		++Index;
	}

	for (int i = 0; B[i] != '\0'; ++i) {
		Out[Index] = B[i];
		++Index;
	}

	Out[Index] = '\0';
}


GAME_UPDATE_AND_RENDER(GameUpdateAndRenderStub)
{
}

GAME_INITIALIZE_STATE(GameInitializeStateStub)
{
}

internal_function sdl_game_code SDLLoadGameCode(char *FileName)
{
	sdl_game_code Result;

	struct stat FileStat;
	stat(FileName, &FileStat);
	Result.ModTime = FileStat.st_mtim;

	Result.GameCode = dlopen(FileName, RTLD_LAZY);
	if (Result.GameCode != 0) {
		Result.InitializeState = dlsym(Result.GameCode, "GameInitializeState");
		Result.UpdateAndRender = dlsym(Result.GameCode, "GameUpdateAndRender");
	}
	else {
		Result.InitializeState = GameInitializeStateStub;
		Result.UpdateAndRender = GameUpdateAndRenderStub;
	}

	return Result;    
}

internal_function void SDLUnloadGameCode(sdl_game_code *Game)
{
	if (Game->GameCode != 0) {
		dlclose(Game->GameCode);
	}

	Game->InitializeState = GameInitializeStateStub;
	Game->UpdateAndRender = GameUpdateAndRenderStub;
}

internal_function void SDLAudioCallback(void *UserData, Uint8 *Stream, int Length)
{
	sdl_ring_buffer *Buffer = (sdl_ring_buffer *)UserData;
	char *StartByte = Buffer->Beginning;
	char *PlayByte = StartByte + Buffer->PlayCursor;
	u32 Region1Size = Buffer->Size - Buffer->PlayCursor;

	if (Region1Size > Length) {
		memcpy(Stream, PlayByte, Length);
		Buffer->PlayCursor += Length;
	}
	else {
		u32 Region2Size = Length - Region1Size;
		memcpy(Stream, PlayByte, Region1Size);
		memcpy(Stream + Region1Size, StartByte, Region2Size);
		Buffer->PlayCursor = Region2Size;
	}
}

internal_function DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile)
{
	debug_read_file_result Result;
	Result.Beginning = 0;
	Result.Size = 0;
	int FileHandle = open(FileName, O_RDONLY);
	if (FileHandle != -1) {
		struct stat FileStat;
		if (fstat(FileHandle, &FileStat) != -1) {
			Result.Beginning = malloc(FileStat.st_size);
			Result.Size = SafeTruncate64(FileStat.st_size);
			if (Result.Beginning != 0) {
				int BytesRead = read(FileHandle, Result.Beginning, Result.Size);
				if (BytesRead == Result.Size) {
					// NOTE: File read successfully
				}
				else {
					// TODO: Logging can't read from file
					free(Result.Beginning);
					Result.Beginning = 0;
					Result.Size = 0;
				}
			}
			else {
				// TODO: Logging can't allocate memory
			}
		}
		else {
			// TODO: Logging can't get file stat 
		}
		close(FileHandle);
	}
	else {
		// TODO: Logging can't open the file
	}
	return Result;
}

internal_function DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemory)
{
	free(Memory);
}

internal_function DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFile)
{
	bool Result = false;
	int FileHandle = open(FileName, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (FileHandle != -1) {
		int BytesWrote = write(FileHandle, Memory, MemorySize);
		if (BytesWrote == MemorySize) {
			// NOTE: File wrote successfully
			Result = true;
		}
		else {
			// TODO: Logging can't write to file
		}
		close(FileHandle);
	}
	else {
		// TODO: Logging can't open file
	}
	return Result;
}

internal_function inline struct timespec Plus_timespec(struct timespec *A, struct timespec *B)
{
	struct timespec Result;
	Result.tv_nsec = A->tv_nsec + B->tv_nsec;
	Result.tv_sec = A->tv_sec + B->tv_sec;
	if (Result.tv_nsec > Giga) {
		Result.tv_nsec -= Giga;
		++Result.tv_sec;
	}
	return Result;
}

internal_function inline bool Less_timespec(struct timespec *A, struct timespec *B)
{
	if (A->tv_sec < B->tv_sec) {
		return true;
	}
	if ((A->tv_sec == B->tv_sec) && (A->tv_nsec < B->tv_nsec)) {
		return true;
	}
	return false;
}

internal_function inline struct timespec Minus_timespec(struct timespec *A, struct timespec *B)
{
	//Assert(Less_timespec(B,A));
	struct timespec Result;
	Result.tv_nsec = A->tv_nsec - B->tv_nsec;
	Result.tv_sec = A->tv_sec - B->tv_sec;
	if (Result.tv_nsec < 0) {
		Result.tv_nsec += Giga;
		--Result.tv_sec;
	}
	return Result;
}

int main(int argc, char *argv[])
{
	// memory
	game_memory Memory = {};

#if HANDMADE_INTERNAL
	void *BaseAddress = (void *)(BTera);
#else
	void *BaseAddress = 0;
#endif
	Memory.PermanentStorageSize =  64 * BMega;
	Memory.TransientStorageSize =  100 * BMega;    
	Memory.TotalSize = Memory.TransientStorageSize + Memory.PermanentStorageSize;
	// note: maybe you need to change the size of the virtual page sizes from 4k to 4m or something
	// to make the copy faster.
	Memory.Beginning = mmap(BaseAddress,
			Memory.TotalSize,
			PROT_READ | PROT_WRITE,
			MAP_ANONYMOUS | MAP_PRIVATE,
			-1,
			0);

	Memory.TransientStorage = (u8 *)BaseAddress + Memory.PermanentStorageSize; 

	Assert(sizeof(game_state) < Memory.PermanentStorageSize);

	Memory.DEBUGPlatformReadEntireFile = &DEBUGPlatformReadEntireFile;
	Memory.DEBUGPlatformFreeFileMemory = &DEBUGPlatformFreeFileMemory;
	Memory.DEBUGPlatformWriteEntireFile = &DEBUGPlatformWriteEntireFile;

	// load game code 
	// find the directory of the executable
	char GameDirectory[Max_File_Path_Length];
	char GameCodeLibrary[Max_File_Path_Length];
	{
		char FullPath[Max_File_Path_Length];
		int PathLength = readlink("/proc/self/exe", FullPath, Max_File_Path_Length); 
		if (Max_File_Path_Length == PathLength || PathLength == -1) {
			printf("Readlink failed to find the path of the executable\n");
			// it might have truncated the path because path was too long
			// or readlink just failed
			return -1;
		}
		int LastSlash = -1;
		for (int i = 0; i < PathLength; ++i) {
			if (FullPath[i] == '/') {
				LastSlash = i;
			}
		}

		for (int i = 0; i < LastSlash + 1; ++i) {
			GameDirectory[i] = FullPath[i];
		}
		GameDirectory[LastSlash+1] = '\0';

		char LibraryName[] = "hm.so";
		CatString(GameDirectory, LibraryName, GameCodeLibrary);
	}
	sdl_game_code Game = SDLLoadGameCode(GameCodeLibrary);

	// initialise game
	thread_context Thread;
	Game.InitializeState(&Thread, &Memory);

	// sdl
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC | SDL_INIT_AUDIO);

	// cursor
	SDL_Cursor *DEBUGCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_CROSSHAIR);
	SDL_Cursor *DefaultCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
	SDL_SetCursor(DefaultCursor);

	// sdl texture
	game_video_buffer VideoBuffer = {};
	u32 MaxBufferWidth = 960;
	u32 MaxBufferHeight = 540;
	VideoBuffer.Width = MaxBufferWidth; // initial width
	VideoBuffer.Height = MaxBufferHeight; // initial height
	int BytesPerPixel =  4;	
	VideoBuffer.Pitch = BytesPerPixel * VideoBuffer.Width;
	int WindowWidth = 1200;
	int WindowHeight = 800;

	SDL_Window *Window = SDL_CreateWindow("New Hero", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WindowWidth, WindowHeight, SDL_WINDOW_RESIZABLE);
	if (Window == 0) {
		// TODO: logging
		return -1;
	}
	bool GameIsFullscreen = false;
	SDL_Renderer *Renderer = SDL_CreateRenderer(Window, -1, SDL_RENDERER_SOFTWARE);
	if (Renderer == 0) {
		// TODO: logging
		return -1;
	}

	VideoBuffer.Beginning = mmap(0, BytesPerPixel * 1920 * 1080, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0); // maximum resolution 1920x1080
	SDL_Texture *Texture = SDL_CreateTexture(Renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, VideoBuffer.Width, VideoBuffer.Height);

	// get monitor refresh rate
	u8 MonitorRefreshRate;
	SDL_DisplayMode DisplayMode;
#if 0
	if (SDL_GetDesktopDisplayMode(0, &DisplayMode) == 0) {
		MonitorRefreshRate = DisplayMode.refresh_rate;
	}
	else {
		MonitorRefreshRate = 60;
		printf("Error: %s\n", SDL_GetError());
	}
#else
	MonitorRefreshRate = 30;
#endif
	r32 TargetFrameLength =  1.0f / (r32)MonitorRefreshRate; 

	// sdl controller
	SDL_GameController *ControllerHandles[MAX_CONTROLLER] = {};
	SDL_Haptic *HapticHandles[MAX_CONTROLLER] = {};
	for (int i = 0; i < SDL_NumJoysticks(); ++i) { // i: controller index
		if (SDL_IsGameController(i)) {
			ControllerHandles[i] = SDL_GameControllerOpen(i);
			SDL_Joystick *Joystick = SDL_GameControllerGetJoystick(ControllerHandles[i]);
			if (SDL_JoystickIsHaptic(Joystick)) {
				SDL_Haptic *HapticHandle = SDL_HapticOpenFromJoystick(Joystick);
				if (SDL_HapticRumbleSupported(HapticHandle)) {
					HapticHandles[i] = HapticHandle;
					SDL_HapticRumbleInit(HapticHandle);
				}
				else {
					SDL_HapticClose(HapticHandle);	
					HapticHandles[i] = 0;
				}
			}
			else {
				SDL_JoystickClose(Joystick);
			}
		}
	}

	// sdl controller buttons
	SDL_GameControllerButton ControllerButton[CONTROLLER_MAX_BUTTON] = {
		SDL_CONTROLLER_BUTTON_A        , SDL_CONTROLLER_BUTTON_B         , SDL_CONTROLLER_BUTTON_X,
		SDL_CONTROLLER_BUTTON_Y        , SDL_CONTROLLER_BUTTON_DPAD_UP   , SDL_CONTROLLER_BUTTON_DPAD_DOWN,
		SDL_CONTROLLER_BUTTON_DPAD_LEFT, SDL_CONTROLLER_BUTTON_DPAD_RIGHT, SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
		SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, SDL_CONTROLLER_BUTTON_START, SDL_CONTROLLER_BUTTON_BACK};

	SDL_Scancode KeyboardButton[CONTROLLER_MAX_BUTTON] = {
		SDL_SCANCODE_W , SDL_SCANCODE_S   , SDL_SCANCODE_A   , SDL_SCANCODE_D,
		SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT,
		SDL_SCANCODE_Q,  SDL_SCANCODE_E, SDL_SCANCODE_X, SDL_SCANCODE_BACKSPACE};

	SDL_GameControllerAxis ControllerAxis[CONTROLLER_MAX_AXIS] = {
		SDL_CONTROLLER_AXIS_LEFTX, SDL_CONTROLLER_AXIS_LEFTY};

	game_input Input = {};
	game_input InputSentToGame = {};

	// sdl audio
	SDL_AudioSpec AudioSettings;
	AudioSettings.freq = 48000;
	AudioSettings.format = AUDIO_S16LSB;
	AudioSettings.channels = 2;
	AudioSettings.samples = 1024;  
	AudioSettings.callback = &SDLAudioCallback; 

	sdl_ring_buffer AudioRingBuffer;
	AudioSettings.userdata = &AudioRingBuffer;
	SDL_OpenAudio(&AudioSettings, 0);
	if (AudioSettings.format != AUDIO_S16LSB) {
		// TODO make the code work with other formats as well?
		return -1;
	}

	game_audio_buffer AudioBuffer;
	AudioBuffer.Frequency = AudioSettings.freq;
	AudioBuffer.Channels = AudioSettings.channels;
	AudioBuffer.FormatSize = sizeof(int16_t); // AUDIO_S16LSB -> int16_t
	AudioBuffer.BytesPerSample = AudioBuffer.Channels * AudioBuffer.FormatSize;
	int MaxAudioBufferSize = 2 * AudioSettings.freq * TargetFrameLength; // 2* to make the buffer a bigger than 1 frame.
	AudioBuffer.Size = 0;
	AudioBuffer.Beginning = mmap(0, AudioBuffer.BytesPerSample * MaxAudioBufferSize, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);	

	// initialise AudioRingBuffer	
	AudioRingBuffer.Size = AudioBuffer.BytesPerSample * AudioSettings.samples * 50; // 2? To make sure buffer is bigger than the samples audio device asks for
	AudioRingBuffer.Beginning = (char *)malloc(AudioRingBuffer.Size);
	memset(AudioRingBuffer.Beginning, 0, AudioRingBuffer.Size);
	AudioRingBuffer.WriteCursor = 0;
	AudioRingBuffer.PlayCursor = 0;

	// game record and replay
	bool GameIsRecording = false;
	bool GameIsReplaying = false;
	u16 RecordLength = 0;
	u16 ReplayCursor = 0;	
	char RecordFileName[] = "game-record.d";
	char RecordFileAddress[Max_File_Path_Length];
	CatString(GameDirectory, RecordFileName, RecordFileAddress);
	int RecordFileHandle = open(RecordFileAddress, O_RDWR | O_CREAT, S_IRWXU);
	void *RecordBuffer = mmap(0, Memory.TotalSize, PROT_READ | PROT_WRITE, MAP_SHARED, RecordFileHandle, 0);
	// mmap here gets an empty file, so the memcpy will fail. so should extend the file first so:
	ftruncate(RecordFileHandle, Memory.TotalSize);

	//game time
	local_persist game_time GameTime;
	GameTime.Elapsed = TargetFrameLength;

	// game loop
	bool GameIsRunning = true;
	bool GameIsPaused = false;
	bool FirstLoop = true;
	while (GameIsRunning) {
		struct stat LibraryStat;
		stat(GameCodeLibrary, &LibraryStat);
		if (Less_timespec(&Game.ModTime, &LibraryStat.st_mtim)) {
			SDLUnloadGameCode(&Game);
			Game = SDLLoadGameCode(GameCodeLibrary);
		}

		for (int i = 0; i < MAX_INPUT_DEVICE; ++i) {
			for (int j = 0; j < CONTROLLER_MAX_BUTTON; ++j) {
				Input.InputDevice[i].Buttons[j].HalfTransitionCount = 0;
			}
		}

		SDL_Event Event;
		while (SDL_PollEvent(&Event)) {
			if ( Event.type == SDL_QUIT ) {
				GameIsRunning = false;
			} 
			else if (Event.type == SDL_MOUSEMOTION) {
				SDL_SetCursor(DEBUGCursor);
			}
			else if (Event.type == SDL_WINDOWEVENT) {
				if (Event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
					// TODO: this is wrong I guess???
					if (MaxBufferWidth > Event.window.data1) {
						VideoBuffer.Width = Event.window.data1;
					}
					else {
						VideoBuffer.Width = MaxBufferWidth;
					}
					if (VideoBuffer.Height > Event.window.data2) {
						VideoBuffer.Height = Event.window.data2;
					}
					else {
						VideoBuffer.Height = MaxBufferHeight;
					}
					VideoBuffer.Pitch = BytesPerPixel * VideoBuffer.Width;	

					SDL_DestroyTexture(Texture);
					Texture = SDL_CreateTexture(Renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, VideoBuffer.Width, VideoBuffer.Height);					
				} 
				else if (Event.type == SDL_WINDOWEVENT_RESIZED) {
					// TODO: I don't know what to do
				}
			} 
			else if (Event.type == SDL_KEYUP || Event.type == SDL_KEYDOWN) {
				// determine the type of event
				if (Event.key.keysym.scancode == SDL_SCANCODE_ESCAPE && Event.type == SDL_KEYUP) {
					GameIsRunning = false;
				}

				if (Event.key.keysym.scancode == SDL_SCANCODE_SPACE && Event.type == SDL_KEYUP) {
					if (GameIsPaused == true) {
						GameIsPaused = false;
						SDL_PauseAudio(0);
					}
					else {
						GameIsPaused = true;
						SDL_PauseAudio(1);
					}
				}

				if (Event.key.keysym.scancode == SDL_SCANCODE_RETURN && Event.type == SDL_KEYUP) {
					if (GameIsFullscreen == true) {
						SDL_SetWindowFullscreen(Window, 0);
						GameIsFullscreen = false; 
					}
					else {
						SDL_SetWindowFullscreen(Window, SDL_WINDOW_FULLSCREEN_DESKTOP);
						GameIsFullscreen = true; 
					}
				}

				if (Event.key.keysym.scancode == SDL_SCANCODE_L && !GameIsReplaying && Event.type == SDL_KEYUP) {
					if (GameIsRecording == true) {
						GameIsRecording = false;
					}
					else {
						GameIsRecording = true;
						RecordLength = 0;
						memcpy(RecordBuffer, Memory.Beginning, Memory.TotalSize);
						lseek(RecordFileHandle, Memory.TotalSize, SEEK_SET);
					}
				}

				if ( Event.key.keysym.scancode == SDL_SCANCODE_K && !GameIsRecording && Event.type == SDL_KEYUP) {
					if (GameIsReplaying == true) {
						GameIsReplaying	= false;
					}
					else if(RecordLength > 0) {
						GameIsReplaying = true;
						ReplayCursor = 0;
					}
				}

				// game keys
				bool IsDown;

				if (Event.type == SDL_KEYDOWN) {
					IsDown = true;
				} 
				else {
					IsDown = false;
				}

				// act according to the type
				for (int i = 0; i < CONTROLLER_MAX_BUTTON; ++i) { // i: KeyboardButton index
					// by default keyboard is controller 0
					if (Event.key.keysym.scancode == KeyboardButton[i]) {
						if (Input.Keyboard.Buttons[i].EndedDown != IsDown) {
							++Input.Keyboard.Buttons[i].HalfTransitionCount;
						}
						Input.Keyboard.Buttons[i].EndedDown = IsDown;
					}
				}
			} 
			else {
				// TODO Logging
			}			
		}
		// get mouse input
		u32 MouseBitMask = SDL_GetMouseState(&Input.MouseX, &Input.MouseY);
		Input.MouseButtonLeft.EndedDown = MouseBitMask & SDL_BUTTON(SDL_BUTTON_LEFT);
		Input.MouseButtonMiddle.EndedDown = MouseBitMask & SDL_BUTTON(SDL_BUTTON_MIDDLE);
		Input.MouseButtonRight.EndedDown = MouseBitMask & SDL_BUTTON(SDL_BUTTON_RIGHT);

		// get controller input
		for (int i = 0; i < MAX_CONTROLLER; ++i) { // i: controller index
			SDL_GameController *Controller = ControllerHandles[i];
			game_controller_state *ControllerState = &Input.Controller[i]; 
			if (Controller !=0 && SDL_GameControllerGetAttached(Controller)) {
				for (int j = 0; j < CONTROLLER_MAX_BUTTON; ++j) { // j: button index
					bool IsDown = SDL_GameControllerGetButton(Controller, ControllerButton[j]);
					if (ControllerState->Buttons[j].EndedDown != IsDown) {
						++ControllerState->Buttons[j].HalfTransitionCount;
					}
					ControllerState->Buttons[j].EndedDown = IsDown;	
				}

				ControllerState->Analog = true;
				for (int i = 0; i < CONTROLLER_MAX_AXIS; ++i) { // i axis index
					int16_t LeftStick = SDL_GameControllerGetAxis(Controller, ControllerAxis[i]);
					if (LeftStick < -CONTROLLER_DEAD_ZONE) {
						ControllerState->Axis[i] = ((r32)LeftStick + CONTROLLER_DEAD_ZONE) / (32768 - CONTROLLER_DEAD_ZONE);
					}
					else if (LeftStick > CONTROLLER_DEAD_ZONE){
						ControllerState->Axis[i] = ((r32)LeftStick - CONTROLLER_DEAD_ZONE) / (32767 - CONTROLLER_DEAD_ZONE);
					}
					else {
						ControllerState->Axis[i] = 0;
					}
				}
			}
			else {
				// controller not attached
			}
		}

		// TODO (ramin): this is not a perfect pause. because if you pause the game and
	    // then resize the window you get garbage in the screen. 
		// fix this when the time comes.	
		if (!GameIsPaused) {
			Game.UpdateAndRender(&Thread, &GameTime, &Memory, &InputSentToGame, &VideoBuffer, &AudioBuffer);

			{
				if (GameIsRecording) {
					write(RecordFileHandle, &Input, sizeof(Input));
					++RecordLength;
				}

				if (GameIsReplaying) {
					if (ReplayCursor == 0) {
						lseek(RecordFileHandle, Memory.TotalSize, SEEK_SET);
						memcpy(Memory.Beginning, RecordBuffer, Memory.TotalSize);
					}

					read(RecordFileHandle, &InputSentToGame, sizeof(InputSentToGame));

					++ReplayCursor;
					if (ReplayCursor == RecordLength) {
						ReplayCursor = 0;
					}
				}
				else {
					InputSentToGame = Input;
				}

				// update cycle audio buffer
				char *StartByte = AudioRingBuffer.Beginning;
				char *WriteByte = StartByte + AudioRingBuffer.WriteCursor;
				u32 Region1Size = AudioRingBuffer.Size - AudioRingBuffer.WriteCursor;

				u32 InputSize = 4 * AudioBuffer.Size;
				char *InputBeginning = (char *)AudioBuffer.Beginning;

				if (Region1Size > InputSize) {
					memcpy(WriteByte, InputBeginning, InputSize);
					AudioRingBuffer.WriteCursor += InputSize;
				}
				else {
					u16 Region2Size = InputSize - Region1Size;
					memcpy(WriteByte, InputBeginning, Region1Size);
					memcpy(StartByte, InputBeginning + Region1Size, Region2Size);
					AudioRingBuffer.WriteCursor = Region2Size;
				}

				//if (GameTime.Current > 3 * 1000 * TargetFrameLength) { // to make it wait at least for 3 frames. 1000 to make it in miliseconds
				//	SDL_PauseAudio(0);
				//}
			}
		}

		{ // force frame rate
			local_persist struct timespec TargetFrameTime;
			if (FirstLoop) {
				TargetFrameTime.tv_sec = 0;
				TargetFrameTime.tv_nsec = (long)(Giga*TargetFrameLength);
			}

			local_persist struct timespec ExitTime = {0,0};
			struct timespec TargetExit = Plus_timespec(&ExitTime, &TargetFrameTime);
			struct timespec EnterTime;
			clock_gettime(CLOCK_REALTIME, &EnterTime);
			if (Less_timespec(&EnterTime , &TargetExit)) {
				struct timespec Request = Minus_timespec(&TargetExit, &EnterTime);
				struct timespec Remaining = {0,0};
				while (nanosleep(&Request, &Remaining) == -1) {
					Request = Remaining;
				}
			}
			else {
				// TODO: Logging we missed a frame rate
				if (FirstLoop) {
					// do nothing
				}
				else {
					printf("We Missed a frame\n");
				}
			}
			clock_gettime(CLOCK_REALTIME, &ExitTime);
		}

#if 0
		if (!GameIsPaused) {
			
			// record the last positions of WriteCursor and PlayCursor
			u8 LinesNum = 25;
			local_persist u16 WriteCursorX[25] = {}; 
			local_persist u16 PlayCursorX[25] = {}; 
			local_persist u8 I = 0;
			u16 BorderX = 20;

			u16 TopX = BorderX + (VideoBuffer.Width - 2*BorderX) * AudioRingBuffer.WriteCursor / AudioRingBuffer.Size;
			WriteCursorX[I] = TopX;
			TopX = BorderX + (VideoBuffer.Width - 2*BorderX) * AudioRingBuffer.PlayCursor / AudioRingBuffer.Size;
			PlayCursorX[I] = TopX;

			++I;
			if (I == LinesNum) {
				I = 0;
			}
			
 			// draw audio sync lines
			for (int i = 0; i < LinesNum; ++i) {// i: line index
				// ...(.. , x , y , length , color)
				DEBUG_DrawVerticalLine(&VideoBuffer, WriteCursorX[i], 100, 100, 0x00FF0000);
				DEBUG_DrawVerticalLine(&VideoBuffer, PlayCursorX[i], 150, 100, 0x0000FFFF);
			}
		}
#endif

		// update texture
		SDL_UpdateTexture(Texture, 0, VideoBuffer.Beginning, VideoBuffer.Pitch);
		SDL_Rect DestRect;
		DestRect.x = 10;
		DestRect.y = 10;
		DestRect.w = VideoBuffer.Width;
		DestRect.h = VideoBuffer.Height;
		SDL_RenderCopy(Renderer, Texture, 0, &DestRect);
		SDL_RenderPresent(Renderer);

#if 0	
		{ // performance
			local_persist struct timespec PreviousTime;

			struct timespec CurrentTime;
			clock_gettime(CLOCK_REALTIME, &CurrentTime);

			if (FirstLoop) {
				// do nothing
			}
			else {
				u64 Nanos = Giga * (CurrentTime.tv_sec - PreviousTime.tv_sec) + CurrentTime.tv_nsec - PreviousTime.tv_nsec;
				printf("Frame Length: %lu\n", Nanos);
			}
			PreviousTime = CurrentTime;
		}
#endif	
		FirstLoop = false;
	}
	return(0);
}
