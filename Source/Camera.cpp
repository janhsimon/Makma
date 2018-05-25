#include "Camera.hpp"

#include <gtc/matrix_transform.hpp>

Camera::Camera(const std::shared_ptr<Window> window, const std::shared_ptr<Input> input, const glm::vec3 &position, const glm::vec3 &eulerAngles, float fov, float nearClip, float farClip, float mouseSensitivity) : Transform(position)
{
	this->window = window;
	this->input = input;
	
	this->nearClip = nearClip;
	this->farClip = farClip;
	this->mouseSensitivity = mouseSensitivity;

	movementSpeed = 0.5f;

	originalEulerAngles = eulerAngles;

	setFOV(fov);

	free = false;
	firstFrame = true;
}

void Camera::setFOV(float fov)
{
	projectionMatrix = glm::perspectiveFov(glm::radians(fov), static_cast<float>(window->getWidth()), static_cast<float>(window->getHeight()), nearClip, farClip);
	projectionMatrix[1][1] *= -1.0f;
}

void Camera::update(float delta)
{
	auto mouseCursorVisible = window->getShowMouseCursor();
	if (input->showCursorKeyPressed && !mouseCursorVisible)
	{
		window->setShowMouseCursor(true);
	}
	else if (!input->showCursorKeyPressed && mouseCursorVisible)
	{
		window->setShowMouseCursor(false);
	}

	free = input->flyKeyPressed;
	
	auto forward = getForward();
	auto right = getRight();
	auto up = getUp();

	if (!free)
	{
		forward.y = 0.0f;
		forward = glm::normalize(forward);
		
		right.y = 0.0f;
		
		up = glm::vec3(0.0f, 1.0f, 0.0f);
	}

	auto movementVector = glm::vec3(0.0f);
	movementVector += forward * (float)input->forwardKeyPressed;
	movementVector -= forward * (float)input->backKeyPressed;
	movementVector += right * (float)input->leftKeyPressed;
	movementVector -= right * (float)input->rightKeyPressed;
	
	if (free)
	{
		movementVector += up * (float)input->upKeyPressed;
		movementVector -= up * (float)input->downKeyPressed;
	}

	if (glm::length(movementVector) > 0.1f)
	{
		position += glm::normalize(movementVector) * movementSpeed * delta * (input->crouchKeyPressed ? 0.5f : 1.0f);
	}

	if (!input->showCursorKeyPressed)
	{
		yaw -= input->mouseDelta.x * mouseSensitivity * 0.01f;
		pitch += input->mouseDelta.y * mouseSensitivity * 0.01f;

		if (firstFrame)
		// we need to throw away the first frame mouse data because 
		// it contains a huge movement due to an SDL bug (?)
		// and that screw up camera rotations on the x-axis
		{
			pitch = originalEulerAngles.x;
			yaw = originalEulerAngles.y;
			roll = originalEulerAngles.z;
			firstFrame = false;
		}

		const auto PITCH_LOCK = 89.0f;

		if (pitch < -PITCH_LOCK)
			pitch = -PITCH_LOCK;
		else if (pitch > PITCH_LOCK)
			pitch = PITCH_LOCK;

		recalculateAxesFromAngles();
	}

	viewMatrix = glm::lookAt(position, position + getForward(), getUp());
}