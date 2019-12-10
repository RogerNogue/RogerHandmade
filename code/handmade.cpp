
#include "handmade.h"

//function that paints a gradient
void renderGradient(const RenderBufferData* Buffer, int gradXOffset, int gradYOffset)
{
	//lets paint pixels, first we call a small raw pointer to manage memory
	//row has to be in bytes, if not when we do pointer arithmetic we would
	//increase the pointer way more space than we need and problems would occur.
	uint8_t* row = (uint8_t*)Buffer->BufferMemory;

	for (int i = 0; i < Buffer->BufferHeight; ++i)
	{
		uint32_t* pixel = (uint32_t*)row;
		for (int j = 0; j < Buffer->BufferWidth; ++j)
		{
			/*
			IMPORTANT! MEM explanation
			Little endian architecture: the first byte goes to the last part and so on.

			REASON: you have to store it BB GG RR XX because then when windows load the pixel,
			they want it to be seen as XX RR GG BB.

			and to have in mem BB GG RR XX, you need in the uint32: 0x XX RR GG BB.

			If we did it by uint8, we would have to first store BB at the first byte, then GG,
			then RR, and finally XX.
			*/

			//now we paint the pixel in a more direct manner
			uint8_t R = (uint8_t)i + gradYOffset;
			uint8_t G = (uint8_t)j + gradXOffset;
			uint8_t B = (uint8_t)(gradXOffset + gradYOffset);

			//we store it as said above
			*pixel = (uint32_t)(B | (G << 8) | (R << 16));
			++pixel;
		}
		row += Buffer->Pitch;
	}
}

void GameUpdateAndRender(RenderBufferData* buffer, int gradXOffset, int gradYOffset)
{
	renderGradient(buffer, gradXOffset, gradYOffset);
}