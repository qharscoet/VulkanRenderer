#include <iostream>
#include <stdexcept>

#include <chrono>
#include <random>

#include <filesystem>

#include <glm/gtc/type_ptr.hpp>

#include "FileUtils.h"

/* Optional TODOs :
	- Use the transfer queue for staging buffer
	- Use separate commandPool for copy buffer */



#include <numbers>

//Wow so modern
#define RADIANS(degrees) (degrees * (std::numbers::pi/180.f))

#include "Device.h"

#include "Renderer.h"

#include "imgui.h"


Renderer::CameraInfo camera{
	.position = { 10.0f, 10.0f, 10.0f },

	.yaw = -145.f,
	.pitch = -45,

	.forward = {-1.0f, -1.0f, -1.0},
	.up = {0.0f, 1.0f, 0.0f},
};

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
	//camera.zoom -= yoffset * 0.1f;
}


void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {

}


static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
{
	if (glfw_state.pressed)
	{
		float xoffset = (xpos - glfw_state.mousepress_x);
		float yoffset = (ypos - glfw_state.mousepress_y);

		camera.yaw += xoffset * 0.1f;
		camera.pitch += -yoffset * 0.1f;

		camera.pitch = std::clamp(camera.pitch, -89.f, 89.f);

		camera.forward[0] = cos(RADIANS(camera.yaw)) * cos(RADIANS(camera.pitch));
		camera.forward[1] = sin(RADIANS(camera.pitch));
		camera.forward[2] = sin(RADIANS(camera.yaw)) * cos(RADIANS(camera.pitch));


		glfw_state.mousepress_x = xpos;
		glfw_state.mousepress_y = ypos;
	}

}

DeviceOptions device_options = {
	.usesMsaa = true,
};


class HelloTriangleApplication {

private:
	const uint32_t WIDTH = 1280;
	const uint32_t HEIGHT = 720;

	//Device m_device;
	GLFWwindow* window = nullptr;

	Renderer m_renderer;
	std::filesystem::path next_scene_path;


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
		glfwSetKeyCallback(window, key_callback);

		glfw_state.width = WIDTH;
		glfw_state.height = HEIGHT;
	}

	void cleanupWindow() {
		glfwDestroyWindow(window);
		glfwTerminate();
	}


	void loadPackets()
	{
		static const std::string gltfAssetsPath = "E:\\glTF-Sample-Assets\\Models\\";
		//m_renderer.addPacket(m_renderer.createPacket("assets/viking_room.obj", "assets/viking_room.png"));
		//m_renderer.addPacket(m_renderer.createPacket(gltfAssetsPath  + "CompareNormal/glTF/CompareNormal.gltf"));
		m_renderer.loadScene(gltfAssetsPath + "Sponza/glTF/Sponza.gltf");
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

			//ImGui::SliderFloat("Zoom", &camera.zoom, 0.0f, 10.0f);            // Edit 1 float using a slider from 0.0f to
			//ImGui::SliderFloat3("Rot", rotation, 0.0f, 4.0f);            // Edit 1 float using a slider from 0.0f to
			//ImGui::SliderFloat3("Translate", translate, 0.0f, 1.0f);          // Edit 1 float using a slider from 0.0f to
			//ImGui::Checkbox("Auto Rotate", &auto_rot);

			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
			ImGui::Separator();


			static const std::filesystem::path basepath{ "E:\\glTF-Sample-Assets\\Models\\" };
			static std::filesystem::path current_path = "Sponza";

			if (ImGui::BeginCombo("Select Scene", current_path.string().c_str()))
			{
				for (auto const& dir_entry : std::filesystem::directory_iterator{ basepath })
				{
					if (!dir_entry.is_directory())
						continue;

					const auto filename = dir_entry.path().filename();
					const bool is_selected = (current_path == filename);
					if (ImGui::Selectable(filename.string().c_str(), is_selected))
					{
						current_path = filename;

						next_scene_path = (basepath / current_path / "glTF" / (current_path.string() +  ".gltf"));
					}

					// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
					if (is_selected)
						ImGui::SetItemDefaultFocus();
				
				}
				ImGui::EndCombo();
			}

			m_renderer.drawImgui();

			ImGui::End();
		}
	}

	void processEvents()
	{
		if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS ||
			glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS ||
			glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS ||
			glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS )
		{
			//TODO : get rid of glm and try our hands at a math library
			glm::vec3 forward = glm::make_vec3(camera.forward);
			glm::vec3 up = glm::make_vec3(camera.up);
			glm::vec3 right = glm::cross(forward, up);
			glm::vec3 pos = glm::make_vec3(camera.position);

			
			float speed = 0.05f;

			if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
				pos += forward * speed;

			if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
				pos -= forward * speed;

			if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
				pos -= right * speed;

			if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
				pos += right * speed;

			camera.position[0] = pos.x;
			camera.position[1] = pos.y;
			camera.position[2] = pos.z;

		}

	}


	void mainLoop() {
		while (!glfwWindowShouldClose(window)) {
			glfwPollEvents();
			processEvents();

			m_renderer.newImGuiFrame();
			drawImGui();

			m_renderer.updateCamera(camera);

			m_renderer.draw();

			if (!next_scene_path.empty())
			{
				m_renderer.waitIdle();
				m_renderer.destroyAllPackets();
				m_renderer.loadScene(next_scene_path);
				next_scene_path.clear();
			}
		}

		m_renderer.waitIdle();
	}

public:
	void run() {

		initWindow();
		m_renderer.init(window, device_options);

		loadPackets();
		float sun[3] = { 0.0, -1.0f, 0.0f };
		m_renderer.addDirectionalLight(sun);

		const float lightPos[2][3] = { 
			{3.0f, 3.0f, 3.0f},
			{0.0f, 2.0f, 0.0f} 
		};
		m_renderer.addLight(lightPos[0]);
		m_renderer.addSpotlight(lightPos[1]);



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