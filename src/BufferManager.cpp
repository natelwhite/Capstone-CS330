#include <filesystem>
#include <stdexcept>
#include <SDL3/SDL_log.h>
#include <fastgltf/types.hpp>
#include <string>
#include "BufferManager.hpp"

BufferManager::BufferManager(std::weak_ptr<SDL_GPUDevice> gpu, std::string filename) : m_gpu(gpu) {
	// Load fastgltf asset
	fastgltf::Parser parser;
	const std::string file_extension { ".glb" };
	const std::string mesh_dir { "meshes/" };
	std::filesystem::path path { SDL_GetBasePath() + mesh_dir + filename + file_extension };
	SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Loading GLTF file: %s", path.c_str());
	fastgltf::Expected<fastgltf::GltfDataBuffer> data = fastgltf::GltfDataBuffer::FromPath(path);
	if (data.error() != fastgltf::Error::None) {
		switch(data.error()) {
		case fastgltf::Error::InvalidPath:
			SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "%s", path.c_str());
			throw std::runtime_error("Invalid GLTF file path");
			break;
		case fastgltf::Error::InvalidFileData:
		case fastgltf::Error::InvalidGLB:
		case fastgltf::Error::InvalidGltf:
			throw std::runtime_error("Invalid GLTF data");
			break;
		default:
			throw std::runtime_error("Error occured while parsing GLTF file");
			break;
		}
	}
	fastgltf::Expected<fastgltf::Asset> asset { parser.loadGltf(
			data.get(),
			path.parent_path(),
			fastgltf::Options::DecomposeNodeMatrices |
			fastgltf::Options::LoadExternalBuffers |
			fastgltf::Options::GenerateMeshIndices
	) };
	if (asset.error() != fastgltf::Error::None) {
		throw std::runtime_error("Error occured while loading GLTF file");
	}

	// Fill m_materials with material data
	for (const fastgltf::Material &mat : asset->materials) {
		const fastgltf::PBRData &pbr { mat.pbrData };
		m_materials.emplace_back(pbr.baseColorFactor, pbr.roughnessFactor, pbr.metallicFactor);
	}

	// Fill m_objects with mesh data
	auto loadMeshes = [&](fastgltf::Node &node, fastgltf::math::fmat4x4 TRS) -> void {
		// decompose TRS matrix
		if (const auto TRS (std::get_if<fastgltf::TRS>(&node.transform)); TRS) {
			SDL_assert(node.meshIndex.has_value());
			const fastgltf::Mesh &mesh { asset->meshes.at(node.meshIndex.value()) };
			Uint32 prim_i_start { };
			std::vector<Primitive> primitives;
			for (const fastgltf::Primitive &prim : mesh.primitives) {
				SDL_assert(prim.indicesAccessor.has_value());
				fastgltf::Accessor &index_access {
					asset->accessors.at(prim.indicesAccessor.value())
				};
				primitives.emplace_back(prim_i_start, index_access.count, prim.materialIndex);
				prim_i_start += index_access.count;
			}
			m_objects.emplace_back(*TRS, primitives);
		}
	};
	fastgltf::iterateSceneNodes(asset.get(), asset->defaultScene.value(), fastgltf::math::fmat4x4(), loadMeshes);

	// Initialize buffers with size
	// necessary to hold the asset's geometry
	Uint32 scene_i_bytes { }, scene_v_bytes { };
	auto prepareBuffers = [&](fastgltf::Node &node, fastgltf::math::fmat4x4 TRS) -> void {
		SDL_assert(node.meshIndex.has_value());
		const fastgltf::Mesh &mesh { asset->meshes.at(node.meshIndex.value()) };
		for (const fastgltf::Primitive &prim : mesh.primitives) {
			// primitive index buffer byte length
			SDL_assert(prim.indicesAccessor.has_value());
			fastgltf::Accessor &index_access {
				asset->accessors.at(prim.indicesAccessor.value())
			};
			SDL_assert(index_access.bufferViewIndex.has_value());
			const size_t prim_i_bytes {
				asset->bufferViews.at(index_access.bufferViewIndex.value()).byteLength
			};
			// Find byte length for each vertex attribute
			Uint32 prim_pos_bytes { }, prim_norm_bytes { };
			for (const fastgltf::Attribute &attrib : prim.attributes) {
				fastgltf::Accessor &access { asset->accessors.at(attrib.accessorIndex) };
				SDL_assert(access.bufferViewIndex.has_value());
				if (attrib.name == "POSITION") {
					prim_pos_bytes = asset->bufferViews.at(access.bufferViewIndex.value()).byteLength;
				} else if (attrib.name == "NORMAL") {
					prim_norm_bytes = asset->bufferViews.at(access.bufferViewIndex.value()).byteLength;
				}
			}
			// Vertex attributes stored in a single buffer,
			// sum them
			const Uint32 prim_v_bytes { prim_pos_bytes + prim_norm_bytes };
			// Sum primitive byte lengths with scene byte lengths
			scene_i_bytes += prim_i_bytes;
			scene_v_bytes += prim_v_bytes;
		}
	};
	fastgltf::iterateSceneNodes(asset.get(), asset->defaultScene.value(), fastgltf::math::fmat4x4(), prepareBuffers);
	const GPUResourceTraits<BUFFER>::Info i_buf_info {
		.usage = SDL_GPU_BUFFERUSAGE_INDEX,
		.size = static_cast<Uint32>(scene_i_bytes)
	};
	const GPUResourceTraits<BUFFER>::Info v_buf_info {
		.usage = SDL_GPU_BUFFERUSAGE_VERTEX,
		.size = static_cast<Uint32>(scene_v_bytes)
	};
	m_i_buf.reset(new GPUResource<BUFFER>(m_gpu, i_buf_info));
	m_v_buf.reset(new GPUResource<BUFFER>(m_gpu, v_buf_info));

	// Fill buffers with geometry data
	// stored in asset
	SDL_GPUCommandBuffer* cmdbuf { SDL_AcquireGPUCommandBuffer(m_gpu.lock().get()) };
	SDL_GPUCopyPass* copy_pass { SDL_BeginGPUCopyPass(cmdbuf) };
	Uint32 i_offset { }, v_offset { };
	Uint32 mesh_i_offset { };
	auto fillBuffers = [&](fastgltf::Node &node, fastgltf::math::fmat4x4 TRS) -> void {
		SDL_assert(node.meshIndex.has_value());
		const fastgltf::Mesh &mesh { asset->meshes.at(node.meshIndex.value()) };
		Uint32 prim_i_offset { };
		for (const fastgltf::Primitive &prim : mesh.primitives) {
			// primitive index buffer
			SDL_assert(prim.indicesAccessor.has_value());
			fastgltf::Accessor &index_access {
				asset->accessors.at(prim.indicesAccessor.value())
			};
			SDL_assert(index_access.bufferViewIndex.has_value());
			const Uint32 prim_i_bytes { static_cast<Uint32>(
				asset->bufferViews.at(index_access.bufferViewIndex.value()).byteLength
			) };
			// Find byte length for each attribute buffer
			// and move attribute data to the appropriate vector
			Uint32 prim_pos_bytes { }, prim_norm_bytes { };
			std::vector<fastgltf::math::fvec3> pos_data, norm_data;
			for (const fastgltf::Attribute &attrib : prim.attributes) {
				const fastgltf::Accessor &access { asset->accessors.at(attrib.accessorIndex) };
				SDL_assert(access.bufferViewIndex.has_value());
				if (attrib.name == "POSITION") {
					prim_pos_bytes = asset->bufferViews.at(access.bufferViewIndex.value()).byteLength;
					pos_data.resize(access.count);
					fastgltf::copyFromAccessor<fastgltf::math::fvec3>(asset.get(), access, pos_data.data());
				} else if (attrib.name == "NORMAL") {
					prim_norm_bytes = asset->bufferViews.at(access.bufferViewIndex.value()).byteLength;
					norm_data.resize(access.count);
					fastgltf::copyFromAccessor<fastgltf::math::fvec3>(
						asset.get(), access, norm_data.data()
					);
				}
			}
			// Vertex attributes stored in a single buffer,
			// sum them
			const Uint32 prim_v_bytes { prim_pos_bytes + prim_norm_bytes };
			// Move all data to transfer buffer,
			// keeping track of byte offsets
			GPUResourceTraits<TRANSFER_BUFFER>::Info trans_buf_info {
				.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
				.size = prim_i_bytes + prim_v_bytes
			};
			GPUResource<TRANSFER_BUFFER> trans_buf { m_gpu, trans_buf_info };
			// Move index data to transfer buffer
			Uint16* i_data { static_cast<Uint16*>(
				SDL_MapGPUTransferBuffer(m_gpu.lock().get(), trans_buf.getResource(), false)
			) };
			fastgltf::copyFromAccessor<Uint16>(asset.get(), index_access, i_data);
			for (size_t i = 0; i < index_access.count; ++i) {
				i_data[i] += prim_i_offset + mesh_i_offset;
			}
			prim_i_offset += pos_data.size();
			// Move Vertices to transfer buffer
			Vertex* v_data { static_cast<Vertex*>(
				reinterpret_cast<Vertex*>(i_data + index_access.count)
			) };
			for (size_t i = 0; i < pos_data.size(); ++i) {
				v_data[i] = Vertex(pos_data.at(i), norm_data.at(i));
			}
			SDL_UnmapGPUTransferBuffer(m_gpu.lock().get(), trans_buf.getResource());
			// Upload transfer buffer to GPU
			const SDL_GPUTransferBufferLocation i_trans_buf_loc {
				.transfer_buffer = trans_buf.getResource(),
				.offset = 0
			};
			const SDL_GPUTransferBufferLocation v_trans_buf_loc {
				.transfer_buffer = trans_buf.getResource(),
				.offset = prim_i_bytes
			};
			const SDL_GPUBufferRegion i_buf_reg {
				.buffer = m_i_buf->getResource(),
				.offset = i_offset,
				.size = prim_i_bytes,
			};
			const SDL_GPUBufferRegion v_buf_reg {
				.buffer = m_v_buf->getResource(),
				.offset = v_offset,
				.size = prim_v_bytes,
			};
			i_offset += prim_i_bytes;
			v_offset += prim_v_bytes;
			SDL_UploadToGPUBuffer(copy_pass, &i_trans_buf_loc, &i_buf_reg, false);
			SDL_UploadToGPUBuffer(copy_pass, &v_trans_buf_loc, &v_buf_reg, false);
		}
		mesh_i_offset += prim_i_offset;
	};
	fastgltf::iterateSceneNodes(asset.get(), asset->defaultScene.value(), fastgltf::math::fmat4x4(), fillBuffers);
	SDL_EndGPUCopyPass(copy_pass);
	SDL_SubmitGPUCommandBuffer(cmdbuf);
}

void BufferManager::sortObjects(const Camera &camera) noexcept {
	const auto square = [](const double &val) {
		return val * val;
	};
	const auto distance = [square](const fastgltf::math::fvec3 &a, const fastgltf::math::fvec3 &b) {
		return sqrt(square(static_cast<double>(a.x() - b.x()))
			+square(static_cast<double>(a.y() - b.y()))
			+square(static_cast<double>(a.z() - b.z()))
		);
	};
	const auto closest = [camera, distance](const Mesh &a, const Mesh &b) {
		const fastgltf::math::fvec3 cam_pos { camera.pos() };
		const fastgltf::math::fvec3 a_pos { a.transform.translation };
		const fastgltf::math::fvec3 b_pos { b.transform.translation };
		const double a_dist { distance(a_pos, cam_pos) };
		const double b_dist { distance(b_pos, cam_pos) };
		return a_dist > b_dist;
	};
	std::sort(m_objects.begin(), m_objects.end(), closest);
}

void BufferManager::renderGeometry(SDL_GPUCommandBuffer* cmdbuf, SDL_GPURenderPass* render_pass, const Camera &camera) const noexcept {
	const PBR default_mat {
		.color = {1, 1, 1, 1},
		.roughness = 0.5f,
		.metalness = 0.0f
	};
	const fastgltf::math::fmat4x4 proj_view { camera.proj() * camera.view() };
	const SDL_GPUBufferBinding I_BUF_BINDING {
		.buffer = m_i_buf->getResource(),
		.offset = 0
	};
	const SDL_GPUBufferBinding V_BUF_BINDING {
		.buffer = m_v_buf->getResource(),
		.offset = 0
	};
	SDL_BindGPUIndexBuffer(render_pass, &I_BUF_BINDING, SDL_GPU_INDEXELEMENTSIZE_16BIT);
	SDL_BindGPUVertexBuffers(render_pass, 0, &V_BUF_BINDING, 1);
	const CameraFragmentUniforms cam_frag_uniforms {
		.near_far = camera.nearFar(),
		.camera_pos = camera.pos(),
	};
	SDL_PushGPUFragmentUniformData(cmdbuf, 0, &cam_frag_uniforms, sizeof(CameraFragmentUniforms));
	for (const Mesh &obj : m_objects) {
		const VertexUniforms vert_uniforms {
			.camera_projection_view = proj_view,
			.mesh_model = obj.model()
		};
		const std::optional<size_t> opt_mat_index { obj.primitives.front().material_index };
		int mat_index { opt_mat_index.has_value() ? static_cast<int>(opt_mat_index.value()) : -1 };
		Uint32 buffer_start { obj.primitives.front().buffer_start };
		Uint32 buffer_count {  };
		SDL_PushGPUVertexUniformData(cmdbuf, 0, &vert_uniforms, sizeof(VertexUniforms));
		for (const Primitive prim : obj.primitives) {
			const std::optional<size_t> opt_prim_mat_index { prim.material_index };
			const int prim_mat_index = opt_prim_mat_index.has_value() ? static_cast<int>(opt_prim_mat_index.value()) : -1;
			if (mat_index != prim_mat_index) {
				const PBR mat { mat_index != -1 ? m_materials.at(mat_index) : default_mat };
				const MaterialFragmentUniforms mat_frag_uniforms {
					.color = mat.color,
					.roughness = mat.roughness,
					.metalness = mat.metalness
				};
				SDL_PushGPUFragmentUniformData(cmdbuf, 1, &mat_frag_uniforms, sizeof(MaterialFragmentUniforms));
				SDL_DrawGPUIndexedPrimitives(render_pass, buffer_count, 1, buffer_start, 0, 0);
				mat_index = prim_mat_index;
				buffer_count = prim.buffer_count;
				buffer_start = prim.buffer_start;
			} else {
				buffer_count += prim.buffer_count;
			}
		}
		const PBR mat { mat_index != -1 ? m_materials.at(mat_index) : default_mat };
		const MaterialFragmentUniforms mat_frag_uniforms {
			.color = mat.color,
			.roughness = mat.roughness,
			.metalness = mat.metalness
		};
		SDL_PushGPUFragmentUniformData(cmdbuf, 1, &mat_frag_uniforms, sizeof(MaterialFragmentUniforms));
		SDL_DrawGPUIndexedPrimitives(render_pass, buffer_count, 1, buffer_start, 0, 0);
	}
}

fastgltf::math::fmat4x4 BufferManager::Mesh::model() const {
	const auto identity { fastgltf::math::fmat4x4(1) };
	const auto translation { fastgltf::math::translate(identity, transform.translation) };
	const auto scale { fastgltf::math::scale(translation, transform.scale) };
	const auto rotation { fastgltf::math::rotate(scale, transform.rotation) };
	return rotation;
}
