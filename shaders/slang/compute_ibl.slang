RWTexture2DArray<float4> irradiance_map;
SamplerCube env_map;

static const float PI = 3.14159265359;


float radicalInverse_VdC(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

float2 hammersley2d(uint i, uint N) {
    return float2(float(i) / float(N), radicalInverse_VdC(i));
}

float3 hemisphereSample_uniform(float2 xi) {
    float phi = xi.x * 2.0 * PI;
    float cosTheta = 1.0 - xi.y;
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    return float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}

float3 hemisphereSample_cos(float2 xi) {
    float phi = xi.x * 2.0 * PI;
    float cosTheta = sqrt(1.0 - xi.y);
    float sinTheta = sqrt(xi.y); // equivalent to `sqrt(1.0 - cosTheta*cosTheta)`;
    return float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
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
    irradiance_map.GetDimensions(cubeSize.x, cubeSize.y, cubeSize.z);
    uint face = threadId.z;
    float2 uv = (float2(threadId.xy) + 0.5f) / cubeSize.xy;

    float3 normal = getDirectionFromCubemapUV(face, uv);
    normal.y = -normal.y;

    float3 irradiance = float3(0.0);

    float3 up = float3(0.0, 1.0, 0.0);
    float3 right = normalize(cross(up, normal));
    up = normalize(cross(normal, right));

	// loops over pi
#if 1
    float sampleDelta = 0.025;
    float nrSamples = 0.0;
    for (float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta)
    {
        for (float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta)
        {
            // spherical to cartesian (in tangent space)
            float3 tangentSample = float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            // tangent space to world
            float3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal;

            irradiance += env_map.SampleLevel(sampleVec, 0).rgb * cos(theta) * sin(theta);
            nrSamples++;
        }
    }

	 irradiance = PI * irradiance * (1.0 / float(nrSamples));
#endif
     //hammersley
#if 0
    uint nrSamples = 1024;
    for (uint i = 0; i < nrSamples; ++i) {
        float2 xi = hammersley2d(i, nrSamples);
        float3 tangentSample = hemisphereSample_cos(xi);
        float3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal;

        irradiance += env_map.SampleLevel(sampleVec, 0).rgb;

	}
    irradiance = irradiance * (1.0 / float(nrSamples));
#endif
    irradiance_map[threadId] = float4(irradiance, 1.0);
}