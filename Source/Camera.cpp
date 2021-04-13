#include <PCH.h>
#include "Camera.h"

Camera::Camera()
{
	mMatProj = glm::perspectiveFovRH_ZO(glm::radians(45.0f), 2560.0f, 1440.0f, 0.1f, 100.0f);
	UpdateViewMatrix();
}

void Camera::AdvanceYaw(float delta)
{
	mYaw += delta;
	UpdateViewMatrix();
}

void Camera::AdvancePitch(float delta)
{
    mPitch += delta;

	UpdateViewMatrix();
}

void Camera::AdvancePosX(float delta)
{
	mPos = mPos + GetViewVector() * delta;
	UpdateViewMatrix();
}

void Camera::AdvancePosY(float delta)
{
	mPos = mPos + GetRightVector() * delta;
	UpdateViewMatrix();
}

void Camera::AdvancePosZ(float delta)
{
	mPos += float3(0.0f, 0.0f, delta); // I don't care
	UpdateViewMatrix();
}

void Camera::UpdateViewMatrix()
{
	mMatView = glm::lookAtRH(mPos, mPos + GetViewVector(), float3(0.0f, 0.0f, 1.0f));
}

float3 Camera::GetViewVector() const
{
	return float3(cos(mYaw) * cos(mPitch), sin(mYaw) * cos(mPitch), sin(mPitch));
}

float3 Camera::GetRightVector() const
{
	return glm::cross(GetViewVector(), float3(0.0f, 0.0f, 1.0f));
}
