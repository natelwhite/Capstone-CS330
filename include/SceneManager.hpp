#pragma once
#include <memory>
#include <SDL3/SDL.h>
#include "BufferManager.hpp"
#include "Camera.hpp"
#include "GPUResources.hpp"

class SceneManager {
public:
	/*
	 * Initializes resources necessary to render 3D geometry
	 *
	 * @param window The window to render the scene to
	 * @param gpu The GPU interface used for computing shaders
	 */
	SceneManager(std::shared_ptr<SDL_Window> window, std::shared_ptr<SDL_GPUDevice> gpu);
	~SceneManager() {}
	
	/*
	 * SDL iterate handler
	 *
	 * The scene only iterates if it is in focus
	 */
	SDL_AppResult iterate() noexcept;
	/*
	 * SDL event handler
	 *
	 * @param e An SDL_Event that the scene should react to
	 * Relative mouse mode is on by default.
	 * If the user presses the escape key,
	 * relative mouse mode is turned off.
	 * If the user clicks on the window while relative mouse mode is off,
	 * relative mouse mode is turned on.
	 */
	SDL_AppResult event(const SDL_Event &e) noexcept;
private:
	std::shared_ptr<SDL_Window> m_window;
	std::shared_ptr<SDL_GPUDevice> m_gpu;
	std::unique_ptr<BufferManager> m_buffer_man { nullptr };
	std::unique_ptr<GPUResource<GRAPHICS_PIPELINE>> m_pipeline { nullptr };
	Camera m_camera { };
	bool m_is_focused { true };
	GPUResource<SHADER>* loadShader(const std::string &filename, const Uint32 &num_samplers, const Uint32 &num_storage_textures, const Uint32 &num_storage_buffers, const Uint32 &num_uniform_buffers);
};
