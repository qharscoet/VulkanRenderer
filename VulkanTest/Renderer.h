#pragma once

#include "Device.h"

#include <filesystem>

class Renderer {

private:
	Device m_device;

public:
	void init(GLFWwindow* window, DeviceOptions options);
	void cleanup();
	void waitIdle();

	Device& getDevice() { return m_device; };



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

	RenderPass renderPass;
	RenderPass renderPassMsaa;
	RenderPass drawParticlesPass;

	const RenderPass* currentDrawPassPtr;
	std::optional<const RenderPass*> nextRenderPassPtr;


	Pipeline computePipeline;
	VkDescriptorPool computeDescriptorPool;

	std::vector<Buffer> particleStorageBuffers;


	std::vector<MeshPacket> packets;

	//Draw callbacks
	void drawRenderPass();
	void drawParticles();

	double lastTime;
	double lastFrameTime;


public:
	void updateUniformBuffer(float zoom);
	void updateComputeUniformBuffer();

	void newImGuiFrame();
	void draw();
	void drawImgui();

	void initPipeline();
	void initComputePipeline();

	void initParticlesBuffers();
	void cleanupParticles();

	MeshPacket createPacket(std::filesystem::path path, std::string texture_path = "");
	void addPacket(const MeshPacket& packet);
	void drawPacket(const MeshPacket& packet);
	void destroyPacket(MeshPacket packet);

};