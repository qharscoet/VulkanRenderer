static const float3 positions[8] = {
    float3(-1.0, -1.0, 1.0),
	float3(1.0, -1.0, 1.0),
	float3(1.0, 1.0, 1.0),
	float3(-1.0, 1.0, 1.0),
	
	float3(-1.0, -1.0, -1.0),
	float3(1.0, -1.0, -1.0),
	float3(1.0, 1.0, -1.0),
	float3(-1.0, 1.0, -1.0)
};

static const int indices[36] = {
	1, 0, 2, 3, 2, 0,
	5, 1, 6, 2, 6, 1,
	6, 7, 5, 4, 5, 7,
	0, 4, 3, 7, 3, 4,
	5, 4, 1, 0, 1, 4,
	2, 3, 6, 7, 6, 3 
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 worldPos : WORLDPOSITION;
};


struct UBO
{
	float4x4 view;
	float4x4 proj;
};

cbuffer ubo : register(b0, space0)
{
	UBO ubo;
}


[[vk::binding(1, 0)]]
TextureCube g_texture : register(t0);
[[vk::binding(1, 0)]]
SamplerState g_sampler : register(s0);

[shader("vertex")]
PSInput VSMain(int vertex_id : SV_VertexID)
{
	PSInput result;

    int idx = indices[vertex_id];
    float3 pos = positions[idx];

	float4x4 rotOnlyView = ubo.view;
	rotOnlyView[3][0] = 0.0;
	rotOnlyView[3][1] = 0.0;
    rotOnlyView[3][2] = 0.0;

    result.position = mul(ubo.proj, mul(rotOnlyView, float4(pos, 0))).xyww;
    result.worldPos = pos;

	return result;
}

[shader("pixel")]
float4 PSMain(PSInput input) : SV_TARGET
{

	return g_texture.Sample(g_sampler, input.worldPos);
}