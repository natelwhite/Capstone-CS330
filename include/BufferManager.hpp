#pragma once

#include <memory>

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_assert.h>
#include <SDL3/SDL_log.h>

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/math.hpp>
#include <fastgltf/types.hpp>

#include "GPUResources.hpp"
#include "Camera.hpp"

class BufferManager {
public:
	/*
	 * Initializes resources necessary to render the geometry stored
	 * in a GLTF file using SDL.
	 *
	 * @param gpu The GPU interface used for storing buffers
	 * @param filename The GLTF file to use for mesh data
	 */
	BufferManager(std::weak_ptr<SDL_GPUDevice> gpu, std::string filename);
	~BufferManager() { }
	/*
	 * Sort objects by distance from the camera
	 *
	 * Call this before the render pass
	 * @param camera Sort using this camera's position
	 */
	void sortObjects(const Camera &camera) noexcept;
	/*
	 * Render geometry stored in buffers
	 *
	 * @param cmdbuf A pointer to a command buffer that has already been acquired.
	 * @param render_pass A pointer to a render pass that has already begun.
	 * @param camera Render from this camera's perspective.
	 */
	void renderGeometry(SDL_GPUCommandBuffer* cmdbuf, SDL_GPURenderPass* render_pass, const Camera &camera) const noexcept;
	/*
	 * Get a description of the internal buffer
	 */
	SDL_GPUVertexBufferDescription getBufferDescription() const noexcept { return BUF_DESCRIPTION; }
	/*
	 * Get the attributes of each vertex in the internal buffer
	 */
	std::vector<SDL_GPUVertexAttribute> getVertexAttributes() const noexcept { return m_vertex_attributes; }

private:
	std::weak_ptr<SDL_GPUDevice> m_gpu;
	std::unique_ptr<GPUResource<BUFFER>> m_v_buf; // vertex buffer
	std::unique_ptr<GPUResource<BUFFER>> m_i_buf; // index buffer
	struct Mesh {
		fastgltf::TRS transform;
		Uint32 buffer_start, buffer_count;
		// Evaluate this mesh's model matrix
		fastgltf::math::fmat4x4 model() const;
	};
	std::vector<Mesh> m_objects;
	// Structures that will be used for rendering data in a GLTF file
	struct Vertex {
		fastgltf::math::fvec3 pos, norm;
	};
	struct VertexUniforms {
		fastgltf::math::fmat4x4 camera_projection_view;
		fastgltf::math::fmat4x4 mesh_model;
	};
	struct FragmentUniforms {
		fastgltf::math::fvec2 near_far;
		fastgltf::math::fvec3 camera_pos;
	};
	// Structure of the vertex buffer
	const SDL_GPUVertexBufferDescription BUF_DESCRIPTION {
		.slot = 0,
		.pitch = sizeof(Vertex),
		.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
		.instance_step_rate = 0
	};
	std::vector<SDL_GPUVertexAttribute> m_vertex_attributes { {
		.location = 0,
		.buffer_slot = 0,
		.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
		.offset = 0
	}, {
		.location = 1,
		.buffer_slot = 0,
		.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
		.offset = sizeof(Vertex::pos)
	} };
};
