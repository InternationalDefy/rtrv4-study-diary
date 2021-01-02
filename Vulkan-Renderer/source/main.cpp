#define GLFW_INCLUDE_VULKAN
#include <glfw/glfw3.h>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

// vulkan.hpp doesnt include glm. it's safe to include glm anywhere around vulkan.hpp
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan.hpp>

// standrad library inlcudes
#include <iostream>
#include <fstream>
#include <variant>
#include <tuple>
#include <stdexcept>
#include <array>
#include <algorithm>
#include <vector>
#include <unordered_map>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <chrono>
#include <optional>
#include <set>

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

const std::string MODEL_PATH = "models/viking_room.obj";
const std::string TEXTURE_PATH = "textures/viking_room.png";

const uint32_t WIDTH = 1280;
const uint32_t HEIGHT = 720;
const int MAX_FRAMES_IN_FLIGHT = 2;

const auto startTime = std::chrono::high_resolution_clock::now();

const std::vector<const char*> validationLayers = {
	"VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

struct UniformBufferObject {
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};

struct Vertex {
	glm::vec3 pos;
	glm::vec3 color;
	glm::vec2 texCoord;
	static vk::VertexInputBindingDescription getBindingDescription() {
		vk::VertexInputBindingDescription bindingDescription{
			.binding = 0,
			.stride = sizeof(Vertex),
			.inputRate = vk::VertexInputRate::eVertex
		};
		return bindingDescription;
	}
	static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescription() {
		std::array<vk::VertexInputAttributeDescription, 3> attributeDescriptions{
			vk::VertexInputAttributeDescription{0,0,vk::Format::eR32G32B32Sfloat,offsetof(Vertex,pos)},
			vk::VertexInputAttributeDescription{0,1,vk::Format::eR32G32B32Sfloat,offsetof(Vertex,color)},
			vk::VertexInputAttributeDescription{0,2,vk::Format::eR32G32Sfloat,offsetof(Vertex,texCoord)},
		};
		return attributeDescriptions;
	}
	bool operator== (const Vertex& other) const {
		return pos == other.pos && color == other.color && texCoord == other.texCoord;
	}
};

// implement type trait hash for Vertex 
namespace std {
	template<> struct hash<Vertex>{
		size_t operator()(Vertex const& vertex) const {
			return ((hash<glm::vec3>()(vertex.pos) 
				^ (hash<glm::vec3>()(vertex.color) << 1)) >> 1) 
				^ (hash<glm::vec2>()(vertex.texCoord) << 1);
		}
	};
}

std::unordered_map<Vertex, uint32_t> uniqueVertices{};

// input is a pointer, because you need to create it. 
// Cpp port, C like implementation -> EXTProcAddr
VkResult CreateDebugUtilsMessengerEXT(
	VkInstance instance, 
	const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, 
	const VkAllocationCallbacks* pAllocator, 
	VkDebugUtilsMessengerEXT* pDebugMessager) {
	// Get Instance's Extension Func Address
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pDebugMessager);
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const
	VkAllocationCallbacks* pAllocator) {
	// Get Instance's Extension Func Address
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr) {
		func(instance, debugMessenger, pAllocator);
	}
}

// TODO is there a handle type to replace uint32_t?
// NOPE this is a repo to vk::Queuefamily
struct QueueFamilyIndices {
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentFamily;
	// found queue family we need
	bool isComplete() {
		return graphicsFamily.has_value() && presentFamily.has_value();
	}
};

// this is a repo to swapchian support capabilities
struct SwapChainSupportDetails {
	vk::SurfaceCapabilitiesKHR capabilities;
	std::vector<vk::SurfaceFormatKHR> formats;
	std::vector<vk::PresentModeKHR> presentModes;
};

class VulkanRenderer {
public:
	void test() {
		initWindow();
		createInstance();
		createSurface();
		pickPhysicalDevice();
		//createLogicalDevice();
	}
private:
	GLFWwindow* window;
	
	vk::Instance instance;
	vk::DebugUtilsMessengerEXT debugMessenger;
	vk::SurfaceKHR surface;
	// VK_NULLHANDLE : 0 -> nullptr
	vk::PhysicalDevice physicalDevice = nullptr;
	vk::Device device;
	// queue
	vk::Queue graphicsQueue;
	vk::Queue presentQueue;
	// swapchain
	vk::SwapchainKHR swapChain;
	std::vector<vk::Image> swapChainImage;
	vk::Format swapChainImageFormat;
	vk::Extent2D swapChainExtent;
	std::vector<vk::ImageView> swapChainImageView;
	std::vector<vk::Framebuffer> swapChainFrameBuffers;
	// pipeline descriptor
	vk::RenderPass renderPass;
	vk::PipelineLayout pipelineLayout;
	vk::DescriptorSetLayout descriptorSetLayout;
	vk::DescriptorPool descriptorPool;
	std::vector<vk::DescriptorSet> descriptorSets;
	vk::Pipeline graphicsPipeline;
	// commands
	vk::CommandPool commandPool;
	std::vector<vk::CommandBuffer> commandBuffers;
	// textures
	vk::Sampler textureSampler;
	vk::Image textureImage;
	vk::ImageView textureImageView;
	vk::DeviceMemory textureImageMemory;
	vk::Image depthImage;
	vk::ImageView depthImageView;
	vk::DeviceMemory depthImageMemory;
	// vertices & indices;
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	// buffers
	vk::Buffer vertexBuffer;
	vk::DeviceMemory vertexBufferMemory;
	vk::Buffer indexBuffer;
	vk::DeviceMemory indexBufferMemory;
	std::vector<vk::Buffer> uniformBuffers;
	std::vector<vk::DeviceMemory> uniformBuffersMemory;
	// fences
	std::vector<vk::Semaphore> imageAvaliableSemaphores;
	std::vector<vk::Semaphore> renderFinishedSemaphores;
	std::vector<vk::Fence> inFlightFences;
	std::vector<vk::Fence> imagesInFlight;
	size_t currentFrame = 0;

	bool framebufferResized = false;

	// window functions
	void initWindow() {
		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		window = glfwCreateWindow(WIDTH, HEIGHT, "Learn-RTR", nullptr, nullptr);
		glfwSetWindowUserPointer(window, this);
		glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
	}
	static std::vector<char> readFile(const std::string& filename) {
		std::ifstream file(filename, std::ios::ate | std::ios::binary);

		if (!file.is_open()) {
			throw std::runtime_error("failed to open file!");
		}
		size_t fileSize = (size_t)file.tellg();
		std::vector<char> buffer(fileSize);
		file.seekg(0);
		file.read(buffer.data(), fileSize);
		file.close();
		return buffer;
	}
	static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
		auto app = reinterpret_cast<VulkanRenderer*>(glfwGetWindowUserPointer(window));
		app->framebufferResized = true;
	}
	static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity, 
		vk::DebugUtilsMessageTypeFlagsEXT messageType, const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
		std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
		return VK_FALSE;
	}

	bool checkValidationLayerSupport() {
		// get all layer properties
		std::vector<vk::LayerProperties> availableLayers{ vk::enumerateInstanceLayerProperties() };
		// check if all the layers' names in validationLayers are supported
		for (const char* layerName : validationLayers) {
			bool layerFound = false;
			for (const auto& layerProperties : availableLayers) {
				if (strcmp(layerName, layerProperties.layerName) == 0) {
					layerFound = true;
					break;
				}
			}
			if (!layerFound) {
				return false;
			}
		}
		return true;
	}
	// get all extensions application required
	std::vector<const char*> getRequiredExtensions() {
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
		std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
#ifdef _DEBUG
		// push validation layers extension
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
		return extensions;
	}
	// get debugmessenger CreateInfo
	vk::DebugUtilsMessengerCreateInfoEXT getDebugMessengerCreateInfo() {
		vk::DebugUtilsMessengerCreateInfoEXT createInfo{
			.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose
			| vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
			| vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
			.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
			| vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
			| vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
			.pfnUserCallback = debugCallback,
		};
		return createInfo;
	}
	// Simply output a message, this function isn't ported to O-O style, :(
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, 
		VkDebugUtilsMessageTypeFlagsEXT messageType, 
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, 
		void* pUserData) {
		std::cout << "validation layer: " << pCallbackData->pMessage << std::endl;
		return VK_FALSE;
	}
	// create vk::Instance
	void createInstance() {
#ifdef _DEBUG
		if (!checkValidationLayerSupport()) {
			throw std::runtime_error("validation layer requested, but not supported");
		}
#endif
		// application info
		vk::ApplicationInfo appInfo{
			.pApplicationName = "Vulkan Renderer",
			.applicationVersion = VK_MAKE_VERSION(1,0,0),
			.pEngineName = "NO ENGINE",
			.engineVersion = VK_MAKE_VERSION(1,0,0),
			.apiVersion = VK_API_VERSION_1_2,
		};
		// extensions info
		// what this auto deduces to -> doenst matter and no meaning
		auto extensions = getRequiredExtensions();
		// create info
#ifdef _DEBUG
		auto p_debug_info = getDebugMessengerCreateInfo();
		vk::InstanceCreateInfo createInfo{
			.pNext = &p_debug_info,
			.pApplicationInfo = &appInfo,
			.enabledLayerCount = static_cast<uint32_t>(validationLayers.size()),
			.ppEnabledLayerNames = validationLayers.data(),
			.enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
			.ppEnabledExtensionNames = extensions.data(),
		};
#else
		vk::InstanceCreateInfo createInfo{			
			.pApplicationInfo = &appInfo,
			.enabledLayerCount = 0,
			.ppEnabledLayerNames = 0,
			.enabledExtensionCount = extensionCount,
			.ppEnabledExtensionNames = extensionNames,
		};
#endif
		// create instance		
		instance = vk::createInstance(createInfo);
		// setup Debugmessenger Itself
#ifdef _DEBUG
		VkInstance p_instance = instance;
		VkDebugUtilsMessengerCreateInfoEXT create_info = getDebugMessengerCreateInfo();
		VkDebugUtilsMessengerEXT debug_messenger = debugMessenger;
		auto result = CreateDebugUtilsMessengerEXT(p_instance, &create_info, NULL, &debug_messenger);
		if (result!=VK_SUCCESS)
		{
			throw("failed to create a debugmessenger");
		}
#endif
	}
	// create vk::SurfaceKHR
	void createSurface() {
		VkSurfaceKHR cSurface = surface;
		if (glfwCreateWindowSurface(instance, window, nullptr, &cSurface) != VK_SUCCESS) {
			throw std::runtime_error("failed to create surface");
		}
		surface = cSurface;
		return;
	}
	// create vk:;physicaldevice : pick a physical device 
	void pickPhysicalDevice() {
		// throw if no device;
		std::vector<vk::PhysicalDevice> devices = instance.enumeratePhysicalDevices();
		if (devices.size() == 0) {
			throw std::runtime_error("no physical devices found");
		}
		// pick a device
		for (const auto& device : devices) {
			
			// FIXME pick function
			if (isPhysicalDeviceSuitable(device)) {
				physicalDevice = device;
			}
		}
		// throw if no suitable device
		if (!physicalDevice) {
			throw std::runtime_error("no suitable device");
		}
	}
	
	bool isPhysicalDeviceSuitable(const vk::PhysicalDevice& device) {
		// check if physicaldevice supports queuefamilies
		QueueFamilyIndices indices = findQueueFamilyIndices(device);
		// check if physicaldevice supports extensions
		bool extensionsSupported = checkDeviceExtensionSupport(device);
		// check if physicaldevice supports swapchain
		bool swapChainAdequate = false;
		if (extensionsSupported) {
			SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
			swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
		}
		// get supported features
		vk::PhysicalDeviceFeatures supportedFeatures = device.getFeatures();
		// for now, we only need feature anisotropy supported;
		return indices.isComplete() && extensionsSupported && swapChainAdequate &&
			supportedFeatures.samplerAnisotropy;
	}
	// QueueFamily : https://stackoverflow.com/questions/55272626/what-is-actually-a-queue-family-in-vulkan
	QueueFamilyIndices findQueueFamilyIndices(const vk::PhysicalDevice& device) {
		// find queue family device supports
		auto queueFamilyProperties = device.getQueueFamilyProperties();
		QueueFamilyIndices indices;
		int i = 0;
		// find a graphics family and present family
		for (const auto& queuefamily : queueFamilyProperties) {
			if (queuefamily.queueFlags & vk::QueueFlagBits::eGraphics) {
				indices.graphicsFamily = i;
			}
			if (device.getSurfaceSupportKHR(i, surface)) {
				indices.presentFamily = i;
			}
			if (indices.isComplete()) {
				break;
			}
			i++;
		}
		return indices;
	}
	// details is a struct to describe surface features.
	// surface capabilities, surface surpoert formats and present modes
	SwapChainSupportDetails querySwapChainSupport(const vk::PhysicalDevice& device) {
		SwapChainSupportDetails details{};
		details.capabilities = device.getSurfaceCapabilitiesKHR(surface);
		// format support
		details.formats = device.getSurfaceFormatsKHR(surface);
		// present mode support
		details.presentModes = device.getSurfacePresentModesKHR(surface);
		return details;
	}
	// DeviceExtension: Extension features physical device support
	// this struct is a huge one with multiple supported features ' name strings
	bool checkDeviceExtensionSupport(const vk::PhysicalDevice& device) {
		// get supported extensions.
		auto supportedExtensions = device.enumerateDeviceExtensionProperties();
		// compare required extensions with supported extensions;
		std::set<std::string> requiredExtensions{ deviceExtensions.begin(), deviceExtensions.end() };
		for (const auto& extension : supportedExtensions) {
			requiredExtensions.erase(extension.extensionName);
		}
		return requiredExtensions.empty();
	}


};


int main() {
	VulkanRenderer renderer;
	renderer.test();
}
