@REM this line turns off prints. The @ makes this own line not being printed too
@echo off

mkdir C:\Users\Roger\Desktop\Roger\handmade\buildStuff
REM popd removes pushd makings
pushd C:\Users\Roger\Desktop\Roger\handmade\buildStuff
REM "-Wall" flag to print all warnings
cl -Zi C:\Users\Roger\Desktop\Roger\handmade\RogerHandmade\code\win32_handmade.cpp user32.lib gdi32.lib
REM cl /Zi ..\code\win32_handmade.cpp
REM -FC to show full paths
REM cl -F4194304 ... requests for a 4MB stack size
popd
