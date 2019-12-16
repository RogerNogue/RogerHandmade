#ifndef __Component_h__
#define __Component_h__

#define internal_function static
#define local_persistent static
#define global_variable static

#define Pi32 3.14159265359f

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

internal_function void GameUpdateAndRender(RenderBufferData* buffer, int gradXOffset, int gradYOffset, SoundData* soundInfo, int32_t period);

#endif
