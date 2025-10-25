struct VSInput
{
	float3 Position : POSITION;
	float3 Normal : NORMAL;
	float3 Color : COLOR0;
	float2 TexCoords : TEXCOORD0;
	float4 Tangent : TANGENT;
};

struct PSInput
{
	float4 position : SV_POSITION;
	float3 color : COLOR0;
	float2 uv : TEXCOORD;
};


struct UniformBuffer
{
	float4x4 view;
	float4x4 proj;
};
ConstantBuffer<UniformBuffer> ubo;


struct Constants
{
	float4x4 model;
	float3 color;
	
};


Sampler2D g_texture;

[shader("vertex")]
PSInput VSMain(VSInput input, uniform Constants pc)
{
	PSInput result;

	result.position = mul(ubo.proj, mul(ubo.view, mul(pc.model, float4(input.Position.xyz, 1.0))));;
	result.uv = input.TexCoords;
	result.color = input.Color;

	return result;
}

[shader("pixel")]
float4 PSMain(PSInput input) : SV_TARGET
{
	return g_texture.Sample(input.uv) * float4(input.color, 0);
}