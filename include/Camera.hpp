#pragma once
#include "SDL3/SDL_scancode.h"
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/math.hpp>
#include <fastgltf/types.hpp>

class Camera {
public:
	Camera() { }
	~Camera() { }
	/*
	 * SDL iterate handler
	 */
	void iterate();
	/**
	 * SDL_Event handler
	 */
	void event(const SDL_Event &e);
	/**
	 * Evaluate this camera's projection matrix
	 *
	 * Using the ratio and field of view of the camera,
	 * returns the projection matrix.
	 */
	fastgltf::math::fmat4x4 proj() const;
	/**
	 * Evaluate this camera's view matrix
	 *
	 * Using the position and forward direction of the camera,
	 * returns the view matrix.
	 */
	fastgltf::math::fmat4x4 view() const;
	/**
	 * Retrieve the near and far constants
	 *
	 * Returns values as a 2D vector
	 */
	fastgltf::math::fvec2 nearFar() const { return m_near_far; }
	/**
	 * Retrieve the current position of the camera
	 */
	fastgltf::math::fvec3 pos() const { return m_transform.translation; }
private:
	const fastgltf::math::fvec2 m_near_far { -1, 1 };
	fastgltf::math::fvec2 m_aspect_ratio { 0, 0 }; // changes with window aspect ratio
	fastgltf::TRS m_transform { };
	float m_pitch { }, m_yaw { };
	fastgltf::math::fvec3 m_forward_dir {
		fastgltf::math::normalize(fastgltf::math::fvec3{
			static_cast<float>(cos(m_pitch) * sin(m_yaw)),
			static_cast<float>(-sin(m_pitch)),
			static_cast<float>(cos(m_pitch) * -cos(m_yaw))
		})
	};
	const float m_speed { 1.0f };
	fastgltf::math::fvec3 m_velocity { };
	enum MOVEMENT {
		FORWARD, LEFT, BACKWARD, RIGHT, UP, DOWN
	};
	bool m_movement_inputs[6] { false, false, false, false, false, false };
	fastgltf::math::fvec3 right() const {
		return { static_cast<float>(cos(m_yaw)), 0, static_cast<float>(sin(m_yaw)) };
	}
	// Taken from https://github.com/spnda/fastgltf/blob/main/examples/gl_viewer/gl_viewer.cpp
	/** Creates a right-handed view matrix */
	fastgltf::math::fmat4x4 lookAtRH(const fastgltf::math::fvec3& eye, const fastgltf::math::fvec3& center, const fastgltf::math::fvec3& up) const noexcept;
	/**
	 * Creates a right-handed perspective matrix, with the near and far clips at -1 and +1, respectively.
	 * @param fov The FOV in radians
	 */
	fastgltf::math::fmat4x4 perspectiveRH(float fov, float ratio, float zNear, float zFar) const noexcept;
};
