#include "Renderer.h"


#include <chrono>
#include <random>

#include <imgui.h>
#include <ImGuizmo.h>

#include <glm/gtc/type_ptr.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

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

	initPipeline();
	//initComputePipeline();
	//initTestPipeline();
	//initTestPipeline2();

	//initParticlesBuffers();

	initDrawLightsRenderPass();

	currentDrawPassPtr = device_options.usesMsaa ? &renderPassMsaa : &renderPass;
	lastTime = glfwGetTime();
}

void Renderer::cleanup()
{

	for (auto& packet : packets)
	{
		destroyPacket(packet);
	}

	for (auto& pass : renderPasses)
	{
		m_device.destroyRenderPass(pass);
	}


	//cleanupParticles();

	m_device.destroyRenderPass(renderPass);
	m_device.destroyRenderPass(renderPassMsaa);
	//m_device.destroyPipeline(computePipeline);
	//m_device.destroyRenderPass(drawParticlesPass);

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

void Renderer::draw()
{
	updateUniformBuffer();
	updateComputeUniformBuffer();
	updateLightData();

	//m_device.dispatchCompute(computePipeline);
	m_device.beginDraw();


	m_device.recordRenderPass(*currentDrawPassPtr);
	//m_device.recordRenderPass(drawParticlesPass);
	for (auto& renderPass : renderPasses)
	{
		m_device.recordRenderPass(renderPass);
	}

	m_device.recordImGui();



	m_device.endDraw();

	if (nextRenderPassPtr.has_value()) {
		currentDrawPassPtr = nextRenderPassPtr.value();
		nextRenderPassPtr.reset();
	}
}

static uint32_t normal_mode = false;
static uint32_t use_blinn = false;
static int debug_mode = 0;
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
		nextRenderPassPtr = device_options.usesMsaa ? &renderPassMsaa : &renderPass;
	}


	if (ImGui::CollapsingHeader("Object List"))
	{
		ImGui::Checkbox("Use Normal Map", (bool*)&normal_mode);
		ImGui::Checkbox("Use Blinn-Phong", (bool*)&use_blinn);
		ImGui::Combo("Debug Mode", &debug_mode, "None\0Normal\0Tangent\0Binormal\0Normal Map\0Shaded Normal\0");
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


void Renderer::drawRenderPass() {
	m_device.bindRessources(1, {&light_data_gpu, &material_data }, {});
	m_device.pushConstants(cameraInfo.position, sizeof(MeshPacket::PushConstantsData), 3 * sizeof(float), (StageFlags)(e_Pixel | e_Vertex));

	uint32_t count = lights.size();
	m_device.pushConstants(&count, sizeof(MeshPacket::PushConstantsData) + 3 * sizeof(float), sizeof(float), (StageFlags)(e_Pixel | e_Vertex));

	m_device.pushConstants(&normal_mode, sizeof(MeshPacket::PushConstantsData) + 4 * sizeof(float), sizeof(uint32_t), (StageFlags)(e_Pixel | e_Vertex));
	m_device.pushConstants(&debug_mode, sizeof(MeshPacket::PushConstantsData) + 5 * sizeof(float), sizeof(uint32_t), (StageFlags)(e_Pixel | e_Vertex));
	m_device.pushConstants(&use_blinn, sizeof(MeshPacket::PushConstantsData) + 6 * sizeof(float), sizeof(uint32_t), (StageFlags)(e_Pixel | e_Vertex));
	for (const auto& packet : packets)
	{
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
				}
			}
		},
		.pushConstantsRanges = {
			{
				.offset = 0,
				.size = sizeof(MeshPacket::PushConstantsData) + sizeof(float) * 7,
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
		.doClear = true,
		.writeSwapChain = true,
		.drawFunction = [&]() { drawRenderPass(); },
		.debugInfo = {
				.name = "Main Render Pass",
				.color = DebugColor::Blue,
		},
	};

	light_data_gpu = m_device.createUniformBuffer(10 * sizeof(LightData));
	material_data = m_device.createUniformBuffer(sizeof(MaterialData));
	memcpy(material_data.mapped_memory, &materials[EMERALD], sizeof(MaterialData));

	renderPass = m_device.createRenderPassAndPipeline(renderPassDesc, desc);

	renderPassDesc.useMsaa = true;
	renderPassMsaa = m_device.createRenderPassAndPipeline(renderPassDesc, desc);

	m_device.setRenderPass(device_options.usesMsaa ? renderPassMsaa : renderPass);
}

void Renderer::drawLightsRenderPass()
{
	for (const auto& l : lights)
	{
		if (l.cube.vertexBuffer.buffer != VK_NULL_HANDLE)
		{
			m_device.pushConstants((void*)&l.color[0], sizeof(MeshPacket::PushConstantsData), 3 * sizeof(float), (StageFlags)(e_Vertex | e_Pixel));
			m_device.drawPacket(l.cube);
		}
	}
}

void Renderer::initDrawLightsRenderPass()
{
	auto attributeDescriptions = Vertex::getAttributeDescriptions();
	auto bindingDescription = Vertex::getBindingDescription();

	PipelineDesc desc = {
		.type = PipelineType::Graphics,
		.vertexShader = "basic.vert.spv",
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

	renderPasses.push_back(m_device.createRenderPassAndPipeline(renderPassDesc, desc));
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

		computePipeline = m_device.createComputePipeline(desc);
		computePipeline.descriptorPool = m_device.createDescriptorPool(desc.bindings[0].data(), desc.bindings[0].size()); //TODO: reconsider this
		m_device.createComputeDescriptorSets(computePipeline);
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
	
	m_device.createRenderTarget(800,600, test_images[0], false, true);
	m_device.createRenderTarget(800,600, test_images[1], false, true);
	m_device.createDepthTarget(800, 600, test_depth, false);
	defaultSampler = m_device.createTextureSampler(1);

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

	renderPass = m_device.createRenderPassAndPipeline(renderPassDesc, desc);

	renderPasses.push_back(renderPass);

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
		.doClear = false,
		.drawFunction = [&]() { drawTest2(m_device); },
		.debugInfo = {
			.name = "Test Render Pass 2",
			.color = DebugColor::Yellow
		}
	};

	renderPass = m_device.createRenderPassAndPipeline(renderPassDesc, desc);

	renderPasses.push_back(renderPass);

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
		particle.velocity = glm::normalize(glm::vec2(x, y)) * 0.25f;
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

	out_packet = m_device.createPacket(out_mesh, tex.pixels.empty() ? nullptr:&tex);

	out_packet.transform = glm::mat4(1.0f);

	out_packet.name = path.filename().replace_extension("").string();

	//m_device.SetImageName(out_packet.texture.image, (out_packet.name + "/BaseColor").c_str());
	m_device.SetBufferName(out_packet.vertexBuffer.buffer, (out_packet.name + "/VertexBuffer").c_str());
	m_device.SetBufferName(out_packet.indexBuffer.buffer, (out_packet.name + "/IndexBuffer").c_str());

	for (int i = 0; i < out_packet.textures.size();  i++)
	{
		if (out_mesh.textures.size() > i)
		{
			const GpuImage& image = out_packet.textures[i];
			const Texture& t = out_mesh.textures[i];
			m_device.SetImageName(image.image, (out_packet.name + "/" + t.name).c_str());
		}
	}

	return out_packet;
}

void Renderer::loadScene(std::filesystem::path path)
{
	if (path.extension() != ".gltf")
		return;


	Scene out_scene;

	loadGltf(path.string().c_str(), &out_scene);

	for (const Node& node : out_scene.nodes)
	{
		if (const std::vector<Mesh>* primitives = std::get_if<std::vector<Mesh>>(&node.data))
		{
			for (const Mesh& mesh : *primitives)
			{
				MeshPacket packet = m_device.createPacket(mesh, nullptr);

				packet.transform = glm::transpose(node.matrix); // node matrix from gltf is row major

				packet.name = node.name;

				m_device.SetBufferName(packet.vertexBuffer.buffer, (packet.name + "/VertexBuffer").c_str());
				m_device.SetBufferName(packet.indexBuffer.buffer, (packet.name + "/IndexBuffer").c_str());

				for (int i = 0; i < packet.textures.size(); i++)
				{
					if (mesh.textures.size() > i)
					{
						const GpuImage& image = packet.textures[i];
						const Texture& t = mesh.textures[i];
						m_device.SetImageName(image.image, (packet.name + "/" + t.name).c_str());
					}
				}

				addPacket(packet);
			}
		}
	}

}

void Renderer::destroyPacket(MeshPacket packet)
{
	m_device.destroyBuffer(packet.vertexBuffer);
	m_device.destroyBuffer(packet.indexBuffer);

	//m_device.destroyImage(packet.texture);
	m_device.destroySampler(packet.sampler);

	for (GpuImage& tex : packet.textures)
	{
		m_device.destroyImage(tex);
	}
}

void Renderer::destroyAllPackets()
{
	for (auto& packet : packets)
	{
		destroyPacket(packet);
	}

	packets.clear();
}

void Renderer::addPacket(const MeshPacket& packet)
{
	packets.push_back(packet);
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

	ubo.view = glm::lookAt(pos, pos + forward, up);
	ubo.proj = glm::perspective(glm::radians(45.0f), dim.width / (float)dim.height, 0.1f, 20.0f);
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

	Light& l = lights[0];


	LightData data;
	size_t offset = 0;
	for (auto& l : lights)
	{
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
}

void Renderer::updateCamera(const CameraInfo& cameraInfo)
{
	this->cameraInfo = cameraInfo;
}


void Renderer::addLight(const float pos[3])
{
	Light light;
	memcpy(light.position, pos, 3 * sizeof(float));

	light.type = LightType::Point;
	light.params.pointLight.constant = 1.0f;
	light.params.pointLight.linear = 0.14f;
	light.params.pointLight.quadratic = 0.07f;

	light.cube = m_device.createCubePacket(pos, 0.2f);

	lights.push_back(light);
}

void Renderer::addDirectionalLight(const float pos[3])
{
	Light light;
	memcpy(light.position, pos, 3 * sizeof(float));

	light.type = LightType::Directional;

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

	light.cube = m_device.createConePacket(pos, 0.5f);
	lights.push_back(light);
}