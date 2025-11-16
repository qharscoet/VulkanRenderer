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
    float3 normal : NORMAL;
    float3 worldPos : WORLDPOSITION;
    float3 tangent : TANGENT;
    float sign : BINORMAL;
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

    float3 eye;
    uint light_count;

    uint normal_mode;
    uint debug_mode;
    uint ibl;
	uint padding1;

    float4 baseColorFactor;

    float metallicFactor;
    float roughnessFactor;
    float occlusionStrength;

};

float3x3 Inverse3x3(float3x3 m)
{
    // Compute determinant of the 3x3 matrix
    float det =
        m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) -
        m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
                m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);

    if (abs(det) < 1e-6)
    {
        // If determinant is too small, return identity matrix (fallback)
        return float3x3(1.0, 0.0, 0.0,
                        0.0, 1.0, 0.0,
                        0.0, 0.0, 1.0);
    }

    float invDet = 1.0 / det;

    // Compute inverse using cofactor method
    float3x3 invMat;
    invMat[0][0] = (m[1][1] * m[2][2] - m[1][2] * m[2][1]) * invDet;
    invMat[0][1] = (m[0][2] * m[2][1] - m[0][1] * m[2][2]) * invDet;
    invMat[0][2] = (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * invDet;

    invMat[1][0] = (m[1][2] * m[2][0] - m[1][0] * m[2][2]) * invDet;
    invMat[1][1] = (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * invDet;
    invMat[1][2] = (m[0][2] * m[1][0] - m[0][0] * m[1][2]) * invDet;

    invMat[2][0] = (m[1][0] * m[2][1] - m[1][1] * m[2][0]) * invDet;
    invMat[2][1] = (m[0][1] * m[2][0] - m[0][0] * m[2][1]) * invDet;
    invMat[2][2] = (m[0][0] * m[1][1] - m[0][1] * m[1][0]) * invDet;

    return invMat;
}

[shader("vertex")]
PSInput VSMain(VSInput input, uniform Constants pc)
{
    PSInput result = (PSInput)0;

    result.position = mul(ubo.proj, mul(ubo.view, mul(pc.model, float4(input.Position.xyz, 1.0)))); ;
    result.uv = input.TexCoords;
    result.color = input.Color;

    // Extract the upper-left 3x3 part of the model matrix
    float3x3 normalMatrix = (float3x3)pc.model;

    // Compute the inverse transpose of the normal matrix
    normalMatrix = transpose(Inverse3x3(normalMatrix));

    result.normal = normalize(mul(normalMatrix, input.Normal));
    result.tangent = normalize(mul(normalMatrix, input.Tangent.xyz));
    result.sign = input.Tangent.w; // normalize(cross(result.tangent, result.normal));
    result.worldPos = mul(pc.model, float4(input.Position.xyz, 1.0f)).xyz;
    return result;
}

struct Light
{
    float ambiant;
    float diffuse;
    float specularStrength;
    float shininess;

    float3 position;
    uint type;

    float4 params;

    float3 color;
};

#define constant params.x
#define linear params.y
#define quadratic params.z

#define direction params
#define cutOff params.w

#define POINTLIGHT 0
#define DIRLIGHT 1
#define SPOTLIGHT 2

static const float PI = 3.14159265359;


[[vk::binding(0, 1)]]
cbuffer light_data : register(b0, space1)
{
    Light light[10];
}
[[vk::binding(1, 1)]]
SamplerCube irradianceMap;
[[vk::binding(2, 1)]]
SamplerCube specularMap; 
[[vk::binding(3, 1)]]
Sampler2D brdfLUT;


Sampler2D g_baseColor;
Sampler2D g_normal;
Sampler2D g_metallicRoughness;
Sampler2D g_occlusion;
Sampler2D g_emissive;


/* https://learnopengl.com/PBR/Theory */
/* https://github.com/KhronosGroup/glTF-Sample-Renderer/ */

float3 computeNormal(PSInput input)
{
    float3 NTex = g_normal.Sample(input.uv).xyz * 2.0f - 1.0f;

    /*http://www.mikktspace.com/*/
    float3 vN = normalize(input.normal);
    float3 vT = normalize(input.tangent);
    float3 vNt = normalize(NTex);

    float3 vB = input.sign * cross(vN, vT);
    float3 vNout = normalize(vNt.x * vT + vNt.y * vB + vNt.z * vN);

    return vNout;
}

float3 getRadiance(Light l, PSInput input, float3 V)
{
    float3 L = normalize(l.position - input.worldPos);
    if (l.type == DIRLIGHT)
        L = normalize(-l.position); // position is used as direction in dirlight

    float3 H = normalize(V + L);

    float attenuation = 1.0f;
    if (l.type == POINTLIGHT)
    {
        float distance = length(l.position - input.worldPos);
        attenuation = 1.0 / (distance * distance);
    } else if (l.type == SPOTLIGHT)
    {
        float distance = length(l.position - input.worldPos);
        attenuation = 1.0 / (distance * distance);

        float theta = dot(L, normalize(-l.direction.xyz));
        float epsilon = 0.01;
        float intensity = saturate((theta - (l.cutOff - epsilon)) / epsilon);
        attenuation *= intensity;
    }

    return l.color * attenuation;
}

float3 fresnelSchlick(float HdotV, float3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - HdotV, 0.0, 1.0), 5.0);
}

float3 fresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    return F0 + (max(float3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float NDistributionGGX(float NdotH, float roughness)
{
	float a = roughness * roughness; //according to Disney and Epic this looks better?
    float a2 = a * a;

	float denom = (NdotH * NdotH) * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0; // TODO : remap k for IBL when needed

	float denom = NdotV * (1.0 - k) + k;
    return NdotV / denom;
}

float GeometrySmith(float NdotV, float NdotL, float roughness)
{
	float ggx1 = GeometrySchlickGGX(NdotV, roughness);
	float ggx2 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

[shader("pixel")]
float4 PSMain(PSInput input, uniform Constants pc) : SV_TARGET
{

    float4 baseColor = g_baseColor.Sample(input.uv) * pc.baseColorFactor;
    float4 metallicRoughness = g_metallicRoughness.Sample(input.uv);

    float metallic = metallicRoughness.b * pc.metallicFactor;
    float roughness = metallicRoughness.g * pc.roughnessFactor;
    float occlusion = 1.0 + pc.occlusionStrength * (g_occlusion.Sample(input.uv).r - 1.0f);
    float3 emissive = g_emissive.Sample(input.uv).xyz;

    float3 Lo = 0.0f;
    float3 N = pc.normal_mode && !isnan(input.tangent) ? computeNormal(input) : normalize(input.normal);
    float3 V = normalize(pc.eye - input.worldPos);

    float3 F0 = float3(0.04, 0.04, 0.04); // dielectric reflectance
    F0 = lerp(F0, baseColor.rgb, metallic);

	for (int i = 0; i < pc.light_count; i++)
	{
		Light l = light[i];
		float3 L = normalize(l.position - input.worldPos);
		if (l.type == DIRLIGHT)
			L = normalize(-l.position); // position is used as direction in dirlight
		float3 H = normalize(V + L);

		float3 radiance = getRadiance(l, input, V);


		float NdotL = max(dot(N, L), 0.0);
		float NdotV = max(dot(N, V), 0.0);
		float NdotH = max(dot(N, H), 0.0);
		float HdotV = max(dot(H, V), 0.0);

		float3 F = fresnelSchlick(HdotV, F0);

		float NDF = NDistributionGGX(NdotH, roughness);
		float G = GeometrySmith(NdotV, NdotL, roughness);

		float3 numerator = NDF * G * F;
		float denominator = 4.0 * NdotV * NdotL + 0.001; // prevent divide by zero
		float3 specular = numerator / denominator;

		float3 kS = F;
		float3 kD = 1.0 - kS;
		kD *= 1.0 - metallic;

		Lo += (kD * (baseColor.rgb/ PI) + specular ) * radiance * NdotL;
	}

    float3 ambient;
    if (pc.ibl > 0)
    {
        float3 kS = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
		float3 kD = 1.0 - kS;
		float3 irradiance = irradianceMap.Sample(N).rgb;
        float3 diffuse = irradiance * baseColor.rgb;

        float3 R = reflect(-V, N);

        uint2 cubeSize;
        uint mipLevels;
        specularMap.GetDimensions(0, cubeSize.x, cubeSize.y, mipLevels);

        float3 prefilteredColor = specularMap.SampleLevel(R, roughness * mipLevels).rgb;

        float3 F = kS;
        float2 envBRDF = brdfLUT.Sample(float2(max(dot(N, V), 0.0), roughness)).rg;
        float3 specular = prefilteredColor * (F * envBRDF.x + envBRDF.y);
        
		ambient = (kD * diffuse + specular) * occlusion;
    }
    else {
		ambient = float3(0.03) * baseColor.rgb * occlusion.r;
	}


    float3 color = ambient + Lo + emissive;
    //float3 color = (Lo * occlusion) + emissive;


    // HDR Tone mapping ? Reinhard operator nantoka
    //color = color / (color + float3(1.0));
    //color = pow(color, float3(1.0 / 2.2));
    return float4(color, 0);
}