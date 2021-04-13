#include <PCH.h>
#include "LightSource.h"

LightSource::LightSource()
{
	mMatProj = glm::perspectiveFovRH_ZO(glm::radians(54.0f), 2048.0f, 2048.0f, 0.1f, 100.0f);
	mMatView = glm::lookAtRH(mPos, mPos+GetViewVector(), float3(0.0f, 0.0f, 1.0f));
}

void LightSource::AdvancePosX(float delta)
{
	mPos += float3(delta, 0.0f, 0.0f);
	UpdateViewMatrix();
}

void LightSource::AdvancePosY(float delta)
{
	mPos += float3(0.0f, delta, 0.0f);
	UpdateViewMatrix();
}

float3 LightSource::GetViewVector() const
{
	return float3(cos(mYaw) * cos(mPitch), sin(mYaw) * cos(mPitch), sin(mPitch));
}

void LightSource::UpdateViewMatrix()
{
	mMatView = glm::lookAtRH(mPos, mPos + GetViewVector(), float3(0.0f, 0.0f, 1.0f));
}
float3 LightSource::GetRightVector() const
{
	return glm::cross(GetViewVector(), float3(0.0f, 0.0f, 1.0f));
}

void LightSource::AdvanceYaw(float delta)
{
	mYaw += delta;
	UpdateViewMatrix();
}

void LightSource::AdvancePitch(float delta)
{
	mPitch += delta;
	UpdateViewMatrix();
}