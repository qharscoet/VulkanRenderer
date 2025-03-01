struct VSInput
{
	[[vk::location(0)]] float3 Position : POSITION;
	[[vk::location(1)]] float3 Normal : NORMAL;
	[[vk::location(2)]] float3 Color : COLOR0;
	[[vk::location(3)]] float3 TexCoords : TEXCOORD0;
};

struct PSInput
{
	float4 position : SV_POSITION;
	[[vk::location(0)]] float3 color : COLOR0;
	[[vk::location(1)]] float2 uv : TEXCOORD;
	[[vk::location(2)]] float3 normal : NORMAL;
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

struct Light
{
	float3 position;
};
	
[[vk::binding(0, 1)]]
cbuffer light_data : register(b0, space1)
{
	Light light;
}

struct Constants
{
	float4x4 model;
};

[[vk::push_constant]]
Constants pc;

PSInput VSMain(VSInput input)
{
	PSInput result = (PSInput)0;

	result.position = mul(ubo.proj, mul(ubo.view, mul(pc.model, float4(input.Position.xyz, 1.0))));;
	result.uv = input.TexCoords;
	result.color = input.Color;
	result.normal = input.Normal;
	return result;
}



[[vk::binding(1, 0)]]
Texture2D g_texture : register(t0);
[[vk::binding(1, 0)]]
SamplerState g_sampler : register(s0);

float4 PSMain(PSInput input) : SV_TARGET
{
	return g_texture.Sample(g_sampler, input.uv) * float4(input.color, 0) * float4(light.position, 1.0f);
}