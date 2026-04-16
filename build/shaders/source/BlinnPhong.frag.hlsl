cbuffer VIEW : register(b0, space3) {
	float2 near_far;
	float4 view_pos;
};
cbuffer PBR : register(b1, space3) {
	float4 color;
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

Output main(Input input) {
	float3 light_color = float3(1, 1, 1);
	float3 light_pos = float3(50, 50, 50);

	float3 ambient = 0.1 * light_color;

	float3 light_direction = normalize(light_pos - input.WorldPos.xyz);
	float3 diffuse = max(0, dot(light_direction, input.Normal)) * light_color;

	float3 pos = view_pos.xyz;
	float3 view_direction = normalize(pos - input.WorldPos.xyz);
	float3 reflect_direction = reflect(-light_direction, input.Normal);
	float3 specular = pow(max(dot(view_direction, reflect_direction), 0.0), 32) * 0.5 * light_color;

	Output result;
	result.Color = float4(color.xyz * (ambient + diffuse + specular), 1.0f);
	return result;
}
