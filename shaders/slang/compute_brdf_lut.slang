RWTexture2D brdf_lut;

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

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float k = (roughness * roughness) / 2.0; // This is for IBL

    float denom = NdotV * (1.0 - k) + k;
    return NdotV / denom;
}

float GeometrySmith(float NdotV, float NdotL, float roughness)
{
    float ggx1 = GeometrySchlickGGX(NdotV, roughness);
    float ggx2 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}
float2 IntegrateBRDF(float NdotV, float roughness)
{
    float3 V;
    V.x = sqrt(1.0 - NdotV * NdotV);
    V.y = 0.0;
    V.z = NdotV;

    float A = 0.0;
    float B = 0.0;

    float3 N = float3(0.0, 0.0, 1.0);

    const uint SAMPLE_COUNT = 1024u;
    for (uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        float2 Xi = hammersley2d(i, SAMPLE_COUNT);
        float3 H = ImportanceSampleGGX(Xi, N, roughness);
        float3 L = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(L.z, 0.0);
        float NdotH = max(H.z, 0.0);
        float VdotH = max(dot(V, H), 0.0);
        float NdotV = max(dot(N, V), 0.0);

        if (NdotL > 0.0)
        {
            float G = GeometrySmith(NdotV, NdotL, roughness);
            float G_Vis = (G * VdotH) / (NdotH * NdotV);
            float Fc = pow(1.0 - VdotH, 5.0);

            A += (1.0 - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }
    A /= float(SAMPLE_COUNT);
    B /= float(SAMPLE_COUNT);
    return float2(A, B);
}


[shader("compute")]
[numthreads(32, 32, 1)]
void CSMain(uint3 threadId: SV_DispatchThreadID)
{
    uint2 lutSize;
    brdf_lut.GetDimensions(lutSize.x, lutSize.y);

    float2 uv = (threadId.xy + 0.5f)/ lutSize;
	uv.y = 1 - uv.y;
    float2 res = IntegrateBRDF(uv.x, uv.y);

    brdf_lut[threadId.xy] = float4(res.xy, 0, 0);
}