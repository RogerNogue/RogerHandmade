#ifndef __Component_h__
#define __Component_h__

#define internal_function static
#define local_persistent static
#define global_variable static

struct RenderBufferData
{
	void* BufferMemory;
	int BufferWidth;
	int BufferHeight;
	int Pitch;
	int BytesPerPixel;
};

internal_function void GameUpdateAndRender();

#endif
