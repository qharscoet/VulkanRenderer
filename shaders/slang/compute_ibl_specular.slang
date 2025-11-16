RWTexture2DArray<float4> specular_map;
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

float3 ImportanceSampleGGX(float2 Xi, float3 N, float roughness)
{
    float a = roughness * roughness;

    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    // from spherical coordinates to cartesian coordinates
    float3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;

    // from tangent-space vector to world-space sample vector
    float3 up = abs(N.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    float3 tangent = normalize(cross(up, N));
    float3 bitangent = cross(N, tangent);

    float3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
    return normalize(sampleVec);
}

float NDistributionGGX(float NdotH, float roughness)
{
    float a = roughness * roughness; // according to Disney and Epic this looks better?
    float a2 = a * a;

    float denom = (NdotH * NdotH) * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
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

struct Constants {
    float roughness;
    uint mipLevel;
};

[shader("compute")]
[numthreads(32, 32, 1)]
void CSMain(uint3 threadId: SV_DispatchThreadID, uniform Constants pc)
{
    uint3 cubeSize;
    specular_map.GetDimensions(cubeSize.x, cubeSize.y, cubeSize.z);
    uint face = threadId.z;
    float2 uv = (float2(threadId.xy) + 0.5f) / cubeSize.xy;

    uint2 envSize;
    env_map.GetDimensions(envSize.x, envSize.y); 

    float3 normal = getDirectionFromCubemapUV(face, uv);
    normal.y = -normal.y;

    float3 N = normal;
    float3 R = N;
    float3 V = R;

    float3 up = float3(0.0, 1.0, 0.0);
    float3 right = normalize(cross(up, normal));
    up = normalize(cross(normal, right));

    float totalWeight = 0.0;
    float3 prefilteredColor = float3(0, 0, 0);

    uint nrSamples = 1024;
    for (uint i = 0; i < nrSamples; ++i) {
        float2 xi = hammersley2d(i, nrSamples);
        float3 H = ImportanceSampleGGX(xi, N, pc.roughness);
        float3 L = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(dot(N, L), 0.0);
        if (NdotL > 0.0)
        {
            float NdotH = max(dot(N, H), 0.0);
            float HdotV = max(dot(H, V), 0.0);

            float D = NDistributionGGX(NdotH, pc.roughness);
            float pdf = (D * NdotH / (4.0 * HdotV)) + 0.0001;

            float saTexel = 4.0 * PI / (6.0 * envSize.x * envSize.x);
            float saSample = 1.0 / (float(nrSamples) * pdf + 0.0001);

            float mipLevel = pc.roughness == 0.0 ? 0.0 : 0.5 * log2(saSample / saTexel);

            prefilteredColor += env_map.SampleLevel(L, mipLevel).rgb * NdotL;
            totalWeight += NdotL;
        }
	}

    prefilteredColor = prefilteredColor / totalWeight;

    specular_map[threadId] = float4(prefilteredColor, 1.0);
}