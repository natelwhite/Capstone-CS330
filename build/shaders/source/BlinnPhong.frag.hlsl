cbuffer VIEW : register(b0, space3) {
	float2 near_far;
	float3 view_pos;
};
cbuffer PBR : register(b1, space3) {
	float4 albedo;
	float1 roughness;
	float1 metalness;
}

struct Input {
	float4 Position : SV_POSITION;
	float3 Normal : NORMAL;
	float4 WorldPos : TEXCOORD1;
};
struct Output {
	float4 Color : SV_Target0;
};

struct Light {
	float strength;
	float3 color;
	float3 pos;
};

// taken from https://learnopengl.com/PBR/Lighting
static const float PI = 3.14159265;
float3 fresnelSchlick(float cosTheta, float3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
float DistributionGGX(float3 N, float3 H, float roughness) {
	float a = roughness * roughness;
	float a2 = a*a;
	float NdotH = max(dot(N, H), 0.0);
	float NdotH2 = NdotH*NdotH;
	float num = a2;
	float denom = (NdotH2 * (a2 - 1.0) + 1.0);
	denom = PI * denom * denom;
	return num / denom;
}
float GeometrySchlickGGX(float NdotV, float roughness) {
	float r = (roughness + 1.0);
	float k = (r*r) / 8.0;
	float num   = NdotV;
	float denom = NdotV * (1.0 - k) + k;
	return num / denom;
}
float GeometrySmith(float3 N, float3 V, float3 L, float roughness) {
	float NdotV = max(dot(N, V), 0.0);
	float NdotL = max(dot(N, L), 0.0);
	float ggx2  = GeometrySchlickGGX(NdotV, roughness);
	float ggx1  = GeometrySchlickGGX(NdotL, roughness);
	return ggx1 * ggx2;
}
float3 mix(float3 x, float3 y, float a) {
	return x * (1 - a) + y * a;
}

static const Light lights[3] = { {
	2,
	float3(1, 1, 1),
	float3(-3, -1, 1)
}, {
	2,
	float3(1, 1, 1),
	float3(0, -1, 1)
}, {
	2,
	float3(1, 1, 1),
	float3(3, -1, 1)
} };

Output main(Input input) {
	float3 worldPos = input.WorldPos.xyz;
	float3 N = normalize(input.Normal);
	float3 V = normalize(view_pos - worldPos);
	float3 F0 = float3(0.04, 0.04, 0.04);
	F0 = mix(F0, albedo.xyz, metalness);

	// light radiance
	float3 Lo = float3(0, 0, 0);
	for (int i = 0; i < 3; ++i) {
		float3 L = normalize(lights[i].pos - worldPos);
		float3 H = normalize(V + L);
		float distance = length(lights[i].pos - worldPos);
		float attenuation = 1.0 / distance; // linear attenuation
		float3 radiance = lights[i].color * attenuation * lights[i].strength;

		// cook-torrance brdf
		float NDF = DistributionGGX(N, H, roughness);
		float G = GeometrySmith(N, V, L, roughness);
		float3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

		float3 kS = F;
		float3 kD = float3(1.0, 1.0, 1.0) - kS;
		kD *= 1.0 - metalness;

		float3 numerator = NDF * G * F;
		float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.001;
		float3 specular = numerator / denominator;
		
		// add to radiance Lo
		float NdotL = max(dot(N, L), 0.0);
		Lo += (kD * albedo.xyz / PI + specular) * radiance * NdotL;
	}

	float3 ambient = float3(0.03, 0.03, 0.03) * albedo.xyz;
	float3 result;
	result = ambient + Lo;
	result = result / (result + float3(1.0, 1.0, 1.0));
	float gamma_correction = 1.0 / 2.2;
	result = pow(result, gamma_correction);
	return Output(float4(result, 1.0));
}

