#include "Renderer.h"


#include <chrono>
#include <random>

#include <imgui.h>

void Renderer::init(GLFWwindow* window, DeviceOptions options)
{
	device_options = options;
	m_device.init(window, options);

	initPipeline();
	initComputePipeline();

	initParticlesBuffers();

	currentDrawPassPtr = device_options.usesMsaa ? &renderPassMsaa : &renderPass;
	lastTime = glfwGetTime();
}

void Renderer::cleanup()
{

	for (auto& packet : packets)
	{
		destroyPacket(packet);
	}


	cleanupParticles();

	m_device.destroyRenderPass(renderPass);
	m_device.destroyRenderPass(renderPassMsaa);
	m_device.destroyPipeline(computePipeline);
	m_device.destroyRenderPass(drawParticlesPass);

}

void Renderer::waitIdle()
{
	m_device.waitIdle();
}

void Renderer::newImGuiFrame()
{
	m_device.newImGuiFrame();
}

void Renderer::draw()
{
	m_device.dispatchCompute(computePipeline);
	m_device.beginDraw();

	m_device.recordRenderPass(*currentDrawPassPtr);
	m_device.recordRenderPass(drawParticlesPass);
	//for (const auto& renderPass : renderPasses)
	//{
	//	recordRenderPass(commandBuffer, renderPass);
	//}


	m_device.recordImGui();

	m_device.endDraw();

	if (nextRenderPassPtr.has_value()) {
		currentDrawPassPtr = nextRenderPassPtr.value();
		nextRenderPassPtr.reset();
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
			if (ImGui::TreeNode(p.name.c_str()))
			{
				ImGui::SliderFloat3("Translate", p.transform.translation, 0.0f, 1.0f);
				ImGui::SliderFloat3("Rot", p.transform.rotation, 0.0f, 1.0f);
				ImGui::SliderFloat3("Scale", p.transform.scale, 0.0f, 4.0f);

				ImGui::TreePop();
			}
		}
	}
}

void Renderer::drawRenderPass() {
	for (const auto& packet : packets)
	{
		drawPacket(packet);
	}
}

void Renderer::initPipeline() 
{
	auto attributeDescriptions = Vertex::getAttributeDescriptions();

	PipelineDesc desc = {
		.type = PipelineType::Graphics,
		.vertexShader = "./shaders/basic.vert.spv",
		.pixelShader = "./shaders/basic.frag.spv",

		.attributeDescriptions = attributeDescriptions.data(),
		.attributeDescriptionsCount = attributeDescriptions.size(),

		.blendMode = BlendMode::Opaque,
		.topology = PrimitiveToplogy::TriangleList,
		.bindings = {
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
		.pushConstantsRanges = {
			{
				.offset = 0,
				.size = sizeof(MeshPacket::PushConstantsData),
				.stageFlags = e_Vertex
			}
		}
	};

	RenderPassDesc renderPassDesc = {
		.colorAttachement_count = 1,
		.hasDepth = true,
		.useMsaa = false,
		.doClear = true,
		.drawFunction = [&]() { drawRenderPass(); }
	};

	renderPass = m_device.createRenderPassAndPipeline(renderPassDesc, desc);

	renderPassDesc.useMsaa = true;
	renderPassMsaa = m_device.createRenderPassAndPipeline(renderPassDesc, desc);

	m_device.setRenderPass(device_options.usesMsaa ? renderPassMsaa : renderPass);
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
			.computeShader = "./shaders/particles.comp.spv",

			.bindings = {
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
			},
		};

		computePipeline = m_device.createComputePipeline(desc);
		computePipeline.descriptorPool = m_device.createDescriptorPool(desc.bindings.data(), desc.bindings.size());
		m_device.createComputeDescriptorSets(computePipeline);
	}

	{
		auto attributeDescriptions = Particle::getAttributeDescriptions();
		PipelineDesc desc = {
			.type = PipelineType::Graphics,
			.vertexShader = "./shaders/particles.vert.spv",
			.pixelShader = "./shaders/particles.frag.spv",

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
			.drawFunction = [&]() { drawParticles(); }
		};

		drawParticlesPass = m_device.createRenderPassAndPipeline(renderPassDesc, desc);
	}
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
		tex = out_mesh.textures.size() > 0 ? out_mesh.textures[0] : loadTexture("assets/viking_room.png");
	}

	out_packet = m_device.createPacket(out_mesh, tex);

	out_packet.transform = {
		.translation = {0.0f, 0.0f, 0.0f},
		.rotation = {0.0f, 0.0f, 0.0f},
		.scale = {1.0f, 1.0f, 1.0f}
	};

	out_packet.name = path.filename().replace_extension("").string();
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


void Renderer::updateUniformBuffer(float zoom) {
	static auto startTime = std::chrono::high_resolution_clock::now();

	Dimensions dim = m_device.getExtent();

	UniformBufferObject ubo{};
	ubo.view = glm::lookAt(glm::vec3(zoom, zoom, zoom), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
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
