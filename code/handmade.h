#ifndef __Component_h__
#define __Component_h__

#define internal_function static
#define local_persistent static
#define global_variable static

#define Pi32 3.14159265359f

#define Kilobytes(Value) ((Value)*1024LL)
#define Megabytes(Value) (Kilobytes(Value)*1024LL)
#define Gigabytes(Value) (Megabytes(Value)*1024LL)
#define Terabytes(Value) (Gigabytes(Value)*1024LL)

//ArraySize function
#define ArraySize(arrayParameter) (sizeof(arrayParameter) / sizeof((arrayParameter)[0]))

#if SLOW_BUILD
#define Assert(Expression) if(!(Expression)) *(int*)0 = 0;
#else
#define Assert(Expression)
#endif

//File I/O non shippable functions
#if INTERNAL_BUILD
struct FileInfo
{
	uint32_t memSize;
	uint64_t* memPointer;
};

internal_function bool PlatformReadEntireFile(char* filename, FileInfo* result);
internal_function bool PlatformFreeFileMemory(FileInfo* result);
internal_function bool PlatformWriteToFile(char* filename, FileInfo* writeInfo);

#endif

struct RenderBufferData
{
	void* BufferMemory;
	int BufferWidth;
	int BufferHeight;
	int Pitch;
	int BytesPerPixel;
};

struct SoundData
{
	int16_t* bufferPointer;
	uint32_t sizeToWrite;
	uint16_t soundVolume;
	int32_t period;
	int32_t samplesPerSec;
};

struct ButtonState
{
	bool pressedAtEnd;
	int32_t transitions;
};

//TODO map the controller and keyboard buttons into game actions
//in order to unify them together.
struct ControllerInput
{
	bool isConnected;

	WORD leftMotorSpeed;
	WORD rightMotorSpeed;
	//joysticks
	//TODO: Probably in the future we want joysticks to be regular buttons
	//And left joystick to be the same as arrows
	float leftMaxX;
	float leftMaxY;
	float rightMaxX;
	float rightMaxY;

	float leftMinX;
	float leftMinY;
	float rightMinX;
	float rightMinY;

	float leftFinalX;
	float leftFinalY;
	float rightFinalX;
	float rightFinalY;

	//short 0-255, triggers
	float leftTriggerMax;
	float rightTriggerMax;
	
	float leftTriggerMin;
	float rightTriggerMin;
	
	float leftTriggerFinal;
	float rightTriggerFinal;

	//simple buttons
	union
	{
		//TODO: add arrows here
		ButtonState basicButtons[8];
		struct
		{
			ButtonState leftShoulder;
			ButtonState rightShoulder;
			ButtonState up;
			ButtonState left;
			ButtonState down;
			ButtonState right;
			ButtonState start;
			ButtonState back;
			

			ButtonState LastButton;
		};
	};
};

struct KeyboardInput
{
	union
	{
		ButtonState keyboardButtons[14];
		struct
		{
			ButtonState Q;
			ButtonState E;

			ButtonState W;
			ButtonState A;
			ButtonState S;
			ButtonState D;

			ButtonState Z;
			ButtonState X;

			ButtonState Up;
			ButtonState Down;
			ButtonState Left;
			ButtonState Right;

			ButtonState Spacebar;
			ButtonState Escape;

			ButtonState LastButton;
		};
	};
	

};

struct GameInput
{
	ControllerInput controllers[4];
};
inline ControllerInput* GetController(GameInput* input, uint32_t index) 
{
	Assert(index < ArraySize(input->controllers));
	return &input->controllers[index];
}

struct GameState
{
	int32_t xOffset;
	int32_t yOffset;
	int32_t toneHz;
};

struct GameMemory
{
	bool isInitialized;
	uint64_t persistentMemorySize;
	void* persistentMemory;

	uint64_t transistentMemorySize;
	void* transistentMemory;
};

struct Timers
{
	float milisecs;
	float cycles;
};

internal_function void GameUpdateAndRender(RenderBufferData* buffer, SoundData* soundInfo,
	GameInput* newInput, KeyboardInput* keyboardIn, GameMemory* gameMem);

#endif
