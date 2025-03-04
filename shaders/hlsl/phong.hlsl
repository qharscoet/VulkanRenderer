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
	[[vk::location(3)]] float3 worldPos : WORLDPOSITION;
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
#define cutOff params.z

#define POINTLIGHT 0
#define DIRLIGHT 1
#define SPOTLIGHT 2

[[vk::binding(0, 1)]]
cbuffer light_data : register(b0, space1)
{
	Light light[10];
}


[[vk::binding(1, 1)]]
cbuffer material_data : register(b1, space1)
{
	float3 mat_diffuse;
	float pad2;
	float3 mat_specular;
	float pad3;
	float mat_shininess;
}

struct Constants
{
	float4x4 model;
	float3 eye;
	uint light_count;
};

[[vk::push_constant]]
Constants pc;

//ChatGPT generated as it's not built-in, but we won't need this when we upgrade models
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

PSInput VSMain(VSInput input)
{
	PSInput result = (PSInput)0;

	result.position = mul(ubo.proj, mul(ubo.view, mul(pc.model, float4(input.Position.xyz, 1.0))));;
	result.uv = input.TexCoords;
	result.color = input.Color;
	
	// Extract the upper-left 3x3 part of the model matrix
	float3x3 normalMatrix = (float3x3) pc.model;

    // Compute the inverse transpose of the normal matrix
	normalMatrix = transpose(Inverse3x3(normalMatrix));
	
	result.normal = mul(normalMatrix, input.Normal);
	result.worldPos = mul(pc.model, float4(input.Position.xyz, 1.0f));
	return result;
}



[[vk::binding(1, 0)]]
Texture2D g_texture : register(t0);
[[vk::binding(1, 0)]]
SamplerState g_sampler : register(s0);


float4 calcLight(PSInput input, Light l)
{
	//Ambiant
	float4 ambiantLight = float4(l.color * l.ambiant, 1.0f);
	
	float3 light_vec;
	
	if (l.type == DIRLIGHT)
		light_vec = normalize(-l.position); //position is used as direction in dirlight
	else	
		light_vec = normalize(l.position - input.worldPos.xyz);
	
	
	float3 norm = normalize(input.normal);
	
	//Diffuse
	float diffuse = max(dot(light_vec, norm), 0.0f);
	float4 diffuseLight = float4(l.color * (diffuse * mat_diffuse), 1.0f);
	
	//Specular
	float3 view_vec = normalize(pc.eye - input.worldPos.xyz);
	float3 reflect_vec = reflect(-light_vec, norm);
	float specular = pow(max(dot(view_vec, reflect_vec), 0.0f), mat_shininess * 128.0f);
	float4 specularLight = float4(l.color * specular * l.specularStrength, 1.0f);
	
	float attenuation = 1.0f;
	
	if(l.type == POINTLIGHT)
	{
		float d = length(l.position - input.worldPos.xyz);
		attenuation = 1.0f / (l.constant + d * l.linear + d * d * l.quadratic);

	}
	
	return (ambiantLight + diffuseLight + specularLight) * attenuation;
	
}

float4 PSMain(PSInput input) : SV_TARGET
{
	float4 texColor = g_texture.Sample(g_sampler, input.uv) * float4(input.color, 0);
	float4 output = 0;
	
	for (int i = 0; i < pc.light_count ; i++)
	{
		output += texColor * calcLight(input, light[i]);
	}
	
	return output;
}