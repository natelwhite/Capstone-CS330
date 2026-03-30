### Install Dependencies
- Windows: [SDL3 Dependencies](https://wiki.libsdl.org/SDL3/README-windows)
- Linux: [SDL3 Dependencies](https://wiki.libsdl.org/SDL3/README-linux)
- macOS: [SDL3 Dependencies](https://wiki.libsdl.org/SDL3/README-macos)
### Clone, Build, & Run
- Clone: `git clone https://github.com/Capstone-CS330.git --recurse-submodules`
- Move to project root directory: `cd Capstone-CS330`
- Generate build using CMake: `cmake -Bbuild`
- Build project: `cmake --build build`
- Compile shaders (optional):
	- Install [shadercross](https://github.com/libSDL-org/SDL_shadercross)
	- Move to HLSL source directory: `cd build/shaders/source`
	- Compile HLSL to binaries: `./compile.sh`
		- You'll likely have to give permission to execute as an application.
	- Move back to project root directory: `cd ../../..`
- Run project: `./build/Enhanced`
