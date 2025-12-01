struct VSInput
{
	float3 Position : POSITION;
	float3 Normal : NORMAL;
	float3 Color : COLOR0;
	float2 TexCoords : TEXCOORD0;
	float4 Tangent : TANGENT;
};

struct GSInput
{
	float4 position : SV_POSITION;
};

struct GSOutput
{
    float4 position : SV_POSITION;
    float4 old_pos : OLDPOS;

    uint render_idx : SV_RenderTargetArrayIndex;
};


struct UniformBuffer
{
    float4x4 shadowMatrices[6];
    float3 lightPos;
    float far_plane;
};
ConstantBuffer<UniformBuffer> ubo;

struct Constants
{
	float4x4 model;
};

[shader("vertex")]
GSInput VSMain(VSInput input, uniform Constants pc)
{
    GSInput result;

	result.position =  mul(pc.model, float4(input.Position.xyz, 1.0));

	return result;
}

[shader("geometry")]
[maxvertexcount(18)]
void GSMain(triangle GSInput input[3],
			inout TriangleStream<GSOutput> triStream)
{
    GSOutput output;
	for (int face = 0; face < 6; ++face)
	{
        for (int i = 0; i < 3; ++i)
        {
            output.position = mul(ubo.shadowMatrices[face], input[i].position);
            output.old_pos = input[i].position;
            output.render_idx = face;
			triStream.Append(output);
        }
		triStream.RestartStrip();
    }
}

[shader("pixel")]
float PSMain(GSOutput input) : SV_Depth
{
    float lightDistance = length(input.old_pos.xyz - ubo.lightPos);

    // map to [0;1] range by dividing by far_plane
    lightDistance = lightDistance / ubo.far_plane;

    return lightDistance;
}