@echo off

mkdir c:\Users\chris\work\handmade\build
pushd c:\Users\chris\work\handmade\build
cl -Zi -FC ..\code\main.cpp -o handmade User32.lib Gdi32.lib
popd
