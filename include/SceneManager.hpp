#pragma once
#include <memory>

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_log.h>

#include "GPUResources.hpp"

class SceneManager {
public:
	SceneManager(std::shared_ptr<SDL_Window> window, std::shared_ptr<SDL_GPUDevice> gpu)
	: m_window(window),
	  m_gpu(gpu),
	  m_pipeline(nullptr) {
		// load shaders
		std::unique_ptr<GPUResource<SHADER>> v_shader { loadShader("RawTriangle.vert", 0, 0, 0, 0) };
		std::unique_ptr<GPUResource<SHADER>> f_shader { loadShader("SolidColor.frag", 0, 0, 0, 0) };
		// create pipeline
		const SDL_GPUColorTargetDescription TARGET {
			.format = SDL_GetGPUSwapchainTextureFormat(m_gpu.get(), m_window.get()),
		};
		const GPUResourceTraits<GRAPHICS_PIPELINE>::Info PIPELINE_INFO {
			.vertex_shader = v_shader->getResource(),
			.fragment_shader = f_shader->getResource(),
			.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
			.rasterizer_state = {
				.fill_mode = SDL_GPU_FILLMODE_FILL
			},
			.target_info = {
				.color_target_descriptions = &TARGET,
				.num_color_targets = 1,
			}
		};
		m_pipeline.reset(new GPUResource<GRAPHICS_PIPELINE>(m_gpu, PIPELINE_INFO));
	}
	~SceneManager() {}
	SDL_AppResult render() const noexcept {
		SDL_GPUCommandBuffer* cmdbuf { SDL_AcquireGPUCommandBuffer(m_gpu.get()) };
		if (!cmdbuf) {
			return SDL_APP_FAILURE;
		}
		SDL_GPUTexture* swapchain_texture;
		if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmdbuf, m_window.get(), &swapchain_texture, NULL, NULL)) {
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
		SDL_BindGPUGraphicsPipeline(render_pass, m_pipeline->getResource());
		SDL_DrawGPUPrimitives(render_pass, 3, 1, 0, 0);
		SDL_EndGPURenderPass(render_pass);
		SDL_SubmitGPUCommandBuffer(cmdbuf);
		return SDL_APP_CONTINUE;
	}

private:
	std::shared_ptr<SDL_Window> m_window;
	std::shared_ptr<SDL_GPUDevice> m_gpu;
	std::unique_ptr<GPUResource<GRAPHICS_PIPELINE>> m_pipeline;
	GPUResource<SHADER>* loadShader(const std::string &filename, const Uint32 &num_samplers, const Uint32 &num_storage_textures, const Uint32 &num_storage_buffers, const Uint32 &num_uniform_buffers) {
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
};
