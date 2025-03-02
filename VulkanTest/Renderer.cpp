#include "Renderer.h"


#include <chrono>
#include <random>

#include <imgui.h>
#include <ImGuizmo.h>

/* TODOs:
	- Separate UBO from other descriptor sets, or include it in the hashing ?
	- Barriers using subpasses
*/

static UniformBufferObject ubo{};

struct LightData
{
	float ambiantStrenght = 0.1f;
	float specularStrength = 0.5;
	float shininess = 32;
	float pad;

	float pos[3];
	float padding;

	float color[3];
};

static LightData light_data;
static bool light_autorotate;

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

static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::TRANSLATE);
static int lastUsing = 0;

static const float identityMatrix[16] =
{ 1.f, 0.f, 0.f, 0.f,
	0.f, 1.f, 0.f, 0.f,
	0.f, 0.f, 1.f, 0.f,
	0.f, 0.f, 0.f, 1.f };

void EditTransform(float* cameraView, float* cameraProjection, float* matrixTranslation, float* matrixRotation, float* matrixScale, bool editTransformDecomposition)
{
	static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::WORLD);
	static bool useSnap = false;
	static float snap[3] = { 1.f, 1.f, 1.f };
	static float bounds[] = { -0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f };
	static float boundsSnap[] = { 0.1f, 0.1f, 0.1f };
	static bool boundSizing = false;
	static bool boundSizingSnap = false;

	float matrix[16];

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

	ImGuizmo::DecomposeMatrixToComponents(matrix, matrixTranslation, matrixRotation, matrixScale);
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
		for (int i = 0; i < packets.size(); i++)
		{
			MeshPacket& p = packets[i];

			char label[32];
			sprintf(label, "Object %d", i);
			if (ImGui::TreeNode(p.name.empty() ? label : p.name.c_str()))
			{
				ImGui::SliderFloat3("Translate", p.transform.translation, -5.0f, 5.0f);
				ImGui::SliderFloat3("Rot", p.transform.rotation, 0.0f, 1.0f);
				ImGui::SliderFloat3("Scale", p.transform.scale, 0.0f, 4.0f);

				ImGui::TreePop();
			}
		}
	}

	if (ImGui::CollapsingHeader("Light List"))
	{
		ImGui::SliderFloat("Ambiant Strength", &light_data.ambiantStrenght, 0.f, 1.f);
		ImGui::SliderFloat("Specular Strength", &light_data.specularStrength, 0.f, 1.f);
		ImGui::SliderFloat("Shininess", &light_data.shininess, 0.f, 256.f);
		for (int i = 0; i < lights.size(); i++)
		{
			Light& l = lights[i];
			MeshPacket& p = l.cube;

			char label[32];
			sprintf(label, "Light %d", i);
			if (ImGui::TreeNode(p.name.empty() ? label : p.name.c_str()))
			{
				ImGui::Checkbox("Light rotate", &light_autorotate);
				ImGui::SliderFloat3("Translate", l.position, -5.0f, 5.0f);
				ImGui::SliderFloat3("Rot", p.transform.rotation, 0.0f, 1.0f);
				ImGui::SliderFloat3("Scale", p.transform.scale, 0.0f, 4.0f);

				ImGui::ColorEdit3("Color", l.color);

				memcpy(p.transform.translation, l.position,  3 * sizeof(float));


				ImGui::TreePop();
			}
		}
	}

	if (ImGui::CollapsingHeader("Test guizmo"))
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

			auto t = packets[matId].transform;

			float* translate = packet.transform.translation;
			float* rotation = packet.transform.rotation;
			float* scale = packet.transform.scale;

	
		
			EditTransform(&ubo.view[0][0], &m[0][0], translate, rotation, scale, lastUsing == matId);
			if (ImGuizmo::IsUsing())
			{
				lastUsing = matId;
			}
		}

	}
}

Buffer light_data_gpu;
void Renderer::drawRenderPass() {
	m_device.bindBuffer(light_data_gpu, 1, 0);
	m_device.pushConstants(cameraInfo.position, sizeof(MeshPacket::PushConstantsData), 3 * sizeof(float), (StageFlags)(e_Pixel | e_Vertex));
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
				{
					.slot = 0,
					.type = BindingType::UBO,
					.stageFlags = e_Vertex,
				},
				{
					.slot = 1,
					.type = BindingType::ImageSampler,
					.stageFlags = e_Pixel,
				}
			},
			{
				{
					.slot = 0,
					.type = BindingType::UBO,
					.stageFlags = e_Pixel,
				},
			}
		},
		.pushConstantsRanges = {
			{
				.offset = 0,
				.size = sizeof(MeshPacket::PushConstantsData) + sizeof(float) * 3,
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
	renderPass = m_device.createRenderPassAndPipeline(renderPassDesc, desc);

	renderPassDesc.useMsaa = true;
	renderPassMsaa = m_device.createRenderPassAndPipeline(renderPassDesc, desc);

	m_device.setRenderPass(device_options.usesMsaa ? renderPassMsaa : renderPass);
}

void Renderer::drawLightsRenderPass()
{
	for (const auto& l : lights)
	{
		m_device.pushConstants((void*)&l.color[0], sizeof(MeshPacket::PushConstantsData), 3 * sizeof(float), (StageFlags)(e_Vertex | e_Pixel));
		m_device.drawPacket(l.cube);
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
	}
	else if (path.extension() == ".gltf")
	{
		loadGltf(path.string().c_str(), &out_mesh);
		if (out_mesh.textures.size() > 0)
			tex = out_mesh.textures[0];
	}

	out_packet = m_device.createPacket(out_mesh, tex.pixels.empty() ? nullptr:&tex);

	out_packet.transform = {
		.translation = {0.0f, 0.0f, 0.0f},
		.rotation = {0.0f, 0.0f, 0.0f},
		.scale = {1.0f, 1.0f, 1.0f}
	};

	out_packet.name = path.filename().replace_extension("").string();

	m_device.SetImageName(out_packet.texture.image, (out_packet.name + "/BaseColor").c_str());
	m_device.SetBufferName(out_packet.vertexBuffer.buffer, (out_packet.name + "/VertexBuffer").c_str());
	m_device.SetBufferName(out_packet.indexBuffer.buffer, (out_packet.name + "/IndexBuffer").c_str());

	return out_packet;
}


void Renderer::destroyPacket(MeshPacket packet)
{
	m_device.destroyBuffer(packet.vertexBuffer);
	m_device.destroyBuffer(packet.indexBuffer);

	m_device.destroyImage(packet.texture);
	m_device.destroySampler(packet.sampler);
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

	if (light_autorotate)
	{
		l.position[0] = 2.0f * cos(angle);
		l.position[2] = 2.0f * sin(angle);

		memcpy(l.cube.transform.translation, l.position, 3 * sizeof(float));
	}

	memcpy(&light_data.pos, l.position, 3*sizeof(float));
	memcpy(&light_data.color, l.color, 3*sizeof(float));

	memcpy(light_data_gpu.mapped_memory, &light_data, sizeof(light_data));
}

void Renderer::updateCamera(const CameraInfo& cameraInfo)
{
	this->cameraInfo = cameraInfo;
}


void Renderer::addLight(const float pos[3])
{
	Light light;
	memcpy(light.position, pos, 3 * sizeof(float));
	light.cube = m_device.createCubePacket(pos, 0.5f);

	lights.push_back(light);
}