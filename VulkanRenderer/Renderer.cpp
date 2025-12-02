#include "Renderer.h"


#include <chrono>
#include <random>

#include <imgui.h>
#include <ImGuizmo.h>

#include <glm/gtc/type_ptr.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

#include "ResourceManager.h"

/* TODOs:
	- Separate UBO from other descriptor sets, or include it in the hashing ?
	- Barriers using subpasses
*/

static UniformBufferObject ubo{};

struct LightData
{
	float ambiantStrenght = 0.1f;
	float diffuse = 1.0f;
	float specularStrength = 0.5;
	float shininess = 32;

	float pos[3];
	uint32_t type;

	union {
		struct {
			float constant;
			float linear;
			float quadratic;
			float pad;
		} pointLight;
		struct {
			float direction[3];
			float cutOff;
		} spotLight;
	} params;

	float color[3];
	float pad;
};

Buffer light_data_gpu;
Buffer material_data;

struct MaterialData
{
	float diffuse[4];
	float specular[4];
	float shininess;
};

enum MaterialType {
	EMERALD, JADE, OBSIDIAN, PEARL, RUBY, TURQUOISE,
	BRASS, BRONZE, CHROME, COPPER, GOLD, SILVER,
	BLACK_PLASTIC, CYAN_PLASTIC, GREEN_PLASTIC, RED_PLASTIC, WHITE_PLASTIC, YELLOW_PLASTIC,
	BLACK_RUBBER, CYAN_RUBBER, GREEN_RUBBER, RED_RUBBER, WHITE_RUBBER, YELLOW_RUBBER,
	MATERIAL_COUNT
};

const char* materialNames[MATERIAL_COUNT] = {
	"Emerald", "Jade", "Obsidian", "Pearl", "Ruby", "Turquoise",
	"Brass", "Bronze", "Chrome", "Copper", "Gold", "Silver",
	"Black Plastic", "Cyan Plastic", "Green Plastic", "Red Plastic", "White Plastic", "Yellow Plastic",
	"Black Rubber", "Cyan Rubber", "Green Rubber", "Red Rubber", "White Rubber", "Yellow Rubber"
};

MaterialData materials[MATERIAL_COUNT] = {
	{ {0.07568f, 0.61424f, 0.07568f}, {0.633f, 0.727811f, 0.633f}, 0.6f }, // EMERALD
	{ {0.54f, 0.89f, 0.63f}, {0.316228f, 0.316228f, 0.316228f}, 0.1f }, // JADE
	{ {0.18275f, 0.17f, 0.22525f}, {0.332741f, 0.328634f, 0.346435f}, 0.3f }, // OBSIDIAN
	{ {1.0f, 0.829f, 0.829f}, {0.296648f, 0.296648f, 0.296648f}, 0.088f }, // PEARL
	{ {0.61424f, 0.04136f, 0.04136f}, {0.727811f, 0.626959f, 0.626959f}, 0.6f }, // RUBY
	{ {0.396f, 0.74151f, 0.69102f}, {0.297254f, 0.30829f, 0.306678f}, 0.1f }, // TURQUOISE
	{ {0.780392f, 0.568627f, 0.113725f}, {0.992157f, 0.941176f, 0.807843f}, 0.21794872f }, // BRASS
	{ {0.714f, 0.4284f, 0.18144f}, {0.393548f, 0.271906f, 0.166721f}, 0.2f }, // BRONZE
	{ {0.4f, 0.4f, 0.4f}, {0.774597f, 0.774597f, 0.774597f}, 0.6f }, // CHROME
	{ {0.7038f, 0.27048f, 0.0828f}, {0.256777f, 0.137622f, 0.086014f}, 0.1f }, // COPPER
	{ {0.75164f, 0.60648f, 0.22648f}, {0.628281f, 0.555802f, 0.366065f}, 0.4f }, // GOLD
	{ {0.50754f, 0.50754f, 0.50754f}, {0.508273f, 0.508273f, 0.508273f}, 0.4f }, // SILVER
	{ {0.01f, 0.01f, 0.01f}, {0.50f, 0.50f, 0.50f}, 0.25f }, // BLACK_PLASTIC
	{ {0.0f, 0.50980392f, 0.50980392f}, {0.50196078f, 0.50196078f, 0.50196078f}, 0.25f }, // CYAN_PLASTIC
	{ {0.1f, 0.35f, 0.1f}, {0.45f, 0.55f, 0.45f}, 0.25f }, // GREEN_PLASTIC
	{ {0.5f, 0.0f, 0.0f}, {0.7f, 0.6f, 0.6f}, 0.25f }, // RED_PLASTIC
	{ {0.55f, 0.55f, 0.55f}, {0.70f, 0.70f, 0.70f}, 0.25f }, // WHITE_PLASTIC
	{ {0.5f, 0.5f, 0.0f}, {0.60f, 0.60f, 0.50f}, 0.25f }, // YELLOW_PLASTIC
	{ {0.01f, 0.01f, 0.01f}, {0.4f, 0.4f, 0.4f}, 0.078125f }, // BLACK_RUBBER
	{ {0.4f, 0.5f, 0.5f}, {0.04f, 0.7f, 0.7f}, 0.078125f }, // CYAN_RUBBER
	{ {0.4f, 0.5f, 0.4f}, {0.04f, 0.7f, 0.04f}, 0.078125f }, // GREEN_RUBBER
	{ {0.5f, 0.4f, 0.4f}, {0.7f, 0.04f, 0.04f}, 0.078125f }, // RED_RUBBER
	{ {0.5f, 0.5f, 0.5f}, {0.7f, 0.7f, 0.7f}, 0.078125f }, // WHITE_RUBBER
	{ {0.5f, 0.5f, 0.4f}, {0.7f, 0.7f, 0.04f}, 0.078125f }  // YELLOW_RUBBER
};


void Renderer::init(GLFWwindow* window, DeviceOptions options)
{
	device_options = options;
	m_device.init(window, options);

	light_data_gpu = m_device.createUniformBuffer(10 * sizeof(LightData));

	createDefaultTextures();

	initPipeline();
	initPipelinePBR();
	initDrawLightsRenderPass();
	initSkyboxRenderPass();

	initDrawShadowMapRenderPass();
	initDrawPointShadowMapRenderPass();

	initComputePipeline();
	initComputeSkyboxPasses();
	//initTestPipeline();
	//initTestPipeline2();

	initParticlesBuffers();


	lastTime = glfwGetTime();
}

void Renderer::cleanup()
{

	destroyAllPackets();

	for (auto& pass : renderPasses)
	{
		m_device.destroyRenderPass(pass);
	}


	cleanupParticles();
	m_device.destroyComputePass(computeParticlesPass);
	m_device.destroyRenderPass(drawParticlesPass);

}

void Renderer::waitIdle()
{
	m_device.waitIdle();
}

void Renderer::newImGuiFrame()
{
	m_device.newImGuiFrame();
	ImGuizmo::BeginFrame();
}



static uint32_t normal_mode = true;
static uint32_t use_blinn = true;
static uint32_t use_pbr = false;
static uint32_t use_ibl = false;
static int debug_mode = 0;

void Renderer::draw()
{
	updateUniformBuffer();
	updateComputeUniformBuffer();
	updateLightData();

	//m_device.recordComputePass(computeParticlesPass);

	static bool computedSky = false;
	if (!computedSky)
	{
		m_device.recordComputePass(computeSkyboxPass);
		m_device.recordComputePass(computeIBLPass);
		m_device.recordComputePass(computeIBLSpecularPass);
		m_device.recordComputePass(computeBRDFLUTPass);
		computedSky = true;
	}

	m_device.beginDraw();

	m_device.recordRenderPass(renderPasses[(size_t)RenderPasses::DrawShadowMap]);
	m_device.recordRenderPass(renderPasses[(size_t)RenderPasses::DrawPointShadowMap]);
	m_device.recordRenderPass(renderPasses[(size_t)RenderPasses::DrawSkybox]);
	m_device.recordRenderPass(use_pbr ? renderPasses[(size_t)RenderPasses::MainPBR] : renderPasses[(size_t)RenderPasses::Main]);
	m_device.recordRenderPass(use_pbr ? renderPasses[(size_t)RenderPasses::MainAlphaPBR] : renderPasses[(size_t)RenderPasses::MainAlpha]);
	m_device.recordRenderPass(renderPasses[(size_t)RenderPasses::DrawLightsRenderPass]);
	//m_device.recordRenderPass(drawParticlesPass);
	//m_device.recordRenderPass(drawParticlesPass);
	//for (auto& renderPass : renderPasses)
	//{
	//	m_device.recordRenderPass(renderPass);
	//}

	m_device.recordImGui();



	m_device.endDraw();
}

static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::TRANSLATE);
static int lastUsing = 0;

static const float identityMatrix[16] =
{ 1.f, 0.f, 0.f, 0.f,
	0.f, 1.f, 0.f, 0.f,
	0.f, 0.f, 1.f, 0.f,
	0.f, 0.f, 0.f, 1.f };

void EditTransform(float* cameraView, float* cameraProjection, float* matrix, bool editTransformDecomposition)
{
	static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::WORLD);
	static bool useSnap = false;
	static float snap[3] = { 1.f, 1.f, 1.f };
	static float bounds[] = { -0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f };
	static float boundsSnap[] = { 0.1f, 0.1f, 0.1f };
	static bool boundSizing = false;
	static bool boundSizingSnap = false;

	

	if (editTransformDecomposition)
	{
		//if (ImGui::IsKeyPressed(90))
		//	mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
		//if (ImGui::IsKeyPressed(69))
		//	mCurrentGizmoOperation = ImGuizmo::ROTATE;
		//if (ImGui::IsKeyPressed(82)) // r Key
		//	mCurrentGizmoOperation = ImGuizmo::SCALE;
		if (ImGui::RadioButton("Translate", mCurrentGizmoOperation == ImGuizmo::TRANSLATE))
			mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
		ImGui::SameLine();
		if (ImGui::RadioButton("Rotate", mCurrentGizmoOperation == ImGuizmo::ROTATE))
			mCurrentGizmoOperation = ImGuizmo::ROTATE;
		ImGui::SameLine();
		if (ImGui::RadioButton("Scale", mCurrentGizmoOperation == ImGuizmo::SCALE))
			mCurrentGizmoOperation = ImGuizmo::SCALE;


		float matrixTranslation[3], matrixRotation[3], matrixScale[3];
		ImGuizmo::DecomposeMatrixToComponents(matrix, matrixTranslation, matrixRotation, matrixScale);

		ImGui::InputFloat3("Tr", matrixTranslation);
		ImGui::InputFloat3("Rt", matrixRotation);
		ImGui::InputFloat3("Sc", matrixScale);

		ImGuizmo::RecomposeMatrixFromComponents(matrixTranslation, matrixRotation, matrixScale, matrix);

		if (mCurrentGizmoOperation != ImGuizmo::SCALE)
		{
			if (ImGui::RadioButton("Local", mCurrentGizmoMode == ImGuizmo::LOCAL))
				mCurrentGizmoMode = ImGuizmo::LOCAL;
			ImGui::SameLine();
			if (ImGui::RadioButton("World", mCurrentGizmoMode == ImGuizmo::WORLD))
				mCurrentGizmoMode = ImGuizmo::WORLD;
		}
		//if (ImGui::IsKeyPressed(83))
		//	useSnap = !useSnap;
		ImGui::Checkbox("snap", &useSnap);
		ImGui::SameLine();

		switch (mCurrentGizmoOperation)
		{
		case ImGuizmo::TRANSLATE:
			ImGui::InputFloat3("Snap", &snap[0]);
			break;
		case ImGuizmo::ROTATE:
			ImGui::InputFloat("Angle Snap", &snap[0]);
			break;
		case ImGuizmo::SCALE:
			ImGui::InputFloat("Scale Snap", &snap[0]);
			break;
		}
		ImGui::Checkbox("Bound Sizing", &boundSizing);
		if (boundSizing)
		{
			ImGui::PushID(3);
			ImGui::Checkbox("", &boundSizingSnap);
			ImGui::SameLine();
			ImGui::InputFloat3("Snap", boundsSnap);
			ImGui::PopID();
		}
	}

	ImGuiIO& io = ImGui::GetIO();
	float viewManipulateRight = io.DisplaySize.x;
	float viewManipulateTop = 0;
	if (false)//useWindow)
	{
		ImGui::SetNextWindowSize(ImVec2(800, 400));
		ImGui::SetNextWindowPos(ImVec2(400, 20));
		ImGui::PushStyleColor(ImGuiCol_WindowBg, (ImVec4)ImColor(0.35f, 0.3f, 0.3f));
		ImGui::Begin("Gizmo", 0, ImGuiWindowFlags_NoMove);
		ImGuizmo::SetDrawlist();
		float windowWidth = (float)ImGui::GetWindowWidth();
		float windowHeight = (float)ImGui::GetWindowHeight();
		ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, windowWidth, windowHeight);
		viewManipulateRight = ImGui::GetWindowPos().x + windowWidth;
		viewManipulateTop = ImGui::GetWindowPos().y;
	}
	else
	{
		ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
	}

	ImGuizmo::DrawGrid(cameraView, cameraProjection, identityMatrix, 100.f);
	//ImGuizmo::DrawCubes(cameraView, cameraProjection, &objectMatrix[0][0], gizmoCount);
	ImGuizmo::Manipulate(cameraView, cameraProjection, mCurrentGizmoOperation, mCurrentGizmoMode, matrix, NULL, useSnap ? &snap[0] : NULL, boundSizing ? bounds : NULL, boundSizingSnap ? boundsSnap : NULL);

	//ImGuizmo::ViewManipulate(cameraView, camDistance, ImVec2(viewManipulateRight - 128, viewManipulateTop), ImVec2(128, 128), 0x10101010);

	if (false)//useWindow)
	{
		ImGui::End();
		ImGui::PopStyleColor(1);
	}
}

void Renderer::drawImgui()
{
	if (ImGui::Checkbox("MSAA", &device_options.usesMsaa)) {
		m_device.setUsesMsaa(device_options.usesMsaa);
	}

	ImGui::Checkbox("Use Normal Map", (bool*)&normal_mode);
	ImGui::Checkbox("Use Blinn-Phong", (bool*)&use_blinn);
	ImGui::Checkbox("Use PBR", (bool*)&use_pbr);
	if (use_pbr)
	{
		ImGui::Checkbox("Use IBL", (bool*)&use_ibl);
	}
	ImGui::Combo("Debug Mode", &debug_mode, "None\0Normal\0Tangent\0Binormal\0Normal Map\0Shaded Normal\0");

	if (ImGui::CollapsingHeader("Object List"))
	{
			for (int i = 0; i < packets.size(); i++)
		{
			MeshPacket& p = packets[i];

			glm::vec3 scale;
			glm::quat rotationQuat;
			glm::vec3 translation;
			glm::vec3 skew;
			glm::vec4 perspective;
			glm::decompose(p.transform, scale, rotationQuat, translation, skew, perspective);

			glm::vec3 eulerRotationDegrees = glm::degrees(glm::eulerAngles(rotationQuat));

			char label[32];
			sprintf(label, "Object %d", i);
			if (ImGui::TreeNode(p.name.empty() ? label : p.name.c_str()))
			{
				ImGui::SliderFloat3("Translate", &translation[0], -5.0f, 5.0f);
				ImGui::SliderFloat3("Rot", &eulerRotationDegrees[0], 0.0f, 180.0f);
				ImGui::SliderFloat3("Scale",&scale[0], 0.0f, 4.0f);

				ImGui::TreePop();
			}

			rotationQuat = glm::quat(glm::radians(eulerRotationDegrees));
			p.transform = glm::recompose(scale, rotationQuat, translation, skew, perspective);
		}
	}

	static int selectedMaterial = 0;
	ImGui::Text("Select Material:");
	if (ImGui::Combo("##Material", &selectedMaterial, materialNames, MATERIAL_COUNT))
		memcpy(material_data.mapped_memory, &materials[selectedMaterial], sizeof(MaterialData));
		
	if (ImGui::CollapsingHeader("Light List"))
	{
		for (int i = 0; i < lights.size(); i++)
		{
			Light& l = lights[i];
			MeshPacket& p = l.cube;

			char label[32];
			sprintf(label, "Light %d", i);
			if (ImGui::TreeNode(label))
			{
				switch (l.type)
				{
				case LightType::Directional: ImGui::Text("Directional light"); break;
				case LightType::Point: ImGui::Text("Point light"); break;
				case LightType::Spotlight: ImGui::Text("Spotlight"); break;
				}

				ImGui::Checkbox("Light rotate", &l.light_autorotate);

				ImGui::SliderFloat3(l.type == LightType::Directional ? "Direction" :"Translate", l.position, -5.0f, 5.0f);
				/*if(l.type == LightType::Spotlight)
					ImGui::SliderFloat3("Rot", p.transform.rotation, 0.0f, 1.0f);*/
				//ImGui::SliderFloat3("Scale", p.transform.scale, 0.0f, 4.0f);

				ImGui::SliderFloat("Ambiant Strength", &l.ambiant, 0.f, 1.f);
				ImGui::SliderFloat("Diffuse Strength", &l.diffuse, 0.f, 1.f);
				ImGui::SliderFloat("Specular Strength", &l.specular, 0.f, 1.f);
				ImGui::SliderFloat("Shininess", &l.shininess, 0.f, 256.f);

				if (l.type == LightType::Point)
				{
					ImGui::Text("Attenuation params:");
					ImGui::SliderFloat("Constant", &l.params.pointLight.constant, 0.f, 1.f);
					ImGui::SliderFloat("Linear", &l.params.pointLight.linear, 0.f, 1.f);
					ImGui::SliderFloat("Quadratic", &l.params.pointLight.quadratic, 0.f, 1.f);
				}
				else if (l.type == LightType::Spotlight)
				{
					ImGui::Text("Params");
					ImGui::SliderFloat3("Direction", l.params.spotLight.direction, 0.0f, 1.0f);
					ImGui::SliderFloat("Angle Cutoff", &l.params.spotLight.cutOff, 0.0f, 1.0f);
				}

				ImGui::ColorEdit3("Color", l.color);

				//p.transform = glm::translate(glm::mat4(1.0), glm::make_vec3(&l.position[0]));
				//memcpy(p.transform.translation, l.position,  3 * sizeof(float));


				ImGui::TreePop();
			}
		}
	}

	if ( false)//ImGui::CollapsingHeader("Test guizmo"))
	{
		auto m = ubo.proj;
		m[1][1] *= -1;

		ImGuizmo::SetOrthographic(false);
		if (ImGuizmo::IsUsing())
		{
			ImGui::Text("Using gizmo");
		}
		else
		{
			ImGui::Text(ImGuizmo::IsOver() ? "Over gizmo" : "");
			ImGui::SameLine();
			ImGui::Text(ImGuizmo::IsOver(ImGuizmo::TRANSLATE) ? "Over translate gizmo" : "");
			ImGui::SameLine();
			ImGui::Text(ImGuizmo::IsOver(ImGuizmo::ROTATE) ? "Over rotate gizmo" : "");
			ImGui::SameLine();
			ImGui::Text(ImGuizmo::IsOver(ImGuizmo::SCALE) ? "Over scale gizmo" : "");
		}

		ImGui::Separator();
		for (int matId = 0; matId <  packets.size(); matId++)
		{
			ImGuizmo::SetID(matId);

			MeshPacket& packet = packets[matId];

			float* matrix = &packets[matId].transform[0][0];
		
			EditTransform(&ubo.view[0][0], &m[0][0], matrix, lastUsing == matId);
			if (ImGuizmo::IsUsing())
			{
				lastUsing = matId;
			}
		}

	}
}


void Renderer::sortTransparentPackets()
{
	glm::vec3 camPos = glm::make_vec3(cameraInfo.position);
	std::sort(transparent_packets.begin(), transparent_packets.end(),
		[camPos](const MeshPacket& a, const MeshPacket& b) {
			glm::vec3 aPos = glm::vec3(a.transform[3]);
			glm::vec3 bPos = glm::vec3(b.transform[3]);
			float distA = glm::length(camPos - aPos);
			float distB = glm::length(camPos - bPos);
			return distA > distB; // Sort in descending order (farthest first)
		});
}

void Renderer::drawRenderPass(const std::vector<MeshPacket>& packets) {
	const ImageBindInfo shadowMapBindInfo = ImageBindInfo{ shadowMap->view, *defaultSampler };
	const ImageBindInfo depthShadowMapBindInfo = ImageBindInfo{ pointShadowMap->view, *defaultSampler };
	m_device.bindRessources(1, {&light_data_gpu, &material_data, sunViewProj.get()}, {shadowMapBindInfo, depthShadowMapBindInfo});
	m_device.pushConstants(cameraInfo.position, sizeof(MeshPacket::PushConstantsData), 3 * sizeof(float), (StageFlags)(e_Pixel | e_Vertex));

	uint32_t count = lights.size();
	m_device.pushConstants(&count, sizeof(MeshPacket::PushConstantsData) + 3 * sizeof(float), sizeof(float), (StageFlags)(e_Pixel | e_Vertex));

	m_device.pushConstants(&normal_mode, sizeof(MeshPacket::PushConstantsData) + 4 * sizeof(float), sizeof(uint32_t), (StageFlags)(e_Pixel | e_Vertex));
	m_device.pushConstants(&debug_mode, sizeof(MeshPacket::PushConstantsData) + 5 * sizeof(float), sizeof(uint32_t), (StageFlags)(e_Pixel | e_Vertex));
	m_device.pushConstants(&use_blinn, sizeof(MeshPacket::PushConstantsData) + 6 * sizeof(float), sizeof(uint32_t), (StageFlags)(e_Pixel | e_Vertex));
	for (const auto& packet : packets)
	{
		const ImageBindInfo baseColor = packet.getTextureBindInfo(MeshPacket::TextureType::BaseColor, getDefaultTexture(), defaultSampler);
		const ImageBindInfo normal = packet.getTextureBindInfo(MeshPacket::TextureType::Normal, getDefaultNormalMap(), defaultSampler);

		//const ImageResourceBindInfo bindInfo = ImageSampler(GET_PACKET_IMAGESAMPLER_PAIR(packet, normal, getDefaultNormalMap(), *defaultSampler));
		m_device.bindRessources(0, {&m_device.getCurrentUniformBuffer()}, {baseColor , normal});

		float alphaCutoff = packet.materialData.getAlphaCutoff();
		m_device.pushConstants(&alphaCutoff, sizeof(MeshPacket::PushConstantsData) + 7 * sizeof(float), sizeof(float), (StageFlags)(e_Pixel | e_Vertex));
		drawPacket(packet);
	}
}

void Renderer::initPipeline() 
{
	auto attributeDescriptions = Vertex::getAttributeDescriptions();
	auto bindingDescription = Vertex::getBindingDescription();

	PipelineDesc desc = {
		.type = PipelineType::Graphics,
		.vertexShader = "phong.vs.spv",
		.pixelShader = "phong.ps.spv",

		.bindingDescription = &bindingDescription,
		.attributeDescriptions = attributeDescriptions.data(),
		.attributeDescriptionsCount = attributeDescriptions.size(),

		.blendMode = BlendMode::Opaque,
		.topology = PrimitiveToplogy::TriangleList,
		.bindings = {
			{
				// View & proj Matrices
				{
					.slot = 0,
					.type = BindingType::UBO,
					.stageFlags = e_Vertex,
				},
				//Single texture
				{
					.slot = 1,
					.type = BindingType::ImageSampler,
					.stageFlags = e_Pixel,
				},
				// Normal
				{
					.slot = 2,
					.type = BindingType::ImageSampler,
					.stageFlags = e_Pixel,
				}
			},
			{
				//Light Data
				{
					.slot = 0,
					.type = BindingType::UBO,
					.stageFlags = e_Pixel,
				},
				//Material data
				{
					.slot = 1,
					.type = BindingType::UBO,
					.stageFlags = e_Pixel,
				},
				// Sun View Projection
				{
					.slot = 2,
					.type = BindingType::UBO,
					.stageFlags = e_Vertex,
				},
				//Shadow map
				{
					.slot = 3,
					.type = BindingType::ImageSampler,
					.stageFlags = e_Pixel,
				},
				// Depth Shadow Map
				{
					.slot = 4,
					.type = BindingType::ImageSampler,
					.stageFlags = e_Pixel,
				}
			}
		},
		.pushConstantsRanges = {
			{
				.offset = 0,
				.size = sizeof(MeshPacket::PushConstantsData) + sizeof(float) * 8,
				.stageFlags = (StageFlags)(e_Vertex | e_Pixel)
			},
			//{
			//	.offset = sizeof(MeshPacket::PushConstantsData),
			//	.size = ,
			//	.stageFlags = (StageFlags)(e_Pixel | e_Vertex)
			//}
		}
	};

	RenderPassDesc renderPassDesc = {
		.colorAttachement_count = 1,
		.hasDepth = true,
		.useMsaa = false,
		.doClear = false,
		.writeSwapChain = true,
		.drawFunction = [&]() { drawRenderPass(packets); },
		.debugInfo = {
				.name = "Main Render Pass",
				.color = DebugColor::Blue,
		},
	};

	material_data = m_device.createUniformBuffer(sizeof(MaterialData));
	memcpy(material_data.mapped_memory, &materials[EMERALD], sizeof(MaterialData));

	renderPasses[(size_t)RenderPasses::Main] = m_device.createRenderPassAndPipeline(renderPassDesc, desc);


	desc.blendMode = BlendMode::AlphaBlend;
	renderPassDesc.doClear = false;
	renderPassDesc.debugInfo.name = "Transparent Render Pass";
	renderPassDesc.drawFunction = [&]() { sortTransparentPackets(); drawRenderPass(transparent_packets); };
	renderPasses[(size_t)RenderPasses::MainAlpha] = m_device.createRenderPassAndPipeline(renderPassDesc, desc);
}

void Renderer::drawLightsRenderPass()
{
	for (const auto& l : lights)
	{
		const MeshPacket& packet = l.cube;
		if (l.cube.vertexBuffer != nullptr)
		{
			const ImageBindInfo baseColor = packet.getTextureBindInfo(MeshPacket::TextureType::BaseColor, getDefaultTexture(), defaultSampler);
			const ImageBindInfo normal = packet.getTextureBindInfo(MeshPacket::TextureType::Normal, getDefaultNormalMap(), defaultSampler);
			m_device.bindRessources(0, { &m_device.getCurrentUniformBuffer() }, { baseColor , normal });

			m_device.pushConstants((void*)&l.color[0], sizeof(MeshPacket::PushConstantsData), 3 * sizeof(float), (StageFlags)(e_Vertex | e_Pixel));
			m_device.drawPacket(packet);
		}
	}
}

void Renderer::initDrawLightsRenderPass()
{
	auto attributeDescriptions = Vertex::getAttributeDescriptions();
	auto bindingDescription = Vertex::getBindingDescription();

	PipelineDesc desc = {
		.type = PipelineType::Graphics,
		.vertexShader = "basic.slang.spv",
		.pixelShader = "basic.ps.spv",

		.bindingDescription = &bindingDescription,
		.attributeDescriptions = attributeDescriptions.data(),
		.attributeDescriptionsCount = attributeDescriptions.size(),

		.blendMode = BlendMode::Opaque,
		.topology = PrimitiveToplogy::TriangleList,
		.bindings = {
			{
				{
					.slot = 0,
					.type = BindingType::UBO,
					.stageFlags = e_Vertex,
				},
				{
					.slot = 1,
					.type = BindingType::ImageSampler,
					.stageFlags = e_Pixel,
				},
				{
					.slot = 2,
					.type = BindingType::ImageSampler,
					.stageFlags = e_Pixel,
				}
			}
		},
		.pushConstantsRanges = {
			{
				.offset = 0,
				.size = sizeof(MeshPacket::PushConstantsData) + 3 * sizeof(float), // + light color
				.stageFlags = (StageFlags)(e_Vertex | e_Pixel)
			}
		}
	};

	RenderPassDesc renderPassDesc = {
		.colorAttachement_count = 1,
		.hasDepth = true,
		.useMsaa = device_options.usesMsaa,
		.doClear = false,
		.writeSwapChain = true,
		.drawFunction = [&]() { drawLightsRenderPass(); },
		.debugInfo = {
				.name = "Draw light Cubes Render Pass",
				.color = DebugColor::Blue,
		},
	};

	renderPasses[(size_t)RenderPasses::DrawLightsRenderPass] = m_device.createRenderPassAndPipeline(renderPassDesc, desc);
}

void Renderer::initComputeSkyboxPasses() {
	{
		PipelineDesc desc = {
			.type = PipelineType::Compute,
			.computeShader = "equi_to_cubemap.slang.spv",
			.bindings = {
				{
					{
						.slot = 0,
						.type = BindingType::StorageImage,
						.stageFlags = e_Compute,
					},
					{
						.slot = 1,
						.type = BindingType::ImageSampler,
						.stageFlags = e_Compute,
					},
				}
			},
		};

		ComputePassDesc computePassDesc = {
				.dispatchFunction = [&]() {
					BarrierDesc transitionBarrier = {
						.image = resultCubemap.get(),
						.oldLayout = ImageLayout::Undefined,
						.newLayout = ImageLayout::General,
						.mipLevels = resultCubemap->mipLevels,
						.layerCount = 6,
					};
					m_device.transitionImage(transitionBarrier, PipelineType::Compute);

					const ImageBindInfo resultCubeWrite = { resultCubemap->writeViews[0] };
					const ImageBindInfo equiRectangular = { equirectangularTexture->view, *defaultSampler };
					m_device.bindRessources(0, {}, { resultCubeWrite, equiRectangular  }, PipelineType::Compute);

					m_device.dispatchCommand(
						(uint32_t)std::ceil(resultCubemap->width / 32.0f),
						(uint32_t)std::ceil(resultCubemap->height / 32.0f),
						6);

					m_device.generateMipmaps(*resultCubemap, PipelineType::Compute);

					transitionBarrier.oldLayout = ImageLayout::General;
					transitionBarrier.newLayout = ImageLayout::ShaderRead;
					m_device.transitionImage(transitionBarrier, PipelineType::Compute);
				},
				.debugInfo = {
					.name = "Compute Skybox from Equirectangular",
					.color = DebugColor::Magenta
				}
		};

		computeSkyboxPass = m_device.createComputePass(computePassDesc, desc);
	}

	{
		PipelineDesc desc = {
			.type = PipelineType::Compute,
			.computeShader = "compute_ibl.slang.spv",
			.bindings = {
				{
					{
						.slot = 0,
						.type = BindingType::StorageImage,
						.stageFlags = e_Compute,
					},
					{
						.slot = 1,
						.type = BindingType::ImageSampler,
						.stageFlags = e_Compute,
					},
				}
			},
		};

		ComputePassDesc computePassDesc = {
				.dispatchFunction = [&]() {
					BarrierDesc transitionBarrier = {
						.image = irradianceMap.get(),
						.oldLayout = ImageLayout::Undefined,
						.newLayout = ImageLayout::General,
						.mipLevels = 1,
						.layerCount = 6,
					};
					m_device.transitionImage(transitionBarrier, PipelineType::Compute);

					const ImageBindInfo irradianceWrite = { irradianceMap->writeViews[0] };
					const ImageBindInfo envMap = { resultCubemap->view, *defaultSampler };
					m_device.bindRessources(0, {}, { irradianceWrite, envMap }, PipelineType::Compute);
					m_device.dispatchCommand(
						(uint32_t)std::ceil(irradianceMap->width / 32.0f),
						(uint32_t)std::ceil(irradianceMap->height / 32.0f),
						6);

					transitionBarrier.oldLayout = ImageLayout::General;
					transitionBarrier.newLayout = ImageLayout::ShaderRead;

					m_device.transitionImage(transitionBarrier, PipelineType::Compute);
				},
				.debugInfo = {
					.name = "Compute Diffuse IBL",
					.color = DebugColor::Magenta
				}
		};
	
		computeIBLPass = m_device.createComputePass(computePassDesc, desc);
	}

	{
		PipelineDesc desc = {
			.type = PipelineType::Compute,
			.computeShader = "compute_ibl_specular.slang.spv",
			.bindings = {
				{
					{
						.slot = 0,
						.type = BindingType::StorageImage,
						.stageFlags = e_Compute,
					},
					{
						.slot = 1,
						.type = BindingType::ImageSampler,
						.stageFlags = e_Compute,
					},
				}
			},
			.pushConstantsRanges = {
				{
					.offset = 0,
					.size = 2 * sizeof(float), // roughness + mip
					.stageFlags = (StageFlags)(e_Compute)
				}
			}
		};

		ComputePassDesc computePassDesc = {
				.dispatchFunction = [&]() {			
					BarrierDesc transitionBarrier = {
						.image = specularMap.get(),
						.oldLayout = ImageLayout::Undefined,
						.newLayout = ImageLayout::General,
						.mipLevels = specularMap->mipLevels,
						.layerCount = 6,
					};
					m_device.transitionImage(transitionBarrier, PipelineType::Compute);


					for (uint32_t i = 0; i < specularMap->mipLevels; i++)
					{
						const ImageBindInfo specularWrite = { specularMap->writeViews[i] };
						const ImageBindInfo envMap = { resultCubemap->view, *defaultSampler };
						m_device.bindRessources(0, {}, { specularWrite, envMap }, PipelineType::Compute);

						float roughness = (float)i / specularMap->mipLevels;
						m_device.pushConstants(&roughness, 0, sizeof(float), e_Compute);
						m_device.pushConstants(&i, sizeof(float), sizeof(float), e_Compute);

						float w = specularMap->width >> i;
						float h = specularMap->height >> i;
						m_device.dispatchCommand(
							(uint32_t)std::ceil(w / 32.0f),
							(uint32_t)std::ceil(h / 32.0f),
							6);
					}

					transitionBarrier.oldLayout = ImageLayout::General;
					transitionBarrier.newLayout = ImageLayout::ShaderRead;
					m_device.transitionImage(transitionBarrier, PipelineType::Compute);
				},
				.debugInfo = {
					.name = "Compute Specular IBL",
					.color = DebugColor::Magenta
				}
		};

		computeIBLSpecularPass = m_device.createComputePass(computePassDesc, desc);
	}

	{
		PipelineDesc desc = {
			.type = PipelineType::Compute,
			.computeShader = "compute_brdf_lut.slang.spv",
			.bindings = {
				{
					{
						.slot = 0,
						.type = BindingType::StorageImage,
						.stageFlags = e_Compute,
					},
				}
			},
		};

		ComputePassDesc computePassDesc = {
				.dispatchFunction = [&]() {
					BarrierDesc transitionBarrier = {
						.image = BRDF_LUT.get(),
						.oldLayout = ImageLayout::Undefined,
						.newLayout = ImageLayout::General,
						.mipLevels = BRDF_LUT->mipLevels,
						.layerCount = 1,
					};
					m_device.transitionImage(transitionBarrier, PipelineType::Compute);

					m_device.bindRessources(0, {}, { {BRDF_LUT.get()->writeViews[0]} }, PipelineType::Compute);
					m_device.dispatchCommand(
						(uint32_t)std::ceil(BRDF_LUT->width / 32.0f),
						(uint32_t)std::ceil(BRDF_LUT->height / 32.0f),
						1);


					transitionBarrier.oldLayout = ImageLayout::General;
					transitionBarrier.newLayout = ImageLayout::ShaderRead;
					m_device.transitionImage(transitionBarrier, PipelineType::Compute);
				},
				.debugInfo = {
					.name = "Compute BRDF LUT",
					.color = DebugColor::Magenta
				}
		};

		computeBRDFLUTPass = m_device.createComputePass(computePassDesc, desc);
	}
}


void Renderer::initSkyboxRenderPass()
{
	PipelineDesc desc = {
		.type = PipelineType::Graphics,
		.vertexShader = "skybox.vs.spv",
		.pixelShader = "skybox.ps.spv",

		.blendMode = BlendMode::Opaque,
		.topology = PrimitiveToplogy::TriangleList,
		.depthCompareOp = DepthCompareOp::LessEqual,
		.bindings = {
			{
				{
					.slot = 0,
					.type = BindingType::UBO,
					.stageFlags = e_Vertex,
				},
				{
					.slot = 1,
					.type = BindingType::ImageSampler,
					.stageFlags = e_Pixel,
				},
			}
		},
	};

	RenderPassDesc renderPassDesc = {
		.colorAttachement_count = 1,
		.hasDepth = true,
		.useMsaa = device_options.usesMsaa,
		.doClear = true,
		.writeSwapChain = true,
		.drawFunction = [&]() { 
				m_device.bindRessources(0, { &m_device.getCurrentUniformBuffer() }, {{ specularMap->view, *defaultSampler}});
				m_device.drawCommand(36);
		; },
		.debugInfo = {
			.name = "Skybox",
			.color = DebugColor::Cyan
		}
	};


	renderPasses[(size_t)RenderPasses::DrawSkybox] = m_device.createRenderPassAndPipeline(renderPassDesc, desc);
}

void Renderer::initDrawShadowMapRenderPass()
{
	auto attributeDescriptions = Vertex::getAttributeDescriptions();
	auto bindingDescription = Vertex::getBindingDescription();

	shadowMap = m_resourceManager.createTexture<DeviceTextureType::DepthTarget>(1024, 1024, false, false, true);
	sunViewProj = m_resourceManager.createBuffer<DeviceBufferType::Uniform>(sizeof(UniformBufferObject));

	PipelineDesc desc = {
		.type = PipelineType::Graphics,
		.vertexShader = "empty.slang.spv",
		.pixelShader = "empty.slang.spv",

		.bindingDescription = &bindingDescription,
		.attributeDescriptions = attributeDescriptions.data(),
		.attributeDescriptionsCount = attributeDescriptions.size(),

		.blendMode = BlendMode::Opaque,
		.cullMode = CullMode::Front,
		.topology = PrimitiveToplogy::TriangleList,
		.bindings = {
			{
				{
					.slot = 0,
					.type = BindingType::UBO,
					.stageFlags = e_Vertex,
				},
			}
		},
		.pushConstantsRanges = {
			{
				.offset = 0,
				.size = sizeof(MeshPacket::PushConstantsData),
				.stageFlags = (StageFlags)(e_Vertex | e_Pixel)
			}
		}
	};

	RenderPassDesc renderPassDesc = {
		.framebufferDesc = {
			.images = {},
			.depth = shadowMap.get(),
			.width = 1024,
			.height = 1024,
		},
		.colorAttachement_count = 0,
		.hasDepth = true,
		.useMsaa = false,
		.doClear = true,
		.drawFunction = [&]() { 
			m_device.bindRessources(0, { sunViewProj.get()}, {});
			for (const auto& packet : packets)
			{
				drawPacket(packet);
			}	
		},
		.postDrawBarriers = {
			{
				.image = shadowMap.get(),
				.oldLayout = ImageLayout::DepthTarget,
				.newLayout = ImageLayout::ShaderRead,
				.mipLevels = 1
			}
		},
		.debugInfo = {
			.name = "Draw Shadow Map",
			.color = DebugColor::Grey,
		}
	};

	renderPasses[(size_t)RenderPasses::DrawShadowMap] = m_device.createRenderPassAndPipeline(renderPassDesc, desc);
}

void Renderer::initDrawPointShadowMapRenderPass()
{
	pointShadowMap = m_resourceManager.createTexture<DeviceTextureType::DepthTarget>(1024, 1024, false, true, true);
	pointLightViewProj = m_resourceManager.createBuffer<DeviceBufferType::Uniform>(sizeof(glm::mat4) * 6 + sizeof(float) * 4); // 6 viewproj + light pos + far plane

	auto attributeDescriptions = Vertex::getAttributeDescriptions();
	auto bindingDescription = Vertex::getBindingDescription();

	PipelineDesc desc = {
		.type = PipelineType::Graphics,
		.vertexShader = "point_shadow.slang.spv",
		.pixelShader = "point_shadow.slang.spv",
		.geometryShader = "point_shadow.slang.spv",

		.bindingDescription = &bindingDescription,
		.attributeDescriptions = attributeDescriptions.data(),
		.attributeDescriptionsCount = attributeDescriptions.size(),

		.blendMode = BlendMode::Opaque,
		.cullMode = CullMode::Front,
		.topology = PrimitiveToplogy::TriangleList,
		.bindings = {
			{
				{
					.slot = 0,
					.type = BindingType::UBO,
					.stageFlags = e_Geometry | e_Pixel,
				},
			}
		},
		.pushConstantsRanges = {
			{
				.offset = 0,
				.size = sizeof(MeshPacket::PushConstantsData),
				.stageFlags = (StageFlags)(e_Vertex | e_Pixel | e_Geometry)
			}
		}
	};

	RenderPassDesc renderPassDesc = {
		.framebufferDesc = {
			.images = {},
			.depth = pointShadowMap.get(),
			.width = 1024,
			.height = 1024,
			.layers = 6,
		},
		.colorAttachement_count = 0,
		.hasDepth = true,
		.useMsaa = false,
		.doClear = true,
		.drawFunction = [&]() {
			m_device.bindRessources(0, { pointLightViewProj.get()}, {});
			for (const auto& packet : packets)
			{
				drawPacket(packet);
			}
		},
		.postDrawBarriers = {
			{
				.image = pointShadowMap.get(),
				.oldLayout = ImageLayout::DepthTarget,
				.newLayout = ImageLayout::ShaderRead,
				.mipLevels = 1,
				.layerCount = 6,
			}
		},
		.debugInfo = {
			.name = "Draw Point Shadow Map",
			.color = DebugColor::Grey,
		}
	};

	renderPasses[(size_t)RenderPasses::DrawPointShadowMap] = m_device.createRenderPassAndPipeline(renderPassDesc, desc);
}

void Renderer::drawRenderPassPBR(const std::vector<MeshPacket>& packets) {
	const ImageBindInfo irradiance = { irradianceMap->view , *defaultSampler};
	const ImageBindInfo specular = { specularMap->view , *defaultSampler};
	const ImageBindInfo brdf = { BRDF_LUT->view , *defaultSampler };
	const ImageBindInfo shadowMapBindInfo = ImageBindInfo{ shadowMap->view, *defaultSampler };
	const ImageBindInfo depthShadowMapBindInfo = ImageBindInfo{ pointShadowMap->view, *defaultSampler };
	m_device.bindRessources(1, { &light_data_gpu, sunViewProj.get()}, {irradiance, specular, brdf, shadowMapBindInfo, depthShadowMapBindInfo});

	uint32_t count = lights.size();
	size_t start_offset = sizeof(MeshPacket::PushConstantsData);

	m_device.pushConstants(cameraInfo.position, start_offset, 3 * sizeof(float), (StageFlags)(e_Vertex | e_Pixel));
	m_device.pushConstants(&count, start_offset + 3 * sizeof(float), sizeof(float), (StageFlags)(e_Vertex | e_Pixel));
	m_device.pushConstants(&normal_mode, start_offset + 4 * sizeof(float), sizeof(uint32_t), (StageFlags)(e_Vertex | e_Pixel));
	m_device.pushConstants(&debug_mode, start_offset + 5 * sizeof(float), sizeof(uint32_t), (StageFlags)(e_Vertex | e_Pixel));
	m_device.pushConstants(&use_ibl, start_offset + 6 * sizeof(float), sizeof(uint32_t), (StageFlags)(e_Vertex | e_Pixel));

	start_offset += 7 * sizeof(float) + sizeof(float); // 2 is padding
	for (const auto& packet : packets)
	{
		const ImageBindInfo baseColor = packet.getTextureBindInfo(MeshPacket::TextureType::BaseColor, getDefaultTexture(), defaultSampler);
		const ImageBindInfo normal = packet.getTextureBindInfo(MeshPacket::TextureType::Normal, getDefaultNormalMap(), defaultSampler);
		const ImageBindInfo  mettalicRoughness = packet.getTextureBindInfo(MeshPacket::TextureType::MetallicRoughness, getDefaultTexture(), defaultSampler);
		const ImageBindInfo  occlusion = packet.getTextureBindInfo(MeshPacket::TextureType::Occlusion, getDefaultTexture(), defaultSampler);
		const ImageBindInfo  emissive = packet.getTextureBindInfo(MeshPacket::TextureType::Emissive, getDefaultTextureBlack(), defaultSampler);
		m_device.bindRessources(0, { &m_device.getCurrentUniformBuffer() }, { baseColor, normal, mettalicRoughness, occlusion, emissive });

		float alphaCutoff = packet.materialData.getAlphaCutoff();

		m_device.pushConstants(&packet.materialData.pbrFactors, start_offset, sizeof(Mesh::Material::PBRFactors), (StageFlags)(e_Vertex | e_Pixel));
		m_device.pushConstants(&alphaCutoff, start_offset + sizeof(Mesh::Material::PBRFactors), sizeof(float), (StageFlags)(e_Vertex | e_Pixel));
		drawPacket(packet);
	}
}

void Renderer::initPipelinePBR()
{
	auto attributeDescriptions = Vertex::getAttributeDescriptions();
	auto bindingDescription = Vertex::getBindingDescription();

	PipelineDesc desc = {
		.type = PipelineType::Graphics,
		.vertexShader = "pbr.slang.spv",
		.pixelShader = "pbr.slang.spv",

		.bindingDescription = &bindingDescription,
		.attributeDescriptions = attributeDescriptions.data(),
		.attributeDescriptionsCount = attributeDescriptions.size(),

		.blendMode = BlendMode::Opaque,
		.topology = PrimitiveToplogy::TriangleList,
		.bindings = {
			{
				// View & proj Matrices
				{
					.slot = 0,
					.type = BindingType::UBO,
					.stageFlags = e_Vertex,
				},
				//Base Color
				{
					.slot = 1,
					.type = BindingType::ImageSampler,
					.stageFlags = e_Pixel,
				},
				// Normal
				{
					.slot = 2,
					.type = BindingType::ImageSampler,
					.stageFlags = e_Pixel,
				},
				// MetallicRoughness
				{
					.slot = 3,
					.type = BindingType::ImageSampler,
					.stageFlags = e_Pixel,
				},
				// Occlusion
				{
					.slot = 4,
					.type = BindingType::ImageSampler,
					.stageFlags = e_Pixel,
				},
				// Emissive
				{
					.slot = 5,
					.type = BindingType::ImageSampler,
					.stageFlags = e_Pixel,
				}
			},
			{
				//Light Data
				{
					.slot = 0,
					.type = BindingType::UBO,
					.stageFlags = e_Pixel,
				},
				//Irradiance map
				{
					.slot = 1,
					.type = BindingType::ImageSampler,
					.stageFlags = e_Pixel,
				},
				//Specular prefiltered map
				{
					.slot = 2,
					.type = BindingType::ImageSampler,
					.stageFlags = e_Pixel,
				},
				// BRDF LUT
				{
					.slot = 3,
					.type = BindingType::ImageSampler,
					.stageFlags = e_Pixel,
				},
				// Sun View Projection
				{
					.slot = 4,
					.type = BindingType::UBO,
					.stageFlags = e_Vertex,
				},
				//Shadow map
				{
					.slot = 5,
					.type = BindingType::ImageSampler,
					.stageFlags = e_Pixel,
				},
				// Depth Shadow Map
				{
					.slot = 6,
					.type = BindingType::ImageSampler,
					.stageFlags = e_Pixel,
				}
			}
		},
		.pushConstantsRanges = {
			{
				.offset = 0,
				.size = sizeof(MeshPacket::PushConstantsData) + sizeof(float) * 7 + sizeof(Mesh::Material::PBRFactors) + 8 /*padding*/,
				.stageFlags = (StageFlags)(e_Vertex | e_Pixel)
			}
		}
	};

	RenderPassDesc renderPassDesc = {
		.colorAttachement_count = 1,
		.hasDepth = true,
		.useMsaa = false,
		.doClear = false,
		.writeSwapChain = true,
		.drawFunction = [&]() { drawRenderPassPBR(packets); },
		.debugInfo = {
				.name = "Main Render Pass PBR",
				.color = DebugColor::Blue,
		},
	};

	renderPasses[(size_t)RenderPasses::MainPBR] = m_device.createRenderPassAndPipeline(renderPassDesc, desc);

	desc.blendMode = BlendMode::AlphaBlend;
	renderPassDesc.doClear = false;
	renderPassDesc.debugInfo.name = "Main Render Pass PBR Alpha Blend";
	renderPassDesc.drawFunction = [&]() { drawRenderPassPBR(transparent_packets); };
	renderPasses[(size_t)RenderPasses::MainAlphaPBR] = m_device.createRenderPassAndPipeline(renderPassDesc, desc);
}


void Renderer::updateParticles()
{
	const uint32_t current_frame = m_device.getCurrentFrame();
	const uint32_t last_frame = (current_frame + 1) % m_device.getMaxFramesInFlight();

	m_device.bindRessources(0, { &m_device.getCurrentComputeUniformBuffer(), &particleStorageBuffers[last_frame], &particleStorageBuffers[current_frame] }, {}, PipelineType::Compute);
	m_device.dispatchCommand(PARTICLE_COUNT / 256, 1, 1);
}

void Renderer::drawParticles()
{
	m_device.bindVertexBuffer(particleStorageBuffers[m_device.getCurrentFrame()]);
	m_device.drawCommand(PARTICLE_COUNT);
}

void Renderer::initComputePipeline()
{
	{
		PipelineDesc desc = {
			.type = PipelineType::Compute,
			.computeShader = "particles.comp.spv",

			.bindings = {
				{
					{
						.slot = 0,
						.type = BindingType::UBO,
						.stageFlags = e_Compute,
					},
					{
						.slot = 1,
						.type = BindingType::StorageBuffer,
						.stageFlags = e_Compute,
					},
					{
						.slot = 2,
						.type = BindingType::StorageBuffer,
						.stageFlags = e_Compute,
					}
				}
			},
		};

		ComputePassDesc computePassDesc = {
			.dispatchFunction = [&]() { updateParticles(); },
			.debugInfo = {
				.name = "Particle Compute Pass",
				.color = DebugColor::Magenta
			}
		};

		computeParticlesPass = m_device.createComputePass(computePassDesc, desc);
		m_device.createComputeDescriptorSets(computeParticlesPass.pipeline); //TODO FIX
	}

	{
		auto attributeDescriptions = Particle::getAttributeDescriptions();
		auto bindingDescription = Particle::getBindingDescription();

		PipelineDesc desc = {
			.type = PipelineType::Graphics,
			.vertexShader = "particles.vert.spv",
			.pixelShader = "particles.frag.spv",

			.bindingDescription = &bindingDescription,
			.attributeDescriptions = attributeDescriptions.data(),
			.attributeDescriptionsCount = attributeDescriptions.size(),

			.blendMode = BlendMode::Opaque,
			.topology = PrimitiveToplogy::PointList,
			.bindings = {},
			.pushConstantsRanges = {}
		};

		RenderPassDesc renderPassDesc = {
			.colorAttachement_count = 1,
			.hasDepth = true,
			.useMsaa = device_options.usesMsaa,
			.doClear = false,
			.writeSwapChain = true,
			.drawFunction = [&]() { drawParticles(); },
			.debugInfo = {
				.name = "Draw Particles Render Pass",
				.color = DebugColor::Green
			} 
		};

		drawParticlesPass = m_device.createRenderPassAndPipeline(renderPassDesc, desc);
	}
}




static GpuImage test_images[2];
static GpuImage test_depth;
static VkSampler defaultSampler;

void drawTest(Device& device)
{
	auto c = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
	device.pushConstants(&c, 0,  sizeof(glm::vec4));
	device.drawCommand(3);
}

void Renderer::initTestPipeline()
{
	auto attributeDescriptions = Vertex::getAttributeDescriptions();

	PipelineDesc desc = {
		.type = PipelineType::Graphics,
		.vertexShader = "fill.vert.spv",
		.pixelShader = "fill.frag.spv",

		.blendMode = BlendMode::Opaque,
		.topology = PrimitiveToplogy::TriangleList,
		.bindings = {
		},
		.pushConstantsRanges = {
			{
				.offset = 0,
				.size = sizeof(float[4]), //size of vec4 color
				.stageFlags = e_Vertex
			}
		}
	};
	
	m_device.createRenderTarget(test_images[0], 800, 600, false, true);
	m_device.createRenderTarget(test_images[1], 800,600 , false, true);
	m_device.createDepthTarget(test_depth, 800, 600, false,false);
	//defaultSampler = m_device.createTextureSampler(1);

	RenderPassDesc renderPassDesc = {
		.framebufferDesc = {
			.images = {&test_images[0], &test_images[1]},
			.depth = &test_depth,
			.width = 800,
			.height = 600,
		},
		.colorAttachement_count = 1,
		.hasDepth = true,
		.useMsaa = false,
		.doClear = true,
		.drawFunction = [&]() { drawTest(m_device); },
		.postDrawBarriers = {
			 {
				.image = &test_images[0],
				.oldLayout = ImageLayout::RenderTarget,
				.newLayout = ImageLayout::ShaderRead,
				.mipLevels = 1
			}
		},
		.debugInfo = {
			.name = "Test Render Pass",
			.color = DebugColor::Red
		}
	};

	renderPasses[(size_t)RenderPasses::Test] = m_device.createRenderPassAndPipeline(renderPassDesc, desc);

}


void drawTest2(Device& device)
{
	device.bindTexture(test_images[0], defaultSampler);
	device.drawCommand(4);
}


void Renderer::initTestPipeline2()
{
	auto attributeDescriptions = Vertex::getAttributeDescriptions();

	PipelineDesc desc = {
		.type = PipelineType::Graphics,
		.vertexShader = "blit.vert.spv",
		.pixelShader = "blit.frag.spv",

		.blendMode = BlendMode::Opaque,
		.topology = PrimitiveToplogy::TriangleStrip,
		.bindings = {
			{
				{
					.slot = 0,
					.type = BindingType::ImageSampler,
					.stageFlags = e_Pixel,
				}
			}
		},
	};

	RenderPassDesc renderPassDesc = {
		.colorAttachement_count = 1,
		.hasDepth = true,
		.useMsaa = device_options.usesMsaa,
		.doClear = true,
		.writeSwapChain = true,
		.drawFunction = [&]() { drawTest2(m_device); },
		.debugInfo = {
			.name = "Test Render Pass 2",
			.color = DebugColor::Yellow
		}
	};


	renderPasses[(size_t)RenderPasses::Test2] = m_device.createRenderPassAndPipeline(renderPassDesc, desc);

}




void Renderer::initParticlesBuffers()
{
	particleStorageBuffers.resize(2);
	// Initialize particles
	std::default_random_engine rndEngine((unsigned)time(nullptr));
	std::uniform_real_distribution<float> rndDist(0.0f, 1.0f);

	// Initial particle positions on a circle
	std::vector<Particle> particles(PARTICLE_COUNT);
	for (auto& particle : particles) {
		float r = 0.25f * sqrt(rndDist(rndEngine));
		float theta = rndDist(rndEngine) * 2 * 3.14159265358979323846;
		float x = r * cos(theta) * HEIGHT / WIDTH;
		float y = r * sin(theta);
		particle.position = glm::vec2(x, y);
		particle.velocity = glm::normalize(glm::vec2(x, y)) * 0.025f;
		particle.color = glm::vec4(rndDist(rndEngine), rndDist(rndEngine), rndDist(rndEngine), 1.0f);
	}

	VkDeviceSize bufferSize = sizeof(Particle) * PARTICLE_COUNT;

	//TODO change this to not allocate the staging buffer 3 times
	for (size_t i = 0; i < 2; i++) {
		particleStorageBuffers[i] = m_device.createLocalBuffer(bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, particles.data());
		particleStorageBuffers[i].size = bufferSize;
		particleStorageBuffers[i].stride = sizeof(Particle);
		particleStorageBuffers[i].count = bufferSize / sizeof(Particle);
	}

	//TODO FIX
	m_device.updateComputeDescriptorSets(particleStorageBuffers);
	//m_device.setVertexBuffer(particleStorageBuffers[0]);
}


MeshPacket Renderer::createPacket(std::filesystem::path path, std::string texture_path)
{
	Mesh  out_mesh;
	MeshPacket out_packet;

	Texture tex;
	if (path.extension() == ".obj") {
		loadObj(path.string().c_str(), &out_mesh);
		tex = loadTexture(texture_path.c_str());

		memset(&out_mesh.material, -1, sizeof(out_mesh.material));
	}
	else if (path.extension() == ".gltf")
	{
		loadGltf(path.string().c_str(), &out_mesh);
		//if (out_mesh.textures.size() > 0)
		//	tex = out_mesh.textures[0];
	}


	std::vector<GpuImageHandle> loaded_textures;
	if (!tex.getPixels<unsigned char>().empty())
	{
		loaded_textures.push_back(m_resourceManager.createTexture(tex));
		out_mesh.material.baseColor.texIdx = 0;
		out_mesh.material.baseColor.samplerIdx = -1;
	}
	out_packet = createPacket(out_mesh, loaded_textures, {});

	out_packet.transform = glm::mat4(1.0f);

	out_packet.name = path.filename().replace_extension("").string();

	//m_device.SetImageName(out_packet.texture.image, (out_packet.name + "/BaseColor").c_str());
	m_device.SetBufferName(out_packet.vertexBuffer->buffer, (out_packet.name + "/VertexBuffer").c_str());
	m_device.SetBufferName(out_packet.indexBuffer->buffer, (out_packet.name + "/IndexBuffer").c_str());

	for (int i = 0; i < out_packet.textures.size();  i++)
	{
		if (out_mesh.textures.size() > i)
		{
			const GpuImage& image = *out_packet.textures[i];
			const Texture& t = out_mesh.textures[i];
			m_device.SetImageName(image.image, (out_packet.name + "/" + t.name).c_str());
		}
	}

	return out_packet;
}

void Renderer::loadSkybox(const std::array<const char*, 6>& faces)
{
	const Texture cubeMap = loadCubemapTexture(faces);
	
	skyboxTexture = m_resourceManager.createTexture(cubeMap);
}

void Renderer::loadSkybox(const std::filesystem::path path)
{

	const Texture equirectangular = loadTexture(path.string().c_str());

	equirectangularTexture = m_resourceManager.createTexture(equirectangular);
	resultCubemap = m_resourceManager.createRWTexture(2048, 2048, ImageFormat::RGBA_Float, true, true);
	irradianceMap = m_resourceManager.createRWTexture(32, 32, ImageFormat::RGBA_Float,  true);
	specularMap = m_resourceManager.createRWTexture(1024, 1024, ImageFormat::RGBA_Float,  true, true);
	BRDF_LUT = m_resourceManager.createRWTexture(512, 512, ImageFormat::RG16_Float, false, false);
}

SamplerDesc getSamplerDesc(const SamplerInfo& info) {
	auto toFilter = [](int v) {
		switch (v) {
		case 9728: return FilterMode::Nearest;
		case 9729: return FilterMode::Linear;
		case 9984: return FilterMode::Nearest_MipNearest;
		case 9985: return FilterMode::Linear_MipNearest;
		case 9986: return FilterMode::Nearest_MipLinear;
		case 9987: return FilterMode::Linear_MipLinear;
		default:   return FilterMode::Linear;
		}
		};

	auto toWrap = [](int v) {
		switch (v) {
		case 33071: return WrapMode::Clamp;
		case 10497: return WrapMode::Repeat;
		case 33648: return WrapMode::MirroredRepeat;
		default:    return WrapMode::Repeat;
		}
		};

	SamplerDesc desc;
	desc.magFilter = toFilter(info.magFilter);
	desc.minFilter = toFilter(info.minFilter);
	desc.wrapS = toWrap(info.wrapS);
	desc.wrapT = toWrap(info.wrapT);
	return desc;
}

void Renderer::loadScene(std::filesystem::path path)
{
	if (path.extension() != ".gltf")
		return;


	Scene out_scene;
	loadGltf(path.string().c_str(), &out_scene);

	std::vector<GpuImageHandle> loaded_textures;
	for(const Texture& t : out_scene.textures)
	{
		GpuImageHandle image = m_resourceManager.createTexture(t);
		loaded_textures.push_back(image);
		m_device.SetImageName(image->image, (path.filename().string() + "/" + t.name).c_str());
	}

	std::vector<SamplerHandle> loaded_samplers;
	for (const SamplerInfo& sampl : out_scene.samplers)
	{
		SamplerHandle sampler = m_resourceManager.createSampler(getSamplerDesc(sampl));
		loaded_samplers.push_back(sampler);
	}


	std::function<void(const Node&)>  loadNode = [&](const Node& node) -> void
	{
		if (const std::vector<Mesh>* primitives = std::get_if<std::vector<Mesh>>(&node.data))
		{
			for (const Mesh& mesh : *primitives)
			{

				MeshPacket packet = createPacket(mesh, loaded_textures, loaded_samplers);

				packet.transform = node.matrix;

				packet.name = node.name;

				m_device.SetBufferName(packet.vertexBuffer->buffer, (packet.name + "/VertexBuffer").c_str());
				m_device.SetBufferName(packet.indexBuffer->buffer, (packet.name + "/IndexBuffer").c_str());

				addPacket(packet);
			}
		}

		for(auto child: node.children)
		{
			loadNode(*child);
		}
	};

	for (const Node& node : out_scene.nodes)
	{
		loadNode(node);
	}

}

void Renderer::destroyPacket(MeshPacket packet)
{
	//m_device.destroyBuffer(packet.vertexBuffer);
	//m_device.destroyBuffer(packet.indexBuffer);

	//m_device.destroyImage(packet.texture);
	//m_device.destroySampler(packet.sampler);

	//for (GpuImage& tex : packet.textures)
	//{
	//	m_device.destroyImage(tex);
	//}
}

void Renderer::destroyAllPackets()
{
	for (auto& packet : packets)
	{
		destroyPacket(packet);
	}

	packets.clear();

	for (auto& packet : transparent_packets)
	{
		destroyPacket(packet);
	}

	transparent_packets.clear();
}

void Renderer::addPacket(const MeshPacket& packet)
{
	if (packet.materialData.alphaCoverage.alphaMode == MeshPacket::MaterialData::AlphaCoverage::AlphaMode::Blend)
	{
		transparent_packets.push_back(packet);
	}
	else
	{
		packets.push_back(packet);
	}
}

void Renderer::drawPacket(const MeshPacket& packet)
{
	m_device.drawPacket(packet);
}


void Renderer::cleanupParticles()
{
	for (int i = 0; i < 2; i++)
	{
		m_device.destroyBuffer(particleStorageBuffers[i]);
	}
}


//TODO : get rid of glm and try our hands at a math library
void Renderer::updateUniformBuffer() {
	static auto startTime = std::chrono::high_resolution_clock::now();

	Dimensions dim = m_device.getExtent();

	glm::vec3 pos = glm::vec3(cameraInfo.position[0], cameraInfo.position[1], cameraInfo.position[2]);
	glm::vec3 up = glm::vec3(cameraInfo.up[0], cameraInfo.up[1], cameraInfo.up[2]);
	glm::vec3 forward = glm::vec3(cameraInfo.forward[0], cameraInfo.forward[1], cameraInfo.forward[2]);

	glm::vec3 center = cameraInfo.freecam ? pos + forward : glm::vec3(0, 0, 0);
	ubo.view = glm::lookAt(pos, center, up);
	ubo.proj = glm::perspective(glm::radians(45.0f), dim.width / (float)dim.height, 0.1f, 50.0f);
	ubo.proj[1][1] *= -1;


	m_device.updateUniformBuffer(&ubo, sizeof(UniformBufferObject));
}

void Renderer::updateComputeUniformBuffer()
{
	double currentTime = glfwGetTime();
	lastFrameTime = (currentTime - lastTime);// *1000.0;
	lastTime = currentTime;

	ParticleUBO ubo{};
	ubo.deltaTime = lastFrameTime * 2.0f;
	m_device.updateComputeUniformBuffer(&ubo, sizeof(ParticleUBO));
}

void Renderer::updateLightData()
{
	static auto startTime = std::chrono::high_resolution_clock::now();

	auto currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

	float angle = time;

	Light* sun_ptr = nullptr; //For now the sun is the first directional light
	Light* pointlight_ptr = nullptr;

	LightData data;
	size_t offset = 0;
	for (auto& l : lights)
	{
		if (l.type == LightType::Directional && sun_ptr == nullptr)
			sun_ptr = &l;
		else if (l.type == LightType::Point && pointlight_ptr == nullptr)
			pointlight_ptr = &l;

		glm::vec3 scale;
		glm::quat rotation;
		glm::vec3 translation;
		glm::vec3 skew;
		glm::vec4 perspective;
		glm::decompose(l.cube.transform, scale, rotation, translation, skew, perspective);

		if (l.light_autorotate)
		{
			l.position[0] = 2.0f * cos(angle);
			l.position[2] = 2.0f * sin(angle);

			//memcpy(l.cube.transform.translation, l.position, 3 * sizeof(float));

		}
		if (l.type == LightType::Spotlight)
		{
			glm::vec3 direction = normalize(-glm::make_vec3(l.position));
			memcpy(l.params.spotLight.direction, &direction[0], 3 * sizeof(float));

			glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f); // Default up vector

			//// Compute rotation axis and angle
			glm::vec3 rotationAxis = glm::cross(up, direction);
			float angle = acos(glm::dot(up, direction));

			//// Create quaternion rotation
			glm::quat rotationQuat = glm::angleAxis(angle, glm::normalize(rotationAxis));
			rotation = rotationQuat;

		}

		translation.x = l.position[0];
		translation.y = l.position[1];
		translation.z = l.position[2];
		l.cube.transform = glm::recompose(scale, rotation, translation, skew, perspective);

		memcpy(&data.pos, l.position, 3 * sizeof(float));
		memcpy(&data.color, l.color, 3*sizeof(float));

		data.ambiantStrenght = l.ambiant;
		data.specularStrength = l.specular;
		data.shininess = l.shininess;
		data.diffuse = l.diffuse;

		data.type = static_cast<uint32_t>(l.type);
		memcpy(&data.params, &l.params, sizeof(l.params));

		////TODO : temp override for initial testing
		//if (l.type == LightType::Spotlight)
		//{
		//	memcpy(&data.params.spotLight.direction, &cameraInfo.forward, 3 * sizeof(float));
		//	memcpy(&data.pos, cameraInfo.position, 3 * sizeof(float));
		//	memcpy(l.params.spotLight.direction, &cameraInfo.forward, 3 * sizeof(float));
		//	memcpy(l.position, cameraInfo.position, 3 * sizeof(float));
		//}

		memcpy((uint8_t*)(light_data_gpu.mapped_memory) + offset, &data, sizeof(data));
		offset += sizeof(data);
	}

	if (sun_ptr && sunViewProj->buffer != VK_NULL_HANDLE)
	{
		UniformBufferObject sun_ubo{};
		sun_ubo.proj = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, 0.1f, 20.0f);
		sun_ubo.view = glm::lookAt(
			glm::vec3(sun_ptr->position[0], sun_ptr->position[1], sun_ptr->position[2]),
			glm::vec3(0.0f, 0.0f, 0.0f),
			glm::vec3(0.0f, 1.0f, 0.0f)
		);
		memcpy(sunViewProj->mapped_memory, &sun_ubo, sizeof(UniformBufferObject));
	}

	if (pointlight_ptr && pointLightViewProj->buffer != VK_NULL_HANDLE)
	{
		glm::vec3 lightPos = glm::vec3(pointlight_ptr->position[0], pointlight_ptr->position[1], pointlight_ptr->position[2]);
		
		float farPlane = 25.0f;
		glm::mat4 shadowProj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, farPlane);
		glm::mat4 shadowViews[] =
		{
			shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
			shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
			shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
			shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),
			shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
			shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f))
		};
		size_t view_size = sizeof(shadowViews);
		memcpy(pointLightViewProj->mapped_memory, shadowViews, view_size );
		memcpy((uint8_t*)pointLightViewProj->mapped_memory + view_size, &lightPos[0], 3 * sizeof(float));
		memcpy((uint8_t*)pointLightViewProj->mapped_memory + view_size + 3*sizeof(float), &farPlane, sizeof(float));
	}
}

void Renderer::updateCamera(const CameraInfo& cameraInfo)
{
	this->cameraInfo = cameraInfo;
}


MeshPacket& Renderer::addSphere(const float pos[3], float size)
{
	addPacket(createSpherePacket(pos, size));
	return packets.back();
}

void Renderer::addLight(const float pos[3])
{
	Light light;
	memcpy(light.position, pos, 3 * sizeof(float));

	light.type = LightType::Point;
	light.params.pointLight.constant = 1.0f;
	light.params.pointLight.linear = 0.14f;
	light.params.pointLight.quadratic = 0.07f;

	light.cube = createCubePacket(pos, 0.2f);

	lights.push_back(light);
}

void Renderer::addDirectionalLight(const float pos[3])
{
	Light light;
	memcpy(light.position, pos, 3 * sizeof(float));

	light.type = LightType::Directional;
	light.light_autorotate = false;
	//light.cube = createCubePacket(pos, 0.2f);
	//light.cube = m_device.createCubePacket(pos, 0.5f);

	lights.push_back(light);
}

void Renderer::addSpotlight(const float pos[3])
{
	Light light;
	memcpy(light.position, pos, 3 * sizeof(float));

	light.type = LightType::Spotlight;
	light.color[0] = 1.0f;
	light.color[1] = 1.0f;
	light.color[2] = 0.0f;

	light.params.spotLight.cutOff = glm::cos(glm::radians(12.5f));

	light.cube = createConePacket(pos, 0.5f);
	lights.push_back(light);
}

MeshPacket Renderer::createPacket(const Mesh& mesh, const std::vector<GpuImageHandle>& textures, const std::vector<SamplerHandle>& samplers)
{
	MeshPacket out_packet;
	out_packet.vertexBuffer = m_resourceManager.createVertexBuffer(mesh.vertices.size() * sizeof(mesh.vertices[0]), (void*)mesh.vertices.data());
	out_packet.indexBuffer = m_resourceManager.createIndexBuffer(mesh.indices.size() * sizeof(mesh.indices[0]), (void*)mesh.indices.data());


	memcpy(&out_packet.materialData.pbrFactors, &mesh.material.pbrFactors, sizeof(mesh.material.pbrFactors));


	if(mesh.material.alphaMode == "OPAQUE")
		out_packet.materialData.alphaCoverage.alphaMode = MeshPacket::MaterialData::AlphaCoverage::AlphaMode::Opaque;
	else if(mesh.material.alphaMode == "MASK")
		out_packet.materialData.alphaCoverage.alphaMode = MeshPacket::MaterialData::AlphaCoverage::AlphaMode::Mask;
	else if(mesh.material.alphaMode == "BLEND")
		out_packet.materialData.alphaCoverage.alphaMode = MeshPacket::MaterialData::AlphaCoverage::AlphaMode::Blend;
	else
		out_packet.materialData.alphaCoverage.alphaMode = MeshPacket::MaterialData::AlphaCoverage::AlphaMode::Opaque;

	out_packet.materialData.alphaCoverage.cutoff = mesh.material.alphaCutoff;
	using TextureType = MeshPacket::TextureType;

	const auto setTextureInfo = [&](Mesh::ImageSamplerIndices indices, TextureType dest_type) {
		if (indices.texIdx >= 0)
		{
			out_packet.textures.push_back(textures[indices.texIdx]);
			out_packet.materialData.texturesIdx[dest_type].texIdx = out_packet.textures.size() - 1;

			const SamplerHandle sampler = indices.samplerIdx >= 0 ? samplers[indices.samplerIdx] : defaultSampler;
			out_packet.samplers.push_back(sampler);
			out_packet.materialData.texturesIdx[dest_type].samplerIdx = out_packet.samplers.size() - 1;
		}
	};

	//Base Color needs to have a default texture always set
	if (mesh.material.baseColor.texIdx>= 0)
	{
		setTextureInfo(mesh.material.baseColor, TextureType::BaseColor);
	}
	else
	{
		out_packet.textures.push_back(getDefaultTexture());
		out_packet.materialData.texturesIdx[TextureType::BaseColor].texIdx = out_packet.textures.size() - 1;

		out_packet.samplers.push_back(defaultSampler);
		out_packet.materialData.texturesIdx[TextureType::BaseColor].samplerIdx = out_packet.samplers.size() - 1;
	}

	setTextureInfo(mesh.material.normal, TextureType::Normal);
	setTextureInfo(mesh.material.mettalicRoughness, TextureType::MetallicRoughness);
	setTextureInfo(mesh.material.occlusion, TextureType::Occlusion);
	setTextureInfo(mesh.material.emissive, TextureType::Emissive);


	return out_packet;
}

MeshPacket Renderer::createCubePacket(const float pos[3], float size)
{
	MeshPacket out_packet;

	auto vertices = Vertex::getCubeVertices();
	auto indices = Vertex::getCubeIndices();
	out_packet.vertexBuffer = m_resourceManager.createVertexBuffer(vertices.size() * sizeof(vertices[0]), (void*)vertices.data());
	out_packet.indexBuffer = m_resourceManager.createIndexBuffer(indices.size() * sizeof(indices[0]), (void*)indices.data());


	out_packet.textures.push_back(getDefaultTexture());
	out_packet.samplers.push_back(defaultSampler);
	out_packet.name = "Cube";


	out_packet.transform = glm::translate(glm::mat4(1.0f), glm::vec3(pos[0], pos[1], pos[2]));
	out_packet.transform = glm::scale(out_packet.transform, glm::vec3(size, size, size));

	memset(&out_packet.materialData, -1, sizeof(out_packet.materialData));
	out_packet.materialData.texturesIdx[MeshPacket::TextureType::BaseColor].texIdx = 0;
	out_packet.materialData.texturesIdx[MeshPacket::TextureType::BaseColor].samplerIdx = 0;

	return out_packet;
}

MeshPacket Renderer::createConePacket(const float pos[3], float size)
{
	MeshPacket out_packet;

	auto vertices = Vertex::getConeVertices();
	auto indices = Vertex::getConeIndices();
	out_packet.vertexBuffer = m_resourceManager.createVertexBuffer(vertices.size() * sizeof(vertices[0]), (void*)vertices.data());
	out_packet.indexBuffer = m_resourceManager.createIndexBuffer(indices.size() * sizeof(indices[0]), (void*)indices.data());


	out_packet.textures.push_back(getDefaultTexture());
	out_packet.samplers.push_back(defaultSampler);
	out_packet.name = "Cube";

	out_packet.transform = glm::translate(glm::mat4(1.0f), glm::vec3(pos[0], pos[1], pos[2]));
	out_packet.transform = glm::scale(out_packet.transform, glm::vec3(size, size, size));

	memset(&out_packet.materialData, -1, sizeof(out_packet.materialData));
	out_packet.materialData.texturesIdx[MeshPacket::TextureType::BaseColor].texIdx = 0;
	out_packet.materialData.texturesIdx[MeshPacket::TextureType::BaseColor].samplerIdx = 0;

	return out_packet;
}

MeshPacket Renderer::createSpherePacket(const float pos[3], float size)
{
	MeshPacket out_packet;

	static BufferHandle vertexBuffer;
	static BufferHandle indexBuffer;

	if (vertexBuffer == nullptr)
	{
		auto vertices = Vertex::generateSphereVertices();
		auto indices = Vertex::generateSphereIndices();
		Vertex::ComputeTangents(vertices, indices);
		vertexBuffer = m_resourceManager.createVertexBuffer(vertices.size() * sizeof(vertices[0]), (void*)vertices.data());
		indexBuffer = m_resourceManager.createIndexBuffer(indices.size() * sizeof(indices[0]), (void*)indices.data());
	}


	out_packet.vertexBuffer = vertexBuffer;
	out_packet.indexBuffer = indexBuffer;
	out_packet.textures.push_back(getDefaultTexture());
	out_packet.samplers.push_back(defaultSampler);
	out_packet.name = "Sphere";


	out_packet.transform = glm::translate(glm::mat4(1.0f), glm::vec3(pos[0], pos[1], pos[2]));
	out_packet.transform = glm::scale(out_packet.transform, glm::vec3(size, size, size));

	memset(&out_packet.materialData, -1, sizeof(out_packet.materialData));
	out_packet.materialData.texturesIdx[MeshPacket::TextureType::BaseColor].texIdx = 0;
	out_packet.materialData.texturesIdx[MeshPacket::TextureType::BaseColor].samplerIdx = 0;
	out_packet.materialData.pbrFactors.baseColorFactor[0] = 1.0f;
	out_packet.materialData.pbrFactors.baseColorFactor[1] = 1.0f;
	out_packet.materialData.pbrFactors.baseColorFactor[2] = 1.0f;
	out_packet.materialData.pbrFactors.baseColorFactor[3] = 1.0f;
	out_packet.materialData.pbrFactors.metallicFactor = 1.0f;
	out_packet.materialData.pbrFactors.roughnessFactor = 1.0f;
	out_packet.materialData.pbrFactors.occlusionStrength = 1.0f;


	return out_packet;
}

void Renderer::createDefaultTextures()
{
	std::vector<unsigned char> pixels(128 * 128 * 4, 255);
	Texture tex{
		.height = 128,
		.width = 128,
		.channels = 4,
		.size = pixels.size() * sizeof(unsigned char),
		.pixels = pixels,
	};

	defaultTexture = m_resourceManager.createTexture(tex);
	defaultSampler = m_resourceManager.createSampler({});

	memset(&pixels[0], 0, pixels.size());
	tex = {
		.height = 128,
		.width = 128,
		.channels = 4,
		.size = pixels.size() * sizeof(unsigned char),
		.is_srgb = false,
		.pixels = pixels,
	};

	defaultTextureBlack = m_resourceManager.createTexture(tex);


	// Fill the pixels array with default normal data, (0.0, 1.0, 0.0, 1);
	for (int i = 0; i < pixels.size(); i += 4)
	{
		pixels[i] = 127;
		pixels[i + 1] = 127;
		pixels[i + 2] = 255;
		pixels[i + 3] = 255;
	}

	tex = {
		.height = 128,
		.width = 128,
		.channels = 4,
		.size = pixels.size() * sizeof(unsigned char),
		.is_srgb = false,
		.pixels = pixels,
	};

	defaultNormalMap = m_resourceManager.createTexture(tex);
}