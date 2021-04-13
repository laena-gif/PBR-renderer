class Camera
{
public:
	Camera();
	const float4x4& GetViewMatrix() const { return mMatView; }
	const float4x4& GetProjMatrix() const { return mMatProj; }
	const float3& GetCameraPos() const { return mPos; }
	void AdvanceYaw(float delta);
	void AdvancePitch(float delta);
	void AdvancePosX(float delta);
	void AdvancePosY(float delta);
	void AdvancePosZ(float delta);

	float3 GetViewVector() const;
	float3 GetRightVector() const;

private:
	void UpdateViewMatrix();
	float3 mPos{ -3.474f, 4.311f, 4.693f };
	float4x4 mMatProj;
	float4x4 mMatView;
	float mYaw = -0.500f;
	float mPitch = -0.48f;
};