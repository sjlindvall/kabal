:: 1) Generate icon files referenced by solitaire.rc
python tools\gen_icons.py --out icons

:: 2) Compile resources (.rc -> .res)
rc /nologo /fo solitaire.res solitaire.rc

:: 3) Compile + link Win32 app
cl /nologo /TC /std:c11 /W4 /D_CRT_SECURE_NO_WARNINGS ^
  main.c solitaire.res ^
  /link /SUBSYSTEM:WINDOWS /OUT:kabal-solitaire.exe user32.lib gdi32.lib comctl32.lib
