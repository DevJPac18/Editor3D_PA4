@echo off
REM Compila el Editor 3D con MinGW (g++). Uso: build.bat  [debug]
REM -fno-ipa-cp: evita un error de GCC 6.x/MinGW 32 bits (clones IPA-CP de metodos thiscall corrompen la pila)
setlocal
cd /d "%~dp0"
set FLAGS=-std=gnu++14 -O2 -fno-ipa-cp -Wall -Wno-unused-parameter
if /I "%1"=="debug" set FLAGS=-std=gnu++14 -g -O0 -Wall -Wno-unused-parameter
set DEFS=-D_WIN32_WINNT=0x0601 -DWINVER=0x0601
set SRC=src/main.cpp src/core/Utils.cpp src/scene/SceneNode.cpp src/scene/Scene.cpp src/scene/SceneFactory.cpp ^
 src/anim/Animation.cpp src/anim/Animator.cpp src/render/GLExt.cpp src/render/Mesh.cpp src/render/Texture.cpp ^
 src/render/TextureGen.cpp src/render/Shader.cpp src/render/Camera.cpp src/render/Renderer.cpp ^
 src/io/ObjLoader.cpp src/io/SceneIO.cpp src/ui/Gui.cpp src/platform/FileDialog.cpp ^
 src/editor/Gizmo.cpp src/editor/Editor.cpp src/editor/EditorPanels.cpp
if not exist bin mkdir bin
echo Compilando...
g++ %FLAGS% %DEFS% -Isrc %SRC% -o bin/Editor3D.exe -static-libgcc -static-libstdc++ -mwindows -lopengl32 -lglu32 -lgdi32 -lcomdlg32 -lshell32
if errorlevel 1 (
  echo.
  echo *** Error de compilacion ***
  exit /b 1
)
echo OK: bin\Editor3D.exe
endlocal
