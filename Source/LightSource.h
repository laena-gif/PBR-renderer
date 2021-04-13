class LightSource
{
public:
	LightSource();
	const float4x4& GetViewMatrix() const { return mMatView; }
	const float4x4& GetProjMatrix() const { return mMatProj; }
	void AdvancePosX(float delta);
	void AdvancePosY(float delta);
	void AdvanceYaw(float delta);
	void AdvancePitch(float delta);
	float3 GetViewVector() const;
	float3 GetRightVector() const;
	float3 GetLightPos() const { return mPos; }

private:
	void UpdateViewMatrix();
	float3 mPos{-3.474f, 3.311f, 4.693f };
	float4x4 mMatProj;
	float4x4 mMatView;
	float mYaw = -0.500f;
	float mPitch = -0.48f;

};