@REM this line turns off prints. The @ makes this own line not being printed too
@echo off

IF NOT EXIST C:\Users\Roger\Desktop\Roger\handmade\buildStuff mkdir C:\Users\Roger\Desktop\Roger\handmade\buildStuff
REM popd removes pushd makings
pushd C:\Users\Roger\Desktop\Roger\handmade\buildStuff
REM "-Wall" flag to print all warnings
REM -O2 for optimization
cl -DINTERNAL_BUILD=1 -DSLOW_BUILD=1 -DHANDMADE_WIN32=1 -W4 -WX -wd4201 -wd4100 -Zi C:\Users\Roger\Desktop\Roger\handmade\RogerHandmade\code\win32_handmade.cpp user32.lib gdi32.lib
REM cl /Zi ..\code\win32_handmade.cpp
REM -FC to show full paths
REM cl -F4194304 ... requests for a 4MB stack size

REM notations on compilation flags:
REM -Wall for turn on all warnings (even warnings from windows code get turned on)
REM Then -W1 -W2 -W3... for other levels of warning
REM -WX treats warnings as errors.
REM -wdXXXX (X are numbers) turn off a certain warning
REM google "msvc wall" for more info

REM -Zi produces .pdb files that knows links between .exe and code. It is good for debug
REM -Oi compiler intrinsics, in case the compiler knows how to do a function,
REM it generates the machine code of the  function and avoids doing calls and stuff

REM -GR- turns off runtime type info

REM -EHa- turns off exception handling

REM -nologo removes compiling text


popd
