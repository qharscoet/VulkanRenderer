#include <iostream>
#include <stdexcept>

#include <chrono>
#include <random>

#include <filesystem>

#include "FileUtils.h"

/* Optional TODOs :
	- Use the transfer queue for staging buffer
	- Use separate commandPool for copy buffer */



#include "Device.h"

#include "Renderer.h"

#include "imgui.h"

float zoom = 2.0f;
float rotation[3] = { 1.0f, 0.0f, 0.0f };
float translate[3] = { 0.0f, 0.0f, 0.0f };
bool auto_rot = false;

struct GLFW_state {
	bool pressed;
	double mousepress_x;
	double mousepress_y;
	int width;
	int height;
} glfw_state;

void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
	auto app = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
	app->getDevice().framebufferResized = true;

	glfw_state.width = width;
	glfw_state.height = height;

}



void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{

	if (button == GLFW_MOUSE_BUTTON_RIGHT)
	{
		switch (action) {
		case GLFW_PRESS: glfw_state.pressed = true; break;
		case GLFW_RELEASE: glfw_state.pressed = false; break;
		default:break;
		}

		glfwGetCursorPos(window, &glfw_state.mousepress_x, &glfw_state.mousepress_y);
	}
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	zoom -= yoffset * 0.1f;
}

static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
{
	if (glfw_state.pressed)
	{
		rotation[0] += (xpos - glfw_state.mousepress_x)/glfw_state.width;
		rotation[1] += (ypos - glfw_state.mousepress_y)/glfw_state.height;


		glfw_state.mousepress_x = xpos;
		glfw_state.mousepress_y = ypos;
	}

}

DeviceOptions device_options = {
	.usesMsaa = true,
};


class HelloTriangleApplication {

private:
	const uint32_t WIDTH = 800;
	const uint32_t HEIGHT = 600;

	//Device m_device;
	GLFWwindow* window = nullptr;

	Renderer m_renderer;


	const std::vector<MeshVertex> vertices = {
		{.pos = {-0.5f, -0.5f, 0.0f},	.color = {1.0f, 0.0f, 0.0f},	.texCoord = {0.0f, 0.0f} },
		{.pos = {0.5f, -0.5f, 0.0f},	.color = {0.0f, 1.0f, 0.0f},	.texCoord = {1.0f, 0.0f} },
		{.pos = {0.5f, 0.5f, 0.0f},		.color = {0.0f, 0.0f, 1.0f},	.texCoord = {1.0f, 1.0f} },
		{.pos = {-0.5f, 0.5f, 0.0f},	.color = {1.0f, 1.0f, 1.0f},	.texCoord = {0.0f, 1.0f} },

		{.pos = {-0.5f, -0.5f, -0.5f},	.color = {1.0f, 0.0f, 0.0f},	.texCoord = {0.0f, 0.0f} },
		{.pos = {0.5f, -0.5f, -0.5f},	.color = {0.0f, 1.0f, 0.0f},	.texCoord = {1.0f, 0.0f} },
		{.pos = {0.5f, 0.5f, -0.5f},	.color = {0.0f, 0.0f, 1.0f},	.texCoord = {1.0f, 1.0f} },
		{.pos = {-0.5f, 0.5f, -0.5f},	.color = {1.0f, 1.0f, 1.0f},	.texCoord = {0.0f, 1.0f} },
	};

	const std::vector<uint32_t> indices = {
		0, 1, 2, 2, 3, 0,
		4, 5, 6, 6, 7, 4
	};

	void initWindow() {
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
		window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);

		glfwSetWindowUserPointer(window, &m_renderer);
		glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);

		glfwSetMouseButtonCallback(window, mouse_button_callback);
		glfwSetScrollCallback(window, scroll_callback);
		glfwSetCursorPosCallback(window, cursor_position_callback);

		glfw_state.width = WIDTH;
		glfw_state.height = HEIGHT;
	}

	void cleanupWindow() {
		glfwDestroyWindow(window);
		glfwTerminate();
	}


	void loadPackets()
	{
		m_renderer.addPacket(m_renderer.createPacket("assets/viking_room.obj", "assets/viking_room.png"));
		m_renderer.addPacket(m_renderer.createPacket("assets/Cube/Cube.gltf"));
	}


	void drawImGui()
	{
		ImGuiIO& io = ImGui::GetIO();

		static bool show_demo_window = false;
		if (show_demo_window)
			ImGui::ShowDemoWindow(&show_demo_window);

		{

			ImGui::Begin("Params");                          // Create a window called "Hello, world!" and append into it.

			ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state

			ImGui::SliderFloat("Zoom", &zoom, 0.0f, 10.0f);            // Edit 1 float using a slider from 0.0f to
			ImGui::SliderFloat3("Rot", rotation, 0.0f, 4.0f);            // Edit 1 float using a slider from 0.0f to
			ImGui::SliderFloat3("Translate", translate, 0.0f, 1.0f);          // Edit 1 float using a slider from 0.0f to
			ImGui::Checkbox("Auto Rotate", &auto_rot);

			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
			ImGui::Separator();

			m_renderer.drawImgui();

			ImGui::End();
		}
	}

	void mainLoop() {
		while (!glfwWindowShouldClose(window)) {
			glfwPollEvents();

			m_renderer.newImGuiFrame();
			drawImGui();

			m_renderer.updateUniformBuffer(zoom);
			m_renderer.updateComputeUniformBuffer();

			m_renderer.draw();
		}

		m_renderer.waitIdle();
	}

public:
	void run() {

		initWindow();
		m_renderer.init(window, device_options);

		loadPackets();

		mainLoop();

		m_renderer.cleanup();
		cleanupWindow();
	}
};

int main() {
	HelloTriangleApplication app;

	try {
		app.run();
	}
	catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}