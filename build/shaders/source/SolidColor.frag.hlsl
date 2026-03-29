cbuffer UBO : register(b0, space3) {
	float2 near_far;
	float3 view_pos;
};

struct Input {
	float4 Position : SV_Position;
};

struct Output {
	float4 Color : SV_Target0;
};

Output main(Input input) {
	Output output;
	output.Color = float4(1.0f, 1.0f, 1.0f, 1.0f);
	return output;
}
