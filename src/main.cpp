#include "SDL3/SDL_keycode.h"
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_log.h>

#include <memory>

#include "SceneManager.hpp"

// Globals for application core resources
namespace {
	std::shared_ptr<SDL_Window> g_Window { nullptr };
	std::shared_ptr<SDL_GPUDevice> g_Gpu { nullptr };
	std::unique_ptr<SceneManager> g_SceneMan { nullptr };
};

// This application uses SDL callback functions to implement the init, iterate, event, and quit logic.
// See https://wiki.libsdl.org/SDL3/README-main-functions for more info
SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
	// Init SDL
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		return SDL_APP_FAILURE;
	}
	// Create window
	const SDL_WindowFlags WINDOW_FLAGS {
		SDL_WINDOW_RESIZABLE
	};
	g_Window.reset(SDL_CreateWindow("Enhanced", 800, 600, WINDOW_FLAGS), SDL_DestroyWindow);
	if (!g_Window) {
		return SDL_APP_FAILURE;
	}
	// Grab mouse focus
	if (!SDL_SetWindowRelativeMouseMode(g_Window.get(), true)) {
		return SDL_APP_FAILURE;
	}
	// Create GPU interface
	const SDL_GPUShaderFormat SHADER_FORMATS {
		SDL_GPU_SHADERFORMAT_SPIRV |
		SDL_GPU_SHADERFORMAT_DXIL
	};
	g_Gpu.reset(SDL_CreateGPUDevice(SHADER_FORMATS, true, NULL), SDL_DestroyGPUDevice);
	if (!g_Gpu) {
		return SDL_APP_FAILURE;
	}
	if (!SDL_ClaimWindowForGPUDevice(g_Gpu.get(), g_Window.get())) {
		return SDL_APP_FAILURE;
	}
	try {
		g_SceneMan.reset(new SceneManager(g_Window, g_Gpu));
	} catch (const std::exception &e) {
		return SDL_APP_FAILURE;
	}
	return SDL_APP_CONTINUE;
}
SDL_AppResult SDL_AppIterate(void* appstate) {
	return g_SceneMan->iterate();
}
SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
	switch(event->type) {
	case SDL_EVENT_QUIT:
		return SDL_APP_SUCCESS;
		break;
	}
	return g_SceneMan->event(*event);
}
void SDL_AppQuit(void* appstate, SDL_AppResult result) {
	switch(result) {
	case SDL_APP_FAILURE: // program failed
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "\t%s\n\tProgram exiting, performing cleanup", SDL_GetError());
		break;
	case SDL_APP_SUCCESS: // user asked to exit
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Program exiting, performing cleanup...");
		break;
	case SDL_APP_CONTINUE: // will never occur
		break;
	}
	// check for resources to release
	g_SceneMan.reset();
	SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Released all SceneManager references");
	g_Gpu.reset();
	if (g_Gpu.use_count() == 0) {
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Released all SDL_GPUDevice references");
	} else {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Program leaked %ld SDL_GPUDevice references", g_Window.use_count());
	}
	g_Window.reset();
	if (g_Window.use_count() == 0) {
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Released all SDL_Window references");
	} else {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Program leaked %ld SDL_Window references", g_Window.use_count());
	}
	SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Program exited.");
}
