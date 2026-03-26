#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_log.h>

#include <memory>

struct SDL_Deleter {
	void operator()(SDL_Window* window) {
		SDL_DestroyWindow(window);
	}
	void operator()(SDL_GPUDevice* gpu) {
		SDL_DestroyGPUDevice(gpu);
	}
};

namespace {
	std::unique_ptr<SDL_Window, SDL_Deleter> g_Window { nullptr };
	std::unique_ptr<SDL_GPUDevice, SDL_Deleter> g_Gpu { nullptr };
};

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		return SDL_APP_FAILURE;
	}
	const SDL_WindowFlags WINDOW_FLAGS {
		SDL_WINDOW_MAXIMIZED |
		SDL_WINDOW_RESIZABLE
	};
	g_Window.reset(SDL_CreateWindow("Enhanced", 0, 0, WINDOW_FLAGS));
	if (!g_Window) {
		return SDL_APP_FAILURE;
	}
	const SDL_GPUShaderFormat SHADER_FORMATS {
		SDL_GPU_SHADERFORMAT_SPIRV |
		SDL_GPU_SHADERFORMAT_DXIL
	};
	g_Gpu.reset(SDL_CreateGPUDevice(SHADER_FORMATS, true, NULL));
	if (!g_Gpu) {
		return SDL_APP_FAILURE;
	}
	if (!SDL_ClaimWindowForGPUDevice(g_Gpu.get(), g_Window.get())) {
		return SDL_APP_FAILURE;
	}
	// TODO:
	// create vertex buffer

	// TODO:
	// create gpu graphics pipeline
	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
	SDL_GPUCommandBuffer* cmdbuf { SDL_AcquireGPUCommandBuffer(g_Gpu.get()) };
	if (!cmdbuf) {
		return SDL_APP_FAILURE;
	}
	SDL_GPUTexture* swapchain_texture;
	if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmdbuf, g_Window.get(), &swapchain_texture, NULL, NULL)) {
		return SDL_APP_FAILURE;
	}
	// texture may still be NULL,
	// but it's not an err,
	// refer to SDL_WaitAndAcquireGPUSwapchainTexture docs for more info
	if (!swapchain_texture) {
		return SDL_APP_CONTINUE;
	}
	const SDL_GPUColorTargetInfo target_info {
		.texture = swapchain_texture,
		.clear_color = {0.0f, 0.0f, 0.0f, 1.0f},
		.load_op = SDL_GPU_LOADOP_CLEAR,
		.store_op = SDL_GPU_STOREOP_STORE
	};
	SDL_GPURenderPass* render_pass { SDL_BeginGPURenderPass(cmdbuf, &target_info, 1, NULL) };
	// TODO:
	// SDL_DrawGPUPrimitives
	SDL_EndGPURenderPass(render_pass);
	SDL_SubmitGPUCommandBuffer(cmdbuf);
	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
	switch(event->type) {
	case SDL_EVENT_KEY_DOWN:
		switch(event->key.key) {
			// KEYBOARD EVENTS
		}
		break;
	case SDL_EVENT_QUIT:
		return SDL_APP_SUCCESS;
		break;
	}
	return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
	switch(result) {
	case SDL_APP_FAILURE: // program failed
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "\t%s\nProgram exiting, performing cleanup", SDL_GetError());
		break;
	case SDL_APP_SUCCESS: // user asked to exit
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Program exiting, performing cleanup...");
		break;
	case SDL_APP_CONTINUE: // will never occur
		break;
	}
	// check for resources to release
	if (g_Gpu) {
		g_Gpu.release();
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Released SDL_GPUDevice");
	}
	if (g_Window) {
		g_Window.release();
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Released SDL_Window");
	}
	SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Program exited.");
}
