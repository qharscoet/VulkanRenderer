RWTexture2DArray<float4> cubemap;
Sampler2D equirectangular;

static const float2 invAtan = float2(0.1591, 0.3183);
float2 SampleSphericalMap(float3 v)
{
    float2 uv = float2(atan2(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}


float3 getDirectionFromCubemapUV(uint face, float2 uv)
{
    uv = uv * 2.0 - 1.0; // [-1, 1]
    float3 dir;

    switch(face)
    {
        case 0: dir = float3( 1,  uv.y, -uv.x); break; // +X
        case 1: dir = float3(-1,  uv.y,  uv.x); break; // -X
        case 2: dir = float3( uv.x, -1,  uv.y); break; // +Y
        case 3: dir = float3( uv.x,  1, -uv.y); break; // -Y
        case 4: dir = float3( uv.x,  uv.y,  1); break; // +Z
        case 5: dir = float3(-uv.x,  uv.y, -1); break; // -Z
    }
    return normalize(dir);
}

[shader("compute")]
[numthreads(32,32,1)]
void CSMain(uint3 threadId : SV_DispatchThreadID)
{
    uint3 cubeSize;
    cubemap.GetDimensions(cubeSize.x, cubeSize.y, cubeSize.z);
    uint face = threadId.z;
    float2 uv = (float2(threadId.xy) + 0.5f) / cubeSize.xy;

	float3 dir = getDirectionFromCubemapUV(face, uv);
	float2 equiUV = SampleSphericalMap(dir);
	
    cubemap[threadId] = equirectangular.SampleLevel(equiUV, 0);
}