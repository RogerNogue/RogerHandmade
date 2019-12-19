#ifndef __Component_h__
#define __Component_h__

#define internal_function static
#define local_persistent static
#define global_variable static

#define Pi32 3.14159265359f

#define Kilobytes(Value) ((Value)*1024)
#define Megabytes(Value) (Kilobytes(Value)*1024)
#define Gigabytes(Value) (Megabytes(Value)*1024)
#define Terabytes(Value) (Gigabytes(Value)*1024)

#if SLOW_BUILD
#define Assert(Expression) if(!(Expression)) *(int*)0 = 0;
#else
#define Assert(Expression)
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
};

struct ButtonState
{
	bool pressedAtEnd;
	int32_t transitions;
};

struct ControllerInput
{
	WORD leftMotorSpeed;
	WORD rightMotorSpeed;
	//joysticks
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
		ButtonState basicButtons[8];
		struct
		{
			ButtonState up;
			ButtonState down;
			ButtonState left;
			ButtonState right;
			ButtonState start;
			ButtonState back;
			ButtonState leftShoulder;
			ButtonState rightShoulder;
		};
	};
};

struct GameInput
{
	ControllerInput controllers[4];
};

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
	int32_t period, GameInput* newInput, GameMemory* gameMem);

#endif
