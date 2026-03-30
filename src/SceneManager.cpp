#include "SceneManager.hpp"

SceneManager::SceneManager(std::shared_ptr<SDL_Window> window, std::shared_ptr<SDL_GPUDevice> gpu) : m_window(window), m_gpu(gpu) {
		// Construct scene geometry
		try {
			m_buffer_man.reset(new BufferManager(m_gpu, "cubes"));
		} catch (const std::exception &e) {
			SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "%s", e.what());
			return;
		}
		// load shaders
		std::unique_ptr<GPUResource<SHADER>> v_shader { loadShader("PositionNormal.vert", 0, 0, 0, 1) };
		std::unique_ptr<GPUResource<SHADER>> f_shader { loadShader("BlinnPhong.frag", 0, 0, 0, 1) };
		// create pipeline
		const SDL_GPUVertexBufferDescription BUF_DESCRIPTION { m_buffer_man->getBufferDescription() };
		const std::vector<SDL_GPUVertexAttribute> VERT_ATTRIBS { m_buffer_man->getVertexAttributes() };
		const SDL_GPUColorTargetDescription TARGET {
			.format = SDL_GetGPUSwapchainTextureFormat(m_gpu.get(), m_window.get()),
			.blend_state = {
				.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
				.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
				.color_blend_op = SDL_GPU_BLENDOP_ADD,
				.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
				.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
				.alpha_blend_op = SDL_GPU_BLENDOP_ADD,
				.enable_blend = true,
			}
		};
		const GPUResourceTraits<GRAPHICS_PIPELINE>::Info PIPELINE_INFO {
			.vertex_shader = v_shader->getResource(),
			.fragment_shader = f_shader->getResource(),
			.vertex_input_state = {
				.vertex_buffer_descriptions = &BUF_DESCRIPTION,
				.num_vertex_buffers = 1,
				.vertex_attributes = VERT_ATTRIBS.data(),
				.num_vertex_attributes = static_cast<Uint32>(VERT_ATTRIBS.size()),
			},
			.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
			.rasterizer_state = {
				.fill_mode = SDL_GPU_FILLMODE_FILL,
				.cull_mode = SDL_GPU_CULLMODE_FRONT,
				.front_face = SDL_GPU_FRONTFACE_CLOCKWISE
			},
			.target_info = {
				.color_target_descriptions = &TARGET,
				.num_color_targets = 1,
			}
		};
		m_pipeline.reset(new GPUResource<GRAPHICS_PIPELINE>(m_gpu, PIPELINE_INFO));
}

SDL_AppResult SceneManager::iterate() noexcept {
	if (m_is_focused) {
		m_camera.iterate();
		m_buffer_man->sortObjects(m_camera);
		SDL_GPUCommandBuffer* cmdbuf { SDL_AcquireGPUCommandBuffer(m_gpu.get()) };
		if (!cmdbuf) {
			return SDL_APP_FAILURE;
		}
		SDL_GPUTexture* swapchain_texture;
		if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmdbuf, m_window.get(), &swapchain_texture, NULL, NULL)) {
			return SDL_APP_FAILURE;
		}
		// Texture may still be NULL,
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
		SDL_BindGPUGraphicsPipeline(render_pass, m_pipeline->getResource());
		m_buffer_man->renderGeometry(cmdbuf, render_pass, m_camera);
		SDL_EndGPURenderPass(render_pass);
		SDL_SubmitGPUCommandBuffer(cmdbuf);
	}
	return SDL_APP_CONTINUE;
}

SDL_AppResult SceneManager::event(const SDL_Event &e) noexcept {
	switch(e.type) {
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
		if (!m_is_focused) {
			m_is_focused = true;
			if (!SDL_SetWindowRelativeMouseMode(m_window.get(), m_is_focused)) {
				return SDL_APP_FAILURE;
			}
		}
		break;
	case SDL_EVENT_KEY_DOWN:
		switch(e.key.key) {
		case SDLK_ESCAPE:
			if (m_is_focused) {
				m_is_focused = false;
				if (!SDL_SetWindowRelativeMouseMode(m_window.get(), m_is_focused)) {
					return SDL_APP_FAILURE;
				}
			}
		}
		break;
	}
	// Class member has event handler,
	// only call it if the scene is in focus
	if (m_is_focused) {
		m_camera.event(e);
	}
	return SDL_APP_CONTINUE;
}

GPUResource<SHADER>* SceneManager::loadShader(const std::string &filename, const Uint32 &num_samplers, const Uint32 &num_storage_textures, const Uint32 &num_storage_buffers, const Uint32 &num_uniform_buffers) {
	// Auto-detect the shader stage from filename
	SDL_GPUShaderStage stage;
	if (filename.contains(".vert")) {
		stage = SDL_GPU_SHADERSTAGE_VERTEX;
	} else if (filename.contains(".frag")) {
		stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
	} else {
		throw std::runtime_error("Could not detect shader stage");
	}
	// determine shader format
	const SDL_GPUShaderFormat host_formats = SDL_GetGPUShaderFormats(m_gpu.get());
	SDL_GPUShaderFormat format = SDL_GPU_SHADERFORMAT_INVALID;
	std::string entrypoint;
	std::string bin_dir;
	std::string file_extension;
	if (host_formats & SDL_GPU_SHADERFORMAT_SPIRV) {
		format = SDL_GPU_SHADERFORMAT_SPIRV;
		bin_dir = "shaders/bin/SPIRV/";
		file_extension = ".spv";
		entrypoint = "main";
	} else if (host_formats & SDL_GPU_SHADERFORMAT_MSL) {
		format = SDL_GPU_SHADERFORMAT_MSL;
		bin_dir = "shaders/bin/MSL/";
		file_extension = ".msl";
		entrypoint = "main0";
	} else if (host_formats & SDL_GPU_SHADERFORMAT_DXIL) {
		format = SDL_GPU_SHADERFORMAT_DXIL;
		bin_dir = "shaders/bin/DXIL/";
		file_extension = ".dxil";
		entrypoint = "main";
	} else {
		throw std::runtime_error("Host shader format is not supported");
	}
	// load data from file
	const std::string full_path { SDL_GetBasePath() + bin_dir + filename + file_extension };
	size_t code_size;
	void* code = SDL_LoadFile(full_path.data(), &code_size);
	if (!code) {
		throw std::runtime_error("Failed to load shader from disk!");
	}
	// create shader
	const GPUResourceTraits<SHADER>::Info SHADER_INFO {
		.code_size = code_size,
		.code = static_cast<Uint8*>(code),
		.entrypoint = entrypoint.data(),
		.format = format,
		.stage = stage,
		.num_samplers = num_samplers,
		.num_storage_textures = num_storage_textures,
		.num_storage_buffers = num_storage_buffers,
		.num_uniform_buffers = num_uniform_buffers
	};
	GPUResource<SHADER>* result {new GPUResource<SHADER>(m_gpu, SHADER_INFO)};
	SDL_free(code);
	return result;
};
