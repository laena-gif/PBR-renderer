#include "MathHelpers.hlsli"

void ComputeAlbedoRf0(float3 albedo, float metalness, out float3 Rf0, out float3 outAlbedo)
{
	outAlbedo = albedo * saturate(1.0 - metalness);
	Rf0 = lerp(RF0_DIELECTRICS, albedo, metalness);
}

float Pow5( float x )
{
	float xx = x*x;
	return xx * xx * x;
}

float3 DiffuseTermBurley(float roughness, float NoL, float NoV, float VoH)
{
	/*
	float f = 2.0 * VoH * VoH * roughness - 0.5;
	float FdV = f * pow(saturate(1.0 - NoV), 5.0) + 1.0;
	float FdL = f * pow(saturate(1.0 - NoL), 5.0) + 1.0;
	float d = FdV * FdL;

	return d / PI;*/
	float FD90 = 0.5 + 2 * VoH * VoH * roughness;
	float FdV = 1 + (FD90 - 1) * Pow5( 1 - NoV );
	float FdL = 1 + (FD90 - 1) * Pow5( 1 - NoL );
	return ( (1 / PI) * FdV * FdL );
}

float DistributionTermGgx(float roughness, float NoH)
{
	float p = pow(roughness, 4.0f);
	float q1 = pow(NoH * NoH * (p - 1) + 1, 2.0f);
	float q = 1 / (PI * q1);

	return p * q;
}

float GeometryTermSmith(float roughness, float NoV, float NoL)
{
	float k = pow((roughness + 1.0), 2.0) / 8.0;
	float q1 = NoV * (1 - k) + k;
	float q2 = NoL * (1 - k) + k;
	float G1 = NoV / q1;
	float G2 = NoL / q2;

	return G1 * G2;
}

float3 FresnelTermShlick(float3 F0, float cosa)
{
	return F0 + (1 - F0) * pow(saturate(1.0 - cosa), 5.0);
}


float3 RedGuard(float3 v)
{
	if (any(v < 0) || any(isnan(v)) || any(!isfinite(v)))
	{
		return float3(1.0, 0.0, 0.0);
	}

	return v;
}

float3 SpecularTerm(float roughness, float NoH, float NoL, float NoV, float3 F0, float VoH)
{
	float3 p = DistributionTermGgx(roughness, NoH) * GeometryTermSmith(roughness, NoV, NoL) * FresnelTermShlick(F0, VoH);
	return RedGuard(p);
}

float3 Brdf(float roughness, float metalness, float3 Albedo, float3 N, float3 V, float3 L)
{
	float3 H = normalize(L + V);
	float NoH = saturate(dot(N, H));
	float VoH = saturate(dot(V, H));
	float NoL = saturate(dot(N, L));
	float NoV = saturate(dot(N, V));
	float3 albedo;
	float3 F0;
	ComputeAlbedoRf0(Albedo, metalness, F0, albedo);
	float roughnessL = max(.01, roughness);

	return (DiffuseTermBurley(roughness, NoL, NoV, VoH) * albedo + SpecularTerm(roughnessL, NoH, NoL, NoV, F0, VoH)) * NoL;
	//return NoL * albedo;

}
