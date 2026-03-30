#pragma once
#include <memory>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_log.h>

enum RESOURCE_TYPES {
	TEXTURE,
	SAMPLER,
	BUFFER,
	TRANSFER_BUFFER,
	SHADER,
	GRAPHICS_PIPELINE
};

// define create & release function for 
// objects that must be released
// using a SDL_GPUDevice handle
template<RESOURCE_TYPES> struct GPUResourceTraits { };
template<> struct GPUResourceTraits<TEXTURE> {
	using Info = SDL_GPUTextureCreateInfo;
	using Type = SDL_GPUTexture;
	static constexpr auto description = "Texture";
	static constexpr auto create = SDL_CreateGPUTexture;
	static constexpr auto release = SDL_ReleaseGPUTexture;
};
template<> struct GPUResourceTraits<SAMPLER> {
	using Info = SDL_GPUSamplerCreateInfo;
	using Type = SDL_GPUSampler;
	static constexpr auto description = "Sampler";
	static constexpr auto create = SDL_CreateGPUSampler;
	static constexpr auto release = SDL_ReleaseGPUSampler;
};
template<> struct GPUResourceTraits<BUFFER> {
	using Info = SDL_GPUBufferCreateInfo;
	using Type = SDL_GPUBuffer;
	static constexpr auto description = "Buffer";
	static constexpr auto create = SDL_CreateGPUBuffer;
	static constexpr auto release = SDL_ReleaseGPUBuffer;
};
template<> struct GPUResourceTraits<SHADER> {
	using Info = SDL_GPUShaderCreateInfo;
	using Type = SDL_GPUShader;
	static constexpr auto description = "Shader";
	static constexpr auto create = SDL_CreateGPUShader;
	static constexpr auto release = SDL_ReleaseGPUShader;
};
template<> struct GPUResourceTraits<GRAPHICS_PIPELINE> {
	using Info = SDL_GPUGraphicsPipelineCreateInfo;
	using Type = SDL_GPUGraphicsPipeline;
	static constexpr auto description = "Graphics Pipeline";
	static constexpr auto create = SDL_CreateGPUGraphicsPipeline;
	static constexpr auto release = SDL_ReleaseGPUGraphicsPipeline;
};
template<> struct GPUResourceTraits<TRANSFER_BUFFER> {
	using Info = SDL_GPUTransferBufferCreateInfo;
	using Type = SDL_GPUTransferBuffer;
	static constexpr auto description = "Transfer Buffer";
	static constexpr auto create = SDL_CreateGPUTransferBuffer;
	static constexpr auto release = SDL_ReleaseGPUTransferBuffer;
};

// A GPUResource will automatically get the create & release function from GPUResourceTraits
template<RESOURCE_TYPES TYPE> class GPUResource {
public:
	GPUResource(std::weak_ptr<SDL_GPUDevice> gpu, const GPUResourceTraits<TYPE>::Info &info)
	: m_gpu(gpu){
		if (m_gpu.expired()) {
			throw std::runtime_error("GPUDevice handle expired");
		}
		m_resource = GPUResourceTraits<TYPE>::create(m_gpu.lock().get(), &info);
		if (!m_resource) {
			throw std::runtime_error(SDL_GetError());
		}
		// uncomment below if memory management is unclear
		// SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Created %s", GPUResourceTraits<TYPE>::description);
	}
	~GPUResource() {
		if (m_gpu.expired()) {
			SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Tried to release %s, but the GPUDevice handle expired", GPUResourceTraits<TYPE>::description);
			return;
		} else if (!m_resource) {
			SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Tried to release %s, but there is no resource to release, did you create it?", GPUResourceTraits<TYPE>::description);
			return;
		}
		GPUResourceTraits<TYPE>::release(m_gpu.lock().get(), m_resource); 
		// uncomment below if memory management is unclear
		// SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Released %s", GPUResourceTraits<TYPE>::description);
	}
	GPUResourceTraits<TYPE>::Type* getResource() const { return m_resource; }
	GPUResource(const GPUResource &other) = delete;
	GPUResource(const GPUResource &&other) = delete;
	GPUResource<TYPE>& operator=(const GPUResource<TYPE>&) = delete;
	GPUResource<TYPE>& operator=(const GPUResource<TYPE>&&) = delete;
private:
	GPUResourceTraits<TYPE>::Type* m_resource;
	std::weak_ptr<SDL_GPUDevice> m_gpu;
};
