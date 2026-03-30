#include "Camera.hpp"

void Camera::iterate() noexcept {
	// Get local camera axes
	const fastgltf::math::fvec3 right_dir = right();
	const fastgltf::math::fvec3 up_dir = -fastgltf::math::cross(m_forward_dir, right_dir);
	fastgltf::math::fvec3 acceleration = {0, 0, 0};
	// Returns acceleration vector based on current speed and a movement direction
	const auto applySpeed = [&](const fastgltf::math::fvec3 &direction) {
		return fastgltf::math::fvec3 {
			direction.x() * m_speed,
			direction.y() * m_speed,
			direction.z() * m_speed
	}; };
	// Find accelerator vector based on input
	if (m_movement_inputs[FORWARD])
		acceleration += applySpeed(m_forward_dir);
	if (m_movement_inputs[BACKWARD])
		acceleration += applySpeed(-m_forward_dir);
	if (m_movement_inputs[RIGHT])
		acceleration += applySpeed(right_dir);
	if (m_movement_inputs[LEFT])
		acceleration += applySpeed(-right_dir);
	if (m_movement_inputs[UP])
		acceleration += applySpeed(up_dir);
	if (m_movement_inputs[DOWN])
		acceleration += applySpeed(-up_dir);
	// Apply acceleration to velocity
	m_velocity += acceleration;
	// Apply velocity to position
	m_transform.translation += m_velocity;
	// Apply friction from wind
	const fastgltf::math::fvec3 drag { m_velocity * -0.1f };
	m_velocity += drag;
	// When velocity is close to zero, make it zero
	const float epsilon { 0.01 };
	bool set_zero_axis[] {
		std::abs(m_velocity.x()) < epsilon && std::abs(m_velocity.x()) > 0, // x
		std::abs(m_velocity.y()) < epsilon && std::abs(m_velocity.y()) > 0, // y
		std::abs(m_velocity.z()) < epsilon && std::abs(m_velocity.z()) > 0, // z
	};
	if (set_zero_axis[0])
		m_velocity.x() = 0;
	if (set_zero_axis[1])
		m_velocity.y() = 0;
	if (set_zero_axis[2])
		m_velocity.z() = 0;
}


void Camera::event(const SDL_Event &e) noexcept {
	switch(e.type) {
	// Resize aspect ratio
	case SDL_EVENT_WINDOW_RESIZED: {
		m_aspect_ratio = { static_cast<float>(e.window.data1), static_cast<float>(e.window.data2) };
		break;
	}
	// Rotate camera
	case SDL_EVENT_MOUSE_MOTION: {
		const float sensitivity { 0.1f };
		const SDL_MouseMotionEvent motion { e.motion };
		m_yaw += fastgltf::math::radians(motion.xrel * sensitivity);
		m_pitch += fastgltf::math::radians(motion.yrel * sensitivity);
		m_forward_dir.x() = cos(m_pitch) * sin(m_yaw);
		m_forward_dir.y() = -sin(m_pitch);
		m_forward_dir.z() = cos(m_pitch) * -cos(m_yaw);
		m_forward_dir = normalize(m_forward_dir);
		break;
	}
	// Move camera
	case SDL_EVENT_KEY_DOWN: {
		switch(e.key.scancode) {
		case SDL_SCANCODE_W: m_movement_inputs[FORWARD] = true;
			break;
		case SDL_SCANCODE_A: m_movement_inputs[LEFT] = true;
			break;
		case SDL_SCANCODE_S: m_movement_inputs[BACKWARD] = true;
			break;
		case SDL_SCANCODE_D: m_movement_inputs[RIGHT] = true;
			break;
		case SDL_SCANCODE_E: m_movement_inputs[UP] = true;
			break;
		case SDL_SCANCODE_Q: m_movement_inputs[DOWN] = true;
			break;
		default:
			break;
		}
		break;
	}
	// Stop moving camera
	case SDL_EVENT_KEY_UP: {
		switch(e.key.scancode) {
		case SDL_SCANCODE_W: m_movement_inputs[FORWARD] = false;
			break;
		case SDL_SCANCODE_A: m_movement_inputs[LEFT] = false;
			break;
		case SDL_SCANCODE_S: m_movement_inputs[BACKWARD] = false;
			break;
		case SDL_SCANCODE_D: m_movement_inputs[RIGHT] = false;
			break;
		case SDL_SCANCODE_E: m_movement_inputs[UP] = false;
			break;
		case SDL_SCANCODE_Q: m_movement_inputs[DOWN] = false;
			break;
		default:
			break;
		}
		}
	}
}

fastgltf::math::fmat4x4 Camera::proj() const noexcept {
	const float ratio { m_aspect_ratio.x() / m_aspect_ratio.y() };
	const float fov { fastgltf::math::radians(70.0f) };
	return perspectiveRH(fov, ratio, m_near_far.x(), m_near_far.y());
}

fastgltf::math::fmat4x4 Camera::view() const noexcept {
	const fastgltf::math::fvec3 up { 0, 1, 0 };
	return lookAtRH(m_transform.translation, m_transform.translation + m_forward_dir, up);
}

fastgltf::math::fmat4x4 Camera::lookAtRH(const fastgltf::math::fvec3& eye, const fastgltf::math::fvec3& center, const fastgltf::math::fvec3& up) const noexcept {
	auto dir = normalize(center - eye);
	auto lft = normalize(cross(dir, up));
	auto rup = cross(lft, dir);
	fastgltf::math::mat<float, 4, 4> ret(1.f);
	ret.col(0) = { lft.x(), rup.x(), -dir.x(), 0.f };
	ret.col(1) = { lft.y(), rup.y(), -dir.y(), 0.f };
	ret.col(2) = { lft.z(), rup.z(), -dir.z(), 0.f };
	ret.col(3) = { -dot(lft, eye), -dot(rup, eye), dot(dir, eye), 1.f };
	return ret;
}

fastgltf::math::fmat4x4 Camera::perspectiveRH(float fov, float ratio, float zNear, float zFar) const noexcept {
	fastgltf::math::mat<float, 4, 4> ret(0.f);
	auto tanHalfFov = std::tanf(fov / 2.f);
	ret.col(0).x() = 1.f / (ratio * tanHalfFov);
	ret.col(1).y() = 1.f / tanHalfFov;
	ret.col(2).z() = -(zFar + zNear) / (zFar - zNear);
	ret.col(2).w() = -1.f;
	ret.col(3).z() = -(2.f * zFar * zNear) / (zFar - zNear);
	return ret;
}
