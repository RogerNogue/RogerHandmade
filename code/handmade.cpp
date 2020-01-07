
#include "handmade.h"

#include <iostream>//c runtime lib for debugging and printing purposes
#include <math.h>//may want to remove this in the future implementing our own stuff

internal_function void controllerReading(int32_t* gradXOffset, int32_t* gradYOffset, GameInput* newInput)
{
	//TODO: Iterate over every connected controller
	if (newInput->controllers[0].up.pressedAtEnd)
	{
		*gradYOffset += 10;
	}
	if (newInput->controllers[0].down.pressedAtEnd)
	{
		*gradYOffset -= 10;
	}
	if (newInput->controllers[0].left.pressedAtEnd)
	{
		*gradXOffset += 10;
	}
	if (newInput->controllers[0].right.pressedAtEnd)
	{
		*gradXOffset -= 10;
	}
	float threshold = 0.5f;
	if (newInput->controllers[0].leftFinalY > threshold || 
		newInput->controllers[0].leftFinalY < -threshold)
	{
		*gradYOffset += int32_t(newInput->controllers[0].leftFinalY * 20);
	}
	if (newInput->controllers[0].leftFinalX > threshold ||
		newInput->controllers[0].leftFinalX < -threshold)
	{
		*gradXOffset -= int32_t(newInput->controllers[0].leftFinalX * 20);
	}
	if (newInput->controllers[0].rightFinalY > threshold ||
		newInput->controllers[0].rightFinalY < -threshold)
	{
		*gradYOffset += int32_t(newInput->controllers[0].rightFinalY * 20);
	}
	if (newInput->controllers[0].rightFinalX > threshold ||
		newInput->controllers[0].rightFinalX < -threshold)
	{
		*gradXOffset -= int32_t(newInput->controllers[0].rightFinalX * 20);
	}
		newInput->controllers[0].leftMotorSpeed = (WORD)(newInput->controllers[0].leftTriggerFinal*65000.0f);
		newInput->controllers[0].rightMotorSpeed = (WORD)(newInput->controllers[0].rightTriggerFinal*65000.0f);
}

internal_function inline void treatSoundKey(ButtonState* key, int32_t* period, int32_t periodValue)
{
	if (key->pressedAtEnd && key->transitions > 0)
	{
		*period = periodValue;
		key->transitions = 0;
	}
}

internal_function inline void treatArrowKey(ButtonState* key, int32_t* gradOffset, int32_t gradNewValue)
{
	if (key->pressedAtEnd)
	{
		//OutputDebugStringA("key pressed\n");
		*gradOffset = gradNewValue;
		key->transitions = 0;
	}
}

internal_function void keyboardTreating(int32_t* period, KeyboardInput* keyboardIn, 
	int32_t* gradXOffset, int32_t* gradYOffset, SoundData* soundInfo)
{
	//C major scale
	treatSoundKey(&keyboardIn->Q, period, soundInfo->samplesPerSec / 261);//C, Do
	treatSoundKey(&keyboardIn->W, period, soundInfo->samplesPerSec / 293);//D, Re
	treatSoundKey(&keyboardIn->E, period, soundInfo->samplesPerSec / 329);//E, Mi
	treatSoundKey(&keyboardIn->A, period, soundInfo->samplesPerSec / 349);//F, Fa
	treatSoundKey(&keyboardIn->S, period, soundInfo->samplesPerSec / 392);//G, Sol
	treatSoundKey(&keyboardIn->D, period, soundInfo->samplesPerSec / 440);//A, La
	treatSoundKey(&keyboardIn->Z, period, soundInfo->samplesPerSec / 493);//B, Si
	treatSoundKey(&keyboardIn->X, period, soundInfo->samplesPerSec / 523);//C, Do

	//screen movement
	treatArrowKey(&keyboardIn->Up, gradYOffset, *gradYOffset + 10);
	treatArrowKey(&keyboardIn->Down, gradYOffset, *gradYOffset - 10);
	treatArrowKey(&keyboardIn->Left, gradXOffset, *gradXOffset - 10);
	treatArrowKey(&keyboardIn->Right, gradXOffset, *gradXOffset + 10);

}

internal_function void generateSound(SoundData* soundInfo, int32_t period)
{
	local_persistent float sineValue;
	int16_t* localBufferPointer = soundInfo->bufferPointer;
	uint32_t pairsOfSamplesToWrite = soundInfo->sizeToWrite / 4;
	for (uint32_t iterator = 0; iterator < pairsOfSamplesToWrite; ++iterator)
	{
		float sinResult = sinf(sineValue);
		int16_t sampleValue = (int16_t)(sinResult * soundInfo->soundVolume);

		*localBufferPointer++ = sampleValue;//left ear sample
		*localBufferPointer++ = sampleValue;//right ear sample
		sineValue += 2.0f * Pi32 / (float)period;

		if (sineValue > 16000.0f)
		{
			//in case sine value gets too big, we lower it maintaining the result.
			sineValue = fmod(sineValue, (2.0f * Pi32));
		}
	}
}

//function that paints a gradient
internal_function void renderGradient(const RenderBufferData* Buffer, int gradXOffset, int gradYOffset)
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
			uint8_t R = (uint8_t)(i + gradYOffset);
			uint8_t G = (uint8_t)(j + gradXOffset);
			uint8_t B = (uint8_t)(gradXOffset + gradYOffset);

			//we store it as said above
			*pixel = (uint32_t)(B | (G << 8) | (R << 16));
			++pixel;
		}
		row += Buffer->Pitch;
	}
}

internal_function void LoadIO(GameMemory* gameMem, FileInfo* fileRed)
{
	if (!PlatformReadEntireFile(__FILE__, fileRed))
	{
		return;
	}
	char* newFile = "OutputTest.txt";//auto writes into the working directory

	//IMPORTANT NOTE: we will want to write into a different file so that we dont overwrite the one we are using
	//because in case write fails, we dont run into a corrupted file
	PlatformWriteToFile(newFile, fileRed);

	PlatformFreeFileMemory(fileRed);
}

internal_function void GameUpdateAndRender(RenderBufferData* buffer, SoundData* soundInfo,
	GameInput* newInput, KeyboardInput* keyboardIn, GameMemory* gameMem)
{
	//TODO: CHECK there is a keyboard plugged in
	Assert(sizeof(GameState) <= gameMem->persistentMemorySize);

	GameState* gameState = (GameState*)gameMem->persistentMemory;
	
	local_persistent int32_t period = soundInfo->samplesPerSec / 261;
	gameState->toneHz = 261;
	FileInfo fileRed;

	if (!gameMem->isInitialized)
	{
		LoadIO(gameMem, &fileRed);//reads from the file and generates another one using file write and read.
		gameMem->isInitialized = true;
	}
	//local_persistent period;
	controllerReading(&gameState->xOffset, &gameState->yOffset, newInput);
	keyboardTreating(&period, keyboardIn, &gameState->xOffset, 
		&gameState->yOffset, soundInfo);

	generateSound(soundInfo, period);
	renderGradient(buffer, gameState->xOffset, gameState->yOffset);
}