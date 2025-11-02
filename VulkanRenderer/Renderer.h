#pragma once

#include "Device.h"
#include "ResourceManager.h"

#include <filesystem>

class Renderer {

private:
	Device m_device;
	ResourceManager m_resourceManager{ &m_device };

public:
	void init(GLFWwindow* window, DeviceOptions options);
	void cleanup();
	void waitIdle();

	Device& getDevice() { return m_device; };

	struct CameraInfo {
		float position[3] = { 0.0f, 0.0f, 0.0f };

		float yaw;
		float pitch;

		float forward[3];
		float up[3];

		bool freecam;
		float zoom = 1.0f;
	};


	enum class LightType {
		Point, Directional, Spotlight
	};

	struct Light {
		float position[3] = { 0.0f, 0.0f, 0.0f };
		float color[3] = { 1.0f, 1.0f, 1.0f };

		float ambiant = 0.1f;
		float diffuse = 1.0f;
		float specular = 0.5;
		float shininess = 32;

		LightType type;

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

		bool light_autorotate;
		MeshPacket cube;
	};


	//Pipeline createPipeline(PipelineDesc desc);
	//Pipeline createComputePipeline(PipelineDesc desc);
	//RenderPass createRenderPassAndPipeline(RenderPassDesc renderPassDesc, PipelineDesc pipelineDesc);
	//void setRenderPass(RenderPass renderPass);
	//void setNextRenderPass(RenderPass renderPass);
	//void addRenderPass(RenderPass& renderPass);
	//MeshPacket createPacket(Mesh& mesh, Texture& tex);
	//void drawPacket(MeshPacket packet);
	//void destroyPipeline(Pipeline pipeline);
	//void destroyRenderPass(RenderPass renderPass);

	//void bindVertexBuffer(Buffer& buffer);
	//void drawCommand(uint32_t vertex_count);




private:
	const uint32_t WIDTH = 800;
	const uint32_t HEIGHT = 600;

	DeviceOptions device_options;
	RenderPass drawParticlesPass;

	std::vector<RenderPass> renderPasses;


	Pipeline computePipeline;
	VkDescriptorPool computeDescriptorPool;

	std::vector<Buffer> particleStorageBuffers;

	GpuImageHandle defaultTexture;
	GpuImageHandle defaultTextureBlack;
	GpuImageHandle defaultNormalMap;
	VkSampler defaultSampler;

	std::vector<MeshPacket> packets;
	std::vector<Light> lights;

	CameraInfo cameraInfo;

	//Draw callbacks
	void drawRenderPass();
	void drawParticles();
	void drawLightsRenderPass();
	void drawRenderPassPBR();

	double lastTime;
	double lastFrameTime;

	void initPipeline();
	void initComputePipeline();
	void initTestPipeline();
	void initTestPipeline2();

	void initPipelinePBR();

	void initDrawLightsRenderPass();

	void initParticlesBuffers();
	void cleanupParticles();

	void updateUniformBuffer();
	void updateComputeUniformBuffer();
	void updateLightData();

public:

	void newImGuiFrame();
	void draw();
	void drawImgui();

	void updateCamera(const CameraInfo& cameraInfo);


	MeshPacket createPacket(std::filesystem::path path, std::string texture_path = "");
	MeshPacket createPacket(const Mesh& mesh, const std::vector<GpuImageHandle>& textures);
	MeshPacket createCubePacket(const float pos[3], float scale);
	MeshPacket createConePacket(const float pos[3], float scale);
	MeshPacket createSpherePacket(const float pos[3], float scale);
	void loadScene(std::filesystem::path path);
	void addPacket(const MeshPacket& packet);
	void drawPacket(const MeshPacket& packet);
	void destroyPacket(MeshPacket packet);
	void destroyAllPackets();

	MeshPacket& addSphere(const float pos[3], float scale);

	void addLight(const float pos[3]);
	void addDirectionalLight(const float direction[3]);
	void addSpotlight(const float position[3]);

	const GpuImageHandle& getDefaultTexture() { return defaultTexture; };
	const GpuImageHandle& getDefaultTextureBlack() { return defaultTextureBlack; };
	const GpuImageHandle& getDefaultNormalMap() { return defaultNormalMap; };

	void createDefaultTextures();

};