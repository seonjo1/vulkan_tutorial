#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <chrono>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <array>
#include <optional>
#include <set>

// 동시에 처리할 최대 프레임 수
const int MAX_FRAMES_IN_FLIGHT = 2;

// 검증 레이어 설정
const std::vector<const char*> validationLayers = {
	"VK_LAYER_KHRONOS_validation"
};

// 스왑 체인 확장
const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

// 디버그 모드시 검증 레이어 사용
#ifdef NDEBUG
	const bool enableValidationLayers = false;
#else
	const bool enableValidationLayers = true;
#endif

/*
	디버그 메신저 객체 생성 함수
	(확장 함수는 자동으로 로드 되지 않으므로 동적으로 가져와야 한다.)
*/
VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
										const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
	// vkCreateDebugUtilsMessengerEXT 함수의 주소를 vkGetInstanceProcAddr를 통해 동적으로 가져옴
	PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	} else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

/*
	디버그 메신저 객체 파괴 함수
	(확장 함수는 자동으로 로드 되지 않으므로 동적으로 가져와야 한다.)
*/
void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
	PFN_vkDestroyDebugUtilsMessengerEXT func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr) {
		func(instance, debugMessenger, pAllocator);
	}
}

// 큐 패밀리 인덱스 관리 구조체
struct QueueFamilyIndices {
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentFamily;

	bool isComplete() {
		return graphicsFamily.has_value() && presentFamily.has_value();
	}
};

// GPU와 surface가 지원하는 SwapChain 지원 세부 정보 구조체
struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};


struct Vertex {
	glm::vec2 pos;
	glm::vec3 color;

	// 정점 데이터가 전달되는 방법을 알려주는 구조체 반환하는 함수
	static VkVertexInputBindingDescription getBindingDescription() {
		// 파이프라인에 정점 데이터가 전달되는 방법을 알려주는 구조체
		VkVertexInputBindingDescription bindingDescription{};		
		bindingDescription.binding = 0;								// 버텍스 바인딩 포인트 (현재 0번에 vertex 정보 바인딩)
		bindingDescription.stride = sizeof(Vertex);					// 버텍스 1개 단위의 정보 크기
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX; // 정점 데이터 처리 방법
																	// 1. VK_VERTEX_INPUT_RATE_VERTEX : 정점별로 데이터 처리
																	// 2. VK_VERTEX_INPUT_RATE_INSTANCE : 인스턴스별로 데이터 처리
		return bindingDescription;
	}

	// 정점 속성별 데이터 형식과 위치를 지정하는 구조체 반환하는 함수
	static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
		// 정점 속성의 데이터 형식과 위치를 지정하는 구조체
		std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};

		// pos 속성 정보 입력
		attributeDescriptions[0].binding = 0;						// 버텍스 버퍼의 바인딩 포인트
		attributeDescriptions[0].location = 0;						// 버텍스 셰이더의 어떤 location에 대응되는지 지정
		attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;	// 저장되는 데이터 형식 (VK_FORMAT_R32G32_SFLOAT = vec3)
		attributeDescriptions[0].offset = offsetof(Vertex, pos);	// 버텍스 구조체에서 해당 속성이 시작되는 위치

		// color 속성 정보 입력
		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(Vertex, color);

		return attributeDescriptions;
	}
};

struct UniformBufferObject {
	alignas(16) glm::mat4 model;
	alignas(16) glm::mat4 view;
	alignas(16) glm::mat4 proj;
};

const std::vector<Vertex> vertices = {
	{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
	{{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
	{{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
	{{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
};

const std::vector<uint16_t> indices = {
	0, 1, 2, 2, 3, 0
};

class HelloTriangleApplication {
public:
	void run() {
		initWindow();
		initVulkan();
		mainLoop();
		cleanup();
	}

private:
	GLFWwindow* window;

	VkInstance instance;
	VkDebugUtilsMessengerEXT debugMessenger;
	VkSurfaceKHR surface;

	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice device;
	
	VkQueue graphicsQueue;
	VkQueue presentQueue;

	VkSwapchainKHR swapChain;
	std::vector<VkImage> swapChainImages;
	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;
	std::vector<VkImageView> swapChainImageViews;
	std::vector<VkFramebuffer> swapChainFramebuffers;

	VkRenderPass renderPass;
	VkDescriptorSetLayout descriptorSetLayout;
	VkPipelineLayout pipelineLayout;
	VkPipeline graphicsPipeline;

	VkCommandPool commandPool;

	VkBuffer vertexBuffer;
	VkDeviceMemory vertexBufferMemory;
	VkBuffer indexBuffer;
	VkDeviceMemory indexBufferMemory;

	std::vector<VkBuffer> uniformBuffers;
	std::vector<VkDeviceMemory> uniformBuffersMemory;
	std::vector<void*> uniformBuffersMapped;

	VkDescriptorPool descriptorPool;
	std::vector<VkDescriptorSet> descriptorSets;
	
	std::vector<VkCommandBuffer> commandBuffers;

	std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	std::vector<VkFence> inFlightFences;
	uint32_t currentFrame = 0;

	bool framebufferResized = false;

	// glfw 실행, window 생성, 콜백 함수 등록
	void initWindow() {
		glfwInit();

		// glfw로 창관리만 하고 렌더링 API는 안 쓴다는 설정
		// 렌더링 및 그래픽 처리는 외부 API인 Vulkan으로 함
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Vulkan", nullptr, nullptr);
		// window에 현재 HelloTriangleApplication 객체를 바인딩
		glfwSetWindowUserPointer(window, this);
		// 프레임버퍼 사이즈 변경 콜백 함수 등록
		glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
	}

	static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
		// window에 바인딩된 객체 호출 및 framebufferResized = true 설정
		auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window));
		app->framebufferResized = true;
	}

	// 렌더링을 위한 초기 setting
	void initVulkan() {
		createInstance();
		setupDebugMessenger();
		createSurface();
		pickPhysicalDevice();
		createLogicalDevice();
		createSwapChain();
		createImageViews();
		createRenderPass();
		createDescriptorSetLayout();
		createGraphicsPipeline();
		createFramebuffers();
		createCommandPool();
		createVertexBuffer();
		createIndexBuffer();
		createUniformBuffers();		
		createDescriptorPool();
		createDescriptorSets();
		createCommandBuffers();
		createSyncObjects();
	}

	/*
		렌더링 루프 실행	
	*/
	void mainLoop() {
		while (!glfwWindowShouldClose(window)) {
			glfwPollEvents();
			drawFrame();
		}
		vkDeviceWaitIdle(device);  // 종료시 실행 중인 GPU 작업을 전부 기다림
	}

	// FrameBuffer, ImageView, SwapChain 삭제
	void cleanupSwapChain() {
		// 프레임 버퍼 배열 삭제
		for (auto framebuffer : swapChainFramebuffers) {
			vkDestroyFramebuffer(device, framebuffer, nullptr);
		}
		// 이미지뷰 삭제
		for (auto imageView : swapChainImageViews) {
			vkDestroyImageView(device, imageView, nullptr);
		}
		// 스왑 체인 파괴
		vkDestroySwapchainKHR(device, swapChain, nullptr);
	}

	/*
		[사용한 자원들 정리]
	*/
	void cleanup() {
		// 스왑 체인 파괴
		cleanupSwapChain();

		vkDestroyPipeline(device, graphicsPipeline, nullptr);      	// 파이프라인 객체 삭제
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);  	// 파이프라인 레이아웃 삭제
		vkDestroyRenderPass(device, renderPass, nullptr);         	// 렌더 패스 삭제

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			// 매핑된 거 해제 안하나?????????????????????
            vkDestroyBuffer(device, uniformBuffers[i], nullptr);	// 유니폼 버퍼 객체 삭제
            vkFreeMemory(device, uniformBuffersMemory[i], nullptr);	// 유니폼 버퍼에 할당된 메모리 삭제
        }

		vkDestroyDescriptorPool(device, descriptorPool, nullptr);			// 디스크립터 풀 삭제
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);	// 디스크립터 셋 레이아수 삭제

		vkDestroyBuffer(device, indexBuffer, nullptr);				// 인덱스 버퍼 객체 삭제
		vkFreeMemory(device, indexBufferMemory, nullptr);			// 인덱스 버퍼에 할당된 메모리 삭제
		
		vkDestroyBuffer(device, vertexBuffer, nullptr);				// 버텍스 버퍼 객체 삭제
		vkFreeMemory(device, vertexBufferMemory, nullptr);			// 버텍스 버퍼에 할당된 메모리 삭제

		// 세마포어, 펜스 파괴
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
			vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
			vkDestroyFence(device, inFlightFences[i], nullptr);
		}

		vkDestroyCommandPool(device, commandPool, nullptr); 	  	// 커맨드 풀 파괴

		vkDestroyDevice(device, nullptr);                         	// 논리적 장치 파괴

		// 메시지 객체 파괴
		if (enableValidationLayers) {
			DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
		}

		vkDestroySurfaceKHR(instance, surface, nullptr);          	// 화면 객체 파괴
		vkDestroyInstance(instance, nullptr);						// 인스턴스 파괴

		glfwDestroyWindow(window);                              	// 윈도우 파괴
		glfwTerminate();									        // glfw 종료
	}

	/*
		변경된 window 크기에 맞게 SwapChain, ImageView, FrameBuffer 재생성
	*/
	void recreateSwapChain() {
		// 현재 프레임버퍼 사이즈 체크
		int width = 0, height = 0;
		glfwGetFramebufferSize(window, &width, &height);
		
		// 현재 프레임 버퍼 사이즈가 0이면 다음 이벤트 호출까지 대기
		while (width == 0 || height == 0) {
			glfwGetFramebufferSize(window, &width, &height);
			glfwWaitEvents(); // 다음 이벤트 발생 전까지 대기하여 CPU 사용률을 줄이는 함수 
		}

		// 모든 GPU 작업 종료될 때까지 대기 (사용중인 리소스를 건들지 않기 위해)
		vkDeviceWaitIdle(device);

		// 스왑 체인 관련 리소스 정리
		cleanupSwapChain();

		// 현재 window 크기에 맞게 SwapChain, ImageView, FrameBuffer 재생성
		createSwapChain();
		createImageViews();
		createFramebuffers();
	}

	/*
		[인스턴스 생성]
		인스턴스 구성 요소
		1. 애플리케이션 정보
		2. 확장
		3. 레이어
		4. 디버그 모드일시 디버그 메신저 객체 생성 정보 추가
	*/ 
	void createInstance() {
		// 디버그 모드에서 검증 레이어 적용 불가능시 예외 발생
		if (enableValidationLayers && !checkValidationLayerSupport()) {
			throw std::runtime_error("validation layers requested, but not available!");
		}

		// 애플리케이션 정보를 담은 구조체
		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Hello Triangle";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;

		// 인스턴스 생성을 위한 정보를 담은 구조체
		VkInstanceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;
		std::vector<const char*> extensions = getRequiredExtensions();
		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();
	
		// 디버깅 메시지 객체 생성을 위한 정보 구조체
		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};

		if (enableValidationLayers) {
			// 디버그 모드시 구조체에 검증 레이어 포함
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();
			// 인스턴스 생성 및 파괴시에도 검증 가능
			populateDebugMessengerCreateInfo(debugCreateInfo);
			createInfo.pNext = reinterpret_cast<VkDebugUtilsMessengerCreateInfoEXT*>(&debugCreateInfo);
		} else {
			// 디버그 모드 아닐 시 검증 레이어 x
			createInfo.enabledLayerCount = 0;		
			createInfo.pNext = nullptr;
		}

		// 인스턴스 생성
		if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
			throw std::runtime_error("failed to create instance!");
		}
	}

	//vkDebugUtilsMessengerCreateInfoEXT 구조체 내부를 채워주는 함수
	void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
		createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
									VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
									VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
								VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
								VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = debugCallback;
	}

	// 디버그 메신저 객체 생성
	void setupDebugMessenger() {
		// 디버그 모드 아니면 return
		if (!enableValidationLayers) return;

		// VkDebugUtilsMessengerCreateInfoEXT 구조체 생성
		VkDebugUtilsMessengerCreateInfoEXT createInfo;
		populateDebugMessengerCreateInfo(createInfo);

		// 디버그 메시지 객체 생성
		if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
			throw std::runtime_error("failed to set up debug messenger!");
		}
	}
	
	// OS에 맞는 surface를 glfw 함수를 통해 생성
	void createSurface() {
		if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
			throw std::runtime_error("failed to create window surface!");
		}
	}
	
	// 적절한 GPU 고르는 함수
	void pickPhysicalDevice() {
		// GPU 장치 목록 불러오기
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

		if (deviceCount == 0) {
  			  throw std::runtime_error("failed to find GPUs with Vulkan support!");
		}

		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

		// 적합한 GPU 탐색
		for (const auto& device : devices) {
			if (isDeviceSuitable(device)) {
				physicalDevice = device;
				break;
			}
		}

		// 적합한 GPU가 발견되지 않은 경우 에러 발생
		if (physicalDevice == VK_NULL_HANDLE) {
			throw std::runtime_error("failed to find a suitable GPU!");
		}
	}	

	// GPU와 소통할 인터페이스인 Logical device 생성
	void createLogicalDevice() {
		// 그래픽 큐 패밀리의 인덱스를 가져옴
		QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
		// 큐 패밀리의 인덱스들을 set으로 래핑
		std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

		// 큐 생성을 위한 정보 설정 
		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		// 큐 우선순위 0.0f ~ 1.0f 로 표현
		float queuePriority = 1.0f;
		// 큐 패밀리 별로 정보 생성
		for (uint32_t queueFamily : uniqueQueueFamilies) {
			VkDeviceQueueCreateInfo queueCreateInfo{};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back(queueCreateInfo);
		}

		// 사용할 장치 기능이 포함된 구조체
		// vkGetPhysicalDeviceFeatures 함수로 디바이스에서 설정 가능한
		// 장치 기능 목록을 확인할 수 있음
		// 일단 지금은 VK_FALSE로 전부 등록함
		VkPhysicalDeviceFeatures deviceFeatures{};

		// 논리적 장치 생성을 위한 정보 등록
		VkDeviceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		createInfo.pQueueCreateInfos = queueCreateInfos.data();
		createInfo.pEnabledFeatures = &deviceFeatures;

		// 확장 설정
		createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
		createInfo.ppEnabledExtensionNames = deviceExtensions.data();
		
		// 구버전 호환을 위해 디버그 모드일 경우
		// 검증 레이어를 포함 시키지만, 현대 시스템에서는 논리적 장치의 레이어를 안 씀
		if (enableValidationLayers) {
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();
		} else {
			createInfo.enabledLayerCount = 0;
		}

		// 논리적 장치 생성
		if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
 		   throw std::runtime_error("failed to create logical device!");
		}

		// 큐 핸들 가져오기
		vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
		vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
	}

	/*
	[스왑 체인 생성]
	스왑 체인의 역할
	1. 다수의 프레임 버퍼(이미지) 관리
	2. 이중 버퍼링 및 삼중 버퍼링 (다수의 버퍼를 이용하여 화면에 프레임 전환시 딜레이 최소화)
	3. 프레젠테이션 모드 관리 (화면에 프레임을 표시하는 방법 설정 가능)
	4. 화면과 GPU작업의 동기화 (GPU가 이미지를 생성하는 작업과 화면이 이미지를 띄우는 작업 간의 동기화) 
	*/ 
	void createSwapChain() {
		// GPU와 surface가 지원하는 SwapChain 정보 불러오기
		SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

		// 서피스 포맷 선택
		VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
		// 프레젠테이션 모드 선택
		VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
		// 스왑 범위 선택 (스왑 체인의 이미지 해상도 결정)
		VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

		// 스왑 체인에서 필요한 이미지 수 결정 (최소 이미지 수 + 1)
		uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

		// 만약 필요한 이미지 수가 최댓값을 넘으면 clamp
		if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
 		   imageCount = swapChainSupport.capabilities.maxImageCount;
		}
		
		// SwapChain 정보 생성
		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = surface;

		// 이미지 개수의 최솟값 설정 (최댓값은 아까 봤던 이미지 최댓값으로 들어감)
		createInfo.minImageCount = imageCount;
		// 이미지 정보 입력
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1; // 한 번의 렌더링에 n 개의 결과가 생긴다. (스테레오 3D, cubemap 이용시 여러 개 레이어 사용)
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // 기본 렌더링에만 사용하는 플래그 (만약 렌더링 후 2차 가공 필요시 다른 플래그 사용)

		// GPU가 지원하는 큐패밀리 목록 가져오기
		QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
		uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

		// 큐패밀리 정보 등록
		if (indices.graphicsFamily != indices.presentFamily) {
			// [그래픽스 큐패밀리와 프레젠트 큐패밀리가 서로 다른 큐인 경우]
			// 하나의 이미지에 두 큐패밀리가 동시에 접근할 수 있게 하여 성능을 높인다.
			// (큐패밀리가 이미지에 접근할 때 다른 큐패밀리가 접근했는지 확인하는 절차 삭제)
			// 그러나 실제로 동시에 접근하면 안 되므로 순서를 프로그래머가 직접 조율해야 한다. 
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT; // 동시 접근 허용
			createInfo.queueFamilyIndexCount = 2;                     // 큐패밀리 개수 등록
			createInfo.pQueueFamilyIndices = queueFamilyIndices;      // 큐패밀리 인덱스 배열 등록
		} else {
			// [그래픽스 큐패밀리와 프레젠트 큐패밀리가 서로 같은 큐인 경우]
			// 어처피 1개의 큐패밀리만 존재하므로 큐패밀리의 이미지 독점을 허용한다.
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;  // 큐패밀리의 독점 접근 허용
		}

		createInfo.preTransform = swapChainSupport.capabilities.currentTransform;  // 스왑 체인 이미지를 화면에 표시할때 적용되는 변환 등록
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;  // 렌더링 결과의 알파 값을 window에도 적용시킬지 설정 (현재는 알파블랜딩 없는 설정)
		createInfo.presentMode = presentMode; // 프레젠트 모드 설정
		createInfo.clipped = VK_TRUE; // 실제 컴퓨터 화면에 보이지 않는 부분을 렌더링 할 것인지 설정 (VK_TRUE = 렌더링 하지 않겠다)

		createInfo.oldSwapchain = VK_NULL_HANDLE; // 재활용할 이전 스왑체인 설정 (만약 설정한다면 새로운 할당을 하지 않고 가능한만큼 이전 스왑체인 리소스 재활용)

		/* 
		[스왑 체인 생성] 
		스왑 체인 생성시 이미지들도 설정대로 만들어지고, 
	 	만약 렌더링에 필요한 추가 이미지가 있으면 따로 만들어야 함 
		*/
		if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
			throw std::runtime_error("failed to create swap chain!");
		}

		// 스왑 체인 이미지 개수 저장
		vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
		// 스왑 체인 이미지 개수만큼 vector 초기화 
		swapChainImages.resize(imageCount);
		// 이미지 개수만큼 vector에 스왑 체인의 이미지 핸들 채우기 
		vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

		// 스왑 체인의 이미지 포맷 저장
		swapChainImageFormat = surfaceFormat.format;
		// 스왑 체인의 이미지 크기 저장
		swapChainExtent = extent;
	}

	/*
	[이미지 뷰 생성]
	이미지 뷰란 VKImage에 대한 접근 방식을 정의하는 객체
	GPU가 이미지를 읽을 때만 사용 (이미지를 텍스처로 쓰거나 후처리 하는 등)
	GPU가 이미지에 쓰기 작업할 떄는 x
	*/ 
	void createImageViews() {
		// 이미지의 개수만큼 vector 초기화
		swapChainImageViews.resize(swapChainImages.size());

		// 이미지 뷰 생성
		for (size_t i = 0; i < swapChainImages.size(); i++) {
			// 이미지 뷰 정보 생성
			VkImageViewCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			createInfo.image = swapChainImages[i];             // 이미지 핸들

			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;       // 이미지 타입 (이미지가 해석될 방법 결정)
			createInfo.format = swapChainImageFormat;          // 이미지 포맷 설정

			// 이미지 rgba 채널 읽는 방법 설정 (그대로 읽기 / 0 또는 1로 읽기 / 다른 채널 값으로 읽기)
			createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;     
			createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

			createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;    // 이미지 형식 결정 (color / depth / stencil 등)
			createInfo.subresourceRange.baseMipLevel = 0;                          // 렌더링할 mipmap 단계 설정
			createInfo.subresourceRange.levelCount = 1;                            // baseMipLevel 기준으로 몇 개의 MipLevel을 더 사용할지 설정 (실제 mipmap 만드는 건 따로 해줘야함)
			createInfo.subresourceRange.baseArrayLayer = 0;                        // ImageView가 참조하는 이미지 레이어의 시작 위치 정의
			createInfo.subresourceRange.layerCount = 1;                            // 스왑 체인에서 설정한 이미지 레이어 개수

			// 이미지 뷰 생성
			if (vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) {
				throw std::runtime_error("failed to create image views!");
			}
		}
	}

	/*
		[렌더패스 생성]
		렌더패스 구성 요소
		1. attachment 설정
		2. subpass
	*/
	void createRenderPass() {
		// [attachment 설정]
		// FrameBuffer의 attachment에 어떤 정보를 어떻게 기록할지 정하는 객체
		VkAttachmentDescription colorAttachment{};
		colorAttachment.format = swapChainImageFormat;		 					// 이미지 포맷 (스왑 체인과 일치 시킴)
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT; 						// 샘플 개수 (멀티 샘플링을 위한 값 사용)
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;					// 렌더링 전 버퍼 클리어 (렌더링 시작 시 기존 attachment의 데이터 처리 방법)
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; 				// 렌더링 결과 저장 (렌더링 후 attachment를 메모리에 저장하는 방법 결정)
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; 		// 이전 데이터 무시 (스텐실 버퍼의 loadOp)
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; 		// 저장 x (스텐실 버퍼의 storeOp)
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; 				// 초기 레이아웃 설정을 UNDEFINED로 설정 (초기 데이터 가공을 하지 않기 때문에 가장 빠름)
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; 			// 최종 레이아웃 화면 출력용으로 설정

		// subpass가 attachment 설정 어떤 것을 어떻게 참조할지 정의
		VkAttachmentReference colorAttachmentRef{};
		colorAttachmentRef.attachment = 0; 										// 특정 attachment 설정의 index
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;	// attachment 설정을 subpass 내에서
																				// 어떤 layout으로 쓸지 결정 (현재는 color attachment로 사용하는 설정)

		// [subpass 정의]
		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1; 										// attachment 설정 개수 등록
		subpass.pColorAttachments = &colorAttachmentRef;						// attachment 설정 등록

		// [렌더 패스 정의]
		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = 1;										// attachment 설정 개수 등록
		renderPassInfo.pAttachments = &colorAttachment;							// attachment 설정 등록
		renderPassInfo.subpassCount = 1;										// subpass 개수 등록
		renderPassInfo.pSubpasses = &subpass;									// subpass 등록

		// [렌더 패스 생성]
		if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
			throw std::runtime_error("failed to create render pass!");
		}
	}

	/* 
		[디스크립터 셋 레이아웃 생성]
		디스크립터 셋 레이아웃이란? 
		셰이더가 사용할 리소스의 타입과 바인딩 위치를 사전에 정의하는 객체
	*/
	void createDescriptorSetLayout() {
		// 셰이더에 바인딩할 리소스의 종류와 바인딩 위치를 설정할 때 쓰이는 구조체
		VkDescriptorSetLayoutBinding uboLayoutBinding{};
		uboLayoutBinding.binding = 0;											// 바인딩 위치 지정 (디스크립터 셋 내부의 순서)
		uboLayoutBinding.descriptorCount = 1;									// 디스크립터의 개수 (구조체는 1개의 디스크립터 취급, 배열 사용시 여러 개 디스크립터 취급)
		uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;	// 디스크립터의 종류 (현재는 Uniform buffer)
		uboLayoutBinding.pImmutableSamplers = nullptr;							// 변경 불가능한(immutable) 샘플러를 사용할 경우 지정하는 포인터인데 지금은 상관 없음
		uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;				// 사용할 스테이지 지정 (현재는 vertex shader에서 사용하고 여러 스테이지 지정 가능)

		// 디스크립터 셋 레이아웃을 생성하기 위한 설정 정보를 포함한 구조체
		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = 1;											// 디스크립터 셋 레이아웃에 포함될 바인딩 정보의 개수
		layoutInfo.pBindings = &uboLayoutBinding;								// 디스크립터 셋 레이아웃에 포함될 바인딩 정보의 배열

		// 디스크립터 셋 레이아웃 생성
		if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
			throw std::runtime_error("failed to create descriptor set layout!");
		}
	}

	/*
	[파이프라인 객체 생성]
	파이프라인 객체는 GPU가 그래픽 또는 컴퓨팅 명령을 실행할 때 필요한 설정을 제공한다.
	*/ 
	void createGraphicsPipeline() {
		// SPIR-V 파일 읽기
		std::vector<char> vertShaderCode = readFile("./shaders/vert.spv");
		std::vector<char> fragShaderCode = readFile("./shaders/frag.spv");

		// shader module 생성
		VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
		VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

		/*
		shader stage 란?
		그래픽 파이프라인에서 사용할 셰이더 단계를 정의하는 구조체
		특정 셰이더 단계에서 사용할 셰이더 코드를 가지고 있음
		*/ 

		// vertex shader stage 설정
		VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
		vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT; // 쉐이더 종류
		vertShaderStageInfo.module = vertShaderModule; // 쉐이더 모듈
		vertShaderStageInfo.pName = "main"; // 쉐이더 파일 내부에서 가장 먼저 시작 될 함수 이름 (엔트리 포인트)

		// fragment shader stage 설정
		VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
		fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT; // 쉐이더 종류
		fragShaderStageInfo.module = fragShaderModule; // 쉐이더 모듈
		fragShaderStageInfo.pName = "main"; // 쉐이더 파일 내부에서 가장 먼저 시작 될 함수 이름 (엔트리 포인트)

		// shader stage 모음
		VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};


		// [vertex 정보 설정]
		VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

		auto bindingDescription = Vertex::getBindingDescription();												// 정점 바인딩 정보를 가진 구조체
		auto attributeDescriptions = Vertex::getAttributeDescriptions();										// 정점 속성 정보를 가진 구조체 배열

		vertexInputInfo.vertexBindingDescriptionCount = 1;														// 정점 바인딩 정보 개수
		vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());	// 정점 속성 정보 개수
		vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;										// 정점 바인딩 정보
		vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();							// 정점 속성 정보

		// [input assembly 설정] (그려질 primitive 설정)
		VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; // primitive로 삼각형 설정
		inputAssembly.primitiveRestartEnable = VK_FALSE; // 인덱스 재시작 x

		/*
		[viewport와 scissor의 설정 값을 정의]
		viewport: 화면좌표로 매핑되어 정규화된 이미지 좌표를, viewport 크기에 맞는 픽셀 좌표로 변경 
				  width, height는 (0, 0) ~ (width, height)로 depth는 (0.0f) ~ (1.0f)로 좌표 설정 
		scissor : 픽셀 좌표의 특정 영역에만 렌더링을 하도록 범위를 설정
				  픽셀 좌표 내의 offset ~ extent 범위만 렌더링 진행 (나머지는 렌더링 x이므로 쓸데없는 계산 최소화)
		*/ 
		VkPipelineViewportStateCreateInfo viewportState{};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1; // 사용할 뷰포트의 수
		viewportState.scissorCount = 1;  // 사용할 시저의수

		// [rasterizer 설정]
		VkPipelineRasterizationStateCreateInfo rasterizer{};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable = VK_FALSE;  				// VK_FALSE로 설정시 depth clamping이 적용되지 않아 0.0f ~ 1.0f 범위 밖의 프레그먼트는 삭제됨
		rasterizer.rasterizerDiscardEnable = VK_FALSE;  		// rasterization 진행 여부 결정, VK_TRUE시 렌더링 진행 x
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;  		// 다각형 그리는 방법 선택 (점만, 윤곽선만, 기본 값 등)
		rasterizer.lineWidth = 1.0f;							// 선의 굵기 설정 
		rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;			// cull 모드 설정 (앞면 혹은 뒷면은 그리지 않는 설정 가능)
		rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;	// 앞면의 기준 설정 (y축 반전에 의해 정점이 시계 반대방향으로 그려지므로 앞면을 시계 반대방향으로 설정)
		rasterizer.depthBiasEnable = VK_FALSE;					// depth에 bias를 설정하여 z-fighting 해결할 수 있음 (원근 투영시 멀어질 수록 z값의 차이가 미미해짐)
																// VK_TRUE일 경우 추가 설정 필요

		// [멀티 샘플링 설정]
		VkPipelineMultisampleStateCreateInfo multisampling{};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_FALSE; // 샘플 기준 셰이딩 false 
													  // VK_TRUE: 프레그먼트 셰이더 단계(음영 계산)부터 샘플별로 계산 후 최종 결과 평균내서 사용 
													  // VK_FALSE: 테스트&블랜딩 단계부터 샘플별로 계산 후 최종 결과 평균내서 사용 (음영 계산은 동일한 값) 
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT; // 픽셀당 샘플 개수 설정

		// [블랜딩 설정]
		// attachment 별 블랜딩 설정 (블랜딩 + 프레임 버퍼 기록 설정)
		VkPipelineColorBlendAttachmentState colorBlendAttachment{};
		// 프레임 버퍼에 RGBA 값 쓰기 가능 모드 설정 (설정 안 하면 색 수정 x)
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_FALSE; // 블랜드 기능 off (블랜드 기능 on 시 추가적인 설정 필요)
		// 파이프라인 전체 블랜딩 설정 (attachment 블랜딩 설정 추가)
		VkPipelineColorBlendStateCreateInfo colorBlending{};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable = VK_FALSE; // 논리 연산 블랜딩 off (블랜딩 대신 논리적 연산을 통해 색을 조합하는 방법으로, 사용시 블렌딩 적용 x)
		colorBlending.logicOp = VK_LOGIC_OP_COPY; // 논리 연산 없이 그냥 전체 복사 (논리 연산 블랜딩이 off면 안 쓰임)
		colorBlending.attachmentCount = 1; // attachment 별 블랜딩 설정 개수
		colorBlending.pAttachments = &colorBlendAttachment; // attachment 별 블랜딩 설정 배열
		// 블랜딩 연산에 사용하는 변수 값 4개 설정 (모든 attachment에 공통으로 사용)
		colorBlending.blendConstants[0] = 0.0f;
		colorBlending.blendConstants[1] = 0.0f;
		colorBlending.blendConstants[2] = 0.0f;
		colorBlending.blendConstants[3] = 0.0f;


		// [파이프라인에서 런타임에 동적으로 상태를 변경할 state 설정]
		std::vector<VkDynamicState> dynamicStates = {
			// Viewport와 Scissor 를 동적 상태로 설정
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};
		VkPipelineDynamicStateCreateInfo dynamicState{};
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
		dynamicState.pDynamicStates = dynamicStates.data();


		// [파이프라인 레이아웃 생성]
		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 1; 									// 디스크립터 셋 레이아웃 개수
		pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout; 					// 디스크립투 셋 레이아웃

		if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
			throw std::runtime_error("failed to create pipeline layout!");
		}

		// [파이프라인 정보 생성]
		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = 2; 								// vertex shader, fragment shader 2개 사용
		pipelineInfo.pStages = shaderStages; 						// vertex shader, fragment shader 총 2개의 stageinfo 입력
		pipelineInfo.pVertexInputState = &vertexInputInfo; 			// 정점 정보 입력
		pipelineInfo.pInputAssemblyState = &inputAssembly;			// primitive 정보 입력
		pipelineInfo.pViewportState = &viewportState;				// viewport, scissor 정보 입력
		pipelineInfo.pRasterizationState = &rasterizer;				// 레스터라이저 설정 입력
		pipelineInfo.pMultisampleState = &multisampling;			// multisampling 설정 입력
		pipelineInfo.pDepthStencilState = nullptr; 					// Optional
		pipelineInfo.pColorBlendState = &colorBlending;				// 블랜딩 설정 입력
		pipelineInfo.pDynamicState = &dynamicState;					// 동적으로 변경할 상태 입력
		pipelineInfo.layout = pipelineLayout;						// 파이프라인 레이아웃 설정 입력
		pipelineInfo.renderPass = renderPass;						// 렌더패스 입력
		pipelineInfo.subpass = 0;									// 렌더패스 내 서브패스의 인덱스
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;			// 상속을 위한 기존 파이프라인 핸들
		pipelineInfo.basePipelineIndex = -1; 						// Optional (상속을 위한 기존 파이프라인 인덱스)	

		// [파이프라인 객체 생성]
		// 두 번째 매개변수는 상속할 파이프라인
		if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
			throw std::runtime_error("failed to create graphics pipeline!");
		}

		// 쉐이더 모듈 더 안 쓰므로 제거
		vkDestroyShaderModule(device, fragShaderModule, nullptr);
		vkDestroyShaderModule(device, vertShaderModule, nullptr);
	}

	/*
		[프레임 버퍼 생성]
		프레임 버퍼를 생성하고 SwapChain의 이미지를 attachment로 설정 
		(depth buffer 같은 image가 더 필요하면 따로 image를 생성해서 attachment에 추가해야 함)
	*/
	void createFramebuffers() {
		// 프레임 버퍼 배열 초기화
		swapChainFramebuffers.resize(swapChainImageViews.size());

		// 이미지 뷰마다 프레임 버퍼 1개씩 생성
		for (size_t i = 0; i < swapChainImageViews.size(); i++) {
			VkImageView attachments[] = {
				swapChainImageViews[i]
			};

			VkFramebufferCreateInfo framebufferInfo{};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = renderPass; 							// 렌더 패스 등록
			framebufferInfo.attachmentCount = 1; 								// attachment 개수
			framebufferInfo.pAttachments = attachments;							// attachment 등록
			framebufferInfo.width = swapChainExtent.width;						// 프레임 버퍼 width
			framebufferInfo.height = swapChainExtent.height;					// 프레임 버퍼 height
			framebufferInfo.layers = 1;											// 레이어 수

			if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
				throw std::runtime_error("failed to create framebuffer!");
			}
		}
	}

	/*
		[커맨드 풀 생성]
		커맨드 풀이란?
		1. 커맨드 버퍼들을 관리한다.
		2. 큐 패밀리당 1개의 커맨드 풀이 필요하다.
	*/
	void createCommandPool() {
		// 큐 패밀리 인덱스 가져오기
		QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; 		// 커맨드 버퍼를 개별적으로 재설정할 수 있도록 설정 
																				// (이게 아니면 커맨드 풀의 모든 커맨드 버퍼 설정이 한 번에 이루어짐)
		poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value(); 	// 그래픽스 큐 인덱스 등록 (대응시킬 큐 패밀리 등록)

		// 커맨드 풀 생성
		if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
			throw std::runtime_error("failed to create command pool!");
		}
	}

	/*
		[버텍스 버퍼 생성]
		1. 스테이징 버퍼 생성
		2. 스테이징 버퍼에 정점 정보 복사
		3. 버텍스 버퍼 생성
		4. 스테이징 버퍼의 정점 정보를 버텍스 버퍼에 복사
		5. 스테이징 버퍼 리소스 해제
	*/ 
	void createVertexBuffer() {
		// 정점 정보 크기		
		VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

		// 스테이징 버퍼 객체, 스테이징 버퍼 메모리 객체 생성
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;

		// [스테이징 버퍼 생성]
		// 용도)
		//		VK_BUFFER_USAGE_TRANSFER_SRC_BIT : 데이터 전송의 소스로 사용
		// 속성)
		// 		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT  : CPU에서 GPU 메모리에 접근이 가능한 설정
		// 		VK_MEMORY_PROPERTY_HOST_COHERENT_BIT : CPU에서 GPU 메모리의 값을 수정하면 그 즉시 GPU 메모리와 캐시에 해당 값을 수정하는 설정 
		//      									  (원래는 CPU에서 GPU 메모리 값을 수정하면 GPU 캐시를 플러쉬하여 다시 캐시에 값을 올리는 형식으로 동작)
		createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

		// [스테이징 버퍼(GPU 메모리)에 정점 정보 입력]
		void* data; // GPU 메모리에 매핑될 CPU 메모리 가상 포인터
		vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data); 	// GPU 메모리에 매핑된 CPU 메모리 포인터 반환 (현재는 버텍스 버퍼 메모리 전부를 매핑)
		memcpy(data, vertices.data(), (size_t) bufferSize);					// CPU 메모리 포인터는 가상포인터로 data에 정점 정보를 복사하면 GPU에 즉시 반영
																			// 실제 CPU 메모리에 저장되는게 아닌 CPU 메모리 포인터는 가상 포인터역할만 하고 GPU 메모리에 바로 저장
																			// 메모리 유형의 속성에 의해 GPU 캐시를 플러쉬할 필요없이 즉시 적용
																			// VK_MEMORY_PROPERTY_HOST_COHERENT_BIT 속성 덕분
																			// 만약 해당 속성 없이 플러쉬도 안 하면 GPU에서 변경사항이 바로 적용되지 않음
		vkUnmapMemory(device, stagingBufferMemory);							// GPU 메모리 매핑 해제


		// [버텍스 버퍼 생성]
		// 용도)
		//		VK_BUFFER_USAGE_TRANSFER_DST_BIT : 데이터 전송의 대상으로 사용
		// 속성)
		// 		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT : 버퍼를 정점 데이터를 저장하고 처리하는 용도로 설정.
		// 		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT : GPU 전용 메모리에 데이터를 저장하여, GPU가 최적화된 방식으로 접근할 수 있게 함.
		createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);

		// [스테이징 버퍼에서 버텍스 버퍼로 메모리 이동]
		copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

		// 스테이징 버퍼와 할당된 메모리 해제
		vkDestroyBuffer(device, stagingBuffer, nullptr);
		vkFreeMemory(device, stagingBufferMemory, nullptr);
	}

	/*
		[인덱스 버퍼 생성]
		버텍스 버퍼 생성 과정과 같음
	*/
	void createIndexBuffer() {
		VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

		void* data;
		vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, indices.data(), (size_t) bufferSize);
		vkUnmapMemory(device, stagingBufferMemory);

		// [버텍스 버퍼 생성]
		// 속성)
		// 		VK_BUFFER_USAGE_INDEX_BUFFER_BIT : 버퍼를 인덱스 데이터를 저장하고 처리하는 용도로 설정.
		// 		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT : GPU 전용 메모리에 데이터를 저장하여, GPU가 최적화된 방식으로 접근할 수 있게 함.
		createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);

		copyBuffer(stagingBuffer, indexBuffer, bufferSize);

		vkDestroyBuffer(device, stagingBuffer, nullptr);
		vkFreeMemory(device, stagingBufferMemory, nullptr);
	}

	// 유니폼 버퍼 생성
	void createUniformBuffers() {
		// 유니폼 버퍼에 저장 될 구조체의 크기
		VkDeviceSize bufferSize = sizeof(UniformBufferObject);

		// 각 요소들을 동시에 처리 가능한 최대 프레임 수만큼 만들어 둔다.
		uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);		// 유니폼 버퍼 객체
		uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);	// 유니폼 버퍼에 할당할 메모리
		uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);	// GPU 메모리에 매핑할 CPU 메모리 포인터

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			// 유니폼 버퍼 객체 생성 + 메모리 할당 + 바인딩
			createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i], uniformBuffersMemory[i]);
			// GPU 메모리 CPU 가상 포인터에 매핑
			vkMapMemory(device, uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);
		}
	}

	// 디스크립터 풀 생성
	void createDescriptorPool() {
		// 디스크립터 풀의 타입별 디스크립터 개수를 설정하는 구조체
		VkDescriptorPoolSize poolSize{};
		poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;							// 디스크립터 타입
		poolSize.descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);		// 디스크립터 개수 (프레임 1개당 디스크립터 1개 할당)

		// 디스크립터 풀을 생성할 때 필요한 설정 정보를 담는 구조체
		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = 1;													// 디스크립터 poolSize 구조체 개수
		poolInfo.pPoolSizes = &poolSize;											// 디스크립터 poolSize 구조체 배열
		poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);				// 풀에 존재할 수 있는 총 디스크립터 셋 개수

		// 디스크립터 풀 생성
		if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
			throw std::runtime_error("failed to create descriptor pool!");
		}
	}

	// 디스크립터 셋 할당 및 업데이트 하여 리소스 바인딩
	void createDescriptorSets() {
		// 디스크립터 셋 레이아웃 벡터 생성 (기존 만들어놨던 디스크립터 셋 레이아웃 객체 이용)
		std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);

		// 디스크립터 셋 할당에 필요한 정보를 설정하는 구조체
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descriptorPool;										// 디스크립터 셋을 할당할 디스크립터 풀 지정
		allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);		// 할당할 디스크립터 셋 개수 지정
		allocInfo.pSetLayouts = layouts.data();											// 할당할 디스크립터 셋 의 레이아웃을 정의하는 배열 

		descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);									// 디스크립터 셋을 저장할 벡터 크기 설정
		
		// 디스크립터 풀에 디스크립터 셋 할당
		if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate descriptor sets!");
		}

		// 디스크립터 셋마다 디스크립터 설정 진행
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			// 디스크립터 셋에 바인딩할 버퍼 정보 
			VkDescriptorBufferInfo bufferInfo{};
			bufferInfo.buffer = uniformBuffers[i];								// 바인딩할 버퍼
			bufferInfo.offset = 0;												// 버퍼에서 데이터 시작 위치 offset
			bufferInfo.range = sizeof(UniformBufferObject);						// 셰이더가 접근할 버퍼 크기

			// 디스크립터 셋 바인딩 및 업데이트
			VkWriteDescriptorSet descriptorWrite{};
			descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrite.dstSet = descriptorSets[i];							// 업데이트 할 디스크립터 셋
			descriptorWrite.dstBinding = 0;										// 업데이트 할 바인딩 포인트
			descriptorWrite.dstArrayElement = 0;								// 업데이트 할 디스크립터가 배열 타입인 경우 해당 배열의 원하는 index 부터 업데이트 가능 (배열 아니면 0으로 지정)
			descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;	// 업데이트 할 디스크립터 타입
			descriptorWrite.descriptorCount = 1;								// 업데이트 할 디스크립터 개수
			descriptorWrite.pBufferInfo = &bufferInfo;							// 업데이트 할 버퍼 디스크립터 정보 구조체 배열

			// 디스크립터 셋을 업데이트 하여 사용할 리소스 바인딩
			vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
		}
	}

	/*
		[버퍼 생성]
		1. 버퍼 객체 생성
		2. 버퍼 메모리 할당
		3. 버퍼 객체에 할당한 메모리 바인딩
	*/ 
	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
		// 버퍼 객체를 생성하기 위한 구조체 (GPU 메모리에 데이터 저장 공간을 할당하는 데 필요한 설정을 정의)
		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = size;										// 버퍼의 크기 지정
		bufferInfo.usage = usage;									// 버퍼의 용도 지정
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;			// 버퍼를 하나의 큐 패밀리에서 쓸지, 여러 큐 패밀리에서 공유할지 설정 (현재 단일 큐 패밀리에서 사용하도록 설정)
																	// 여러 큐 패밀리에서 공유하는 모드 사용시 추가 설정 필요
		// [버퍼 생성]
		// 버퍼를 생성하지만 할당은 안되어있는 상태로 만들어짐       
		if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
			throw std::runtime_error("failed to create buffer!");
		}

		// [버퍼에 메모리 할당]
		// 메모리 할당 요구사항 조회
		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

		// 메모리 할당을 위한 구조체
		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;	// 할당할 메모리 크기
		// 메모리 요구사항 설정 (GPU 메모리 유형 중 buffer와 호환되고 properties 속성들과 일치하는 것 찾아 저장)
		// 메모리 유형 - GPU 메모리는 구역마다 유형이 다르다. (memoryTypeBits는 buffer가 호환되는 GPU의 메모리 유형이 전부 담겨있음)
		// 메모리 유형의 속성 - 메모리 유형마다 특성을 가지고 있음
		allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

		// 버퍼 메모리 할당
		if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate buffer memory!");
		}

		// 버퍼 객체에 할당된 메모리를 바인딩 (4번째 매개변수는 할당할 메모리의 offset)
		vkBindBufferMemory(device, buffer, bufferMemory, 0);
	}

	// srcBuffer 에서 dstBuffer 로 데이터 복사
	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
		// 커맨드 버퍼 할당을 위한 구조체 
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = commandPool;	// 커맨드 풀 지정
		allocInfo.commandBufferCount = 1;		// 커맨드 버퍼 개수

		// 커맨드 버퍼 생성
		VkCommandBuffer commandBuffer;
		vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

		// 커맨드 버퍼 기록을 위한 정보 객체
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;  // 커맨드 버퍼를 1번만 제출

		// GPU에 필요한 작업을 모두 커맨드 버퍼에 기록하기 시작
		vkBeginCommandBuffer(commandBuffer, &beginInfo);

		VkBufferCopy copyRegion{}; 	// 복사할 버퍼 영역을 지정 (크기, src 와 dst의 시작 offset 등)
		copyRegion.size = size;		// 복사할 버퍼 크기 설정
		vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion); // 커맨드 버퍼에 복사 명령 기록

		// 커맨드 버퍼 기록 중지
		vkEndCommandBuffer(commandBuffer);

		// 복사 커맨드 버퍼 제출 정보 객체 생성
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;								// 커맨드 버퍼 개수
		submitInfo.pCommandBuffers = &commandBuffer;					// 커맨드 버퍼 등록

		vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);	// 커맨드 버퍼 큐에 제출
		vkQueueWaitIdle(graphicsQueue);									// 그래픽스 큐 작업 종료 대기

		vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);	// 커맨드 버퍼 제거
	}

	/*
		GPU와 buffer가 호환되는 메모리 유형중 properties에 해당하는 속성들을 갖는 메모리 유형 찾기
	*/
	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
		// GPU에서 사용한 메모리 유형을 가져온다.
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

		for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
			// typeFilter & (1 << i) : GPU의 메모리 유형중 버퍼와 호환되는 것인지 판단
			// memProperties.memoryTypes[i].propertyFlags & properties : GPU 메모리 유형의 속성이 properties와 일치하는지 판단
			if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
				// 해당 메모리 유형 반환
				return i;
			}
		}

		throw std::runtime_error("failed to find suitable memory type!");
	}

	/*
		[커맨드 버퍼 생성]
		커맨드 버퍼에 GPU에서 실행할 작업을 전부 기록한뒤 제출한다.
		GPU는 해당 커맨드 버퍼의 작업을 알아서 실행하고, CPU는 다른 일을 할 수 있게 된다. (병렬 처리)
	*/
	void createCommandBuffers() {
		// 동시에 처리할 프레임 버퍼 수만큼 커맨드 버퍼 생성
		commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

		// 커맨드 버퍼 설정값 준비
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = commandPool; 								// 커맨드 풀 등록
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;					// 큐에 직접 제출할 수 있는 커맨드 버퍼 설정
		allocInfo.commandBufferCount = (uint32_t) commandBuffers.size(); 	// 할당할 커맨드 버퍼의 개수

		// 커맨드 버퍼 할당
		if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate command buffers!");
		}
	}

	/*
		[커맨드 버퍼에 작업 기록]
		1. 커맨드 버퍼 기록 시작
		2. 렌더패스 시작하는 명령 기록
		3. 파이프라인 설정 명령 기록
		4. 렌더링 명령 기록
		5. 렌더 패스 종료 명령 기록
		6. 커맨드 버퍼 기록 종료
	*/
	void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
		
		// 커맨드 버퍼 기록을 위한 정보 객체
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		// GPU에 필요한 작업을 모두 커맨드 버퍼에 기록하기 시작
		if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
			throw std::runtime_error("failed to begin recording command buffer!");
		}

		// 렌더 패스 정보 지정
		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = renderPass;								// 렌더 패스 등록
		renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];		// 프레임 버퍼 등록
		renderPassInfo.renderArea.offset = {0, 0};							// 렌더링 시작 좌표 등록
		renderPassInfo.renderArea.extent = swapChainExtent;					// 렌더링 width, height 등록 (보통 프레임버퍼, 스왑체인의 크기와 같게 설정)

		VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};				
		renderPassInfo.clearValueCount = 1;									// clear color 개수 등록
		renderPassInfo.pClearValues = &clearColor;							// clear color 등록 (첨부한 attachment 개수와 같게 등록)
		
		/* 
			[렌더 패스를 시작하는 명령을 기록] 
			GPU에서 렌더링에 필요한 자원과 설정을 준비 (대략 과정)
			1. 렌더링 자원 초기화 (프레임 버퍼와 렌더 패스에 등록된 attachment layout 초기화)
			2. 서브패스 및 attachment 설정 적용
			3. 렌더링 작업을 위한 컨텍스트 준비 (뷰포트, 시저 등 설정)
		*/
		vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		//	[사용할 그래픽 파이프 라인을 설정하는 명령 기록]
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

		// 뷰포트 정보 입력
		VkViewport viewport{};
		viewport.x = 0.0f;									// 뷰포트의 시작 x 좌표
		viewport.y = 0.0f;									// 뷰포트의 시작 y 좌표
		viewport.width = (float) swapChainExtent.width;		// 뷰포트의 width 크기
		viewport.height = (float) swapChainExtent.height;	// 뷰포트의 height 크기
		viewport.minDepth = 0.0f;							// 뷰포트의 최소 깊이
		viewport.maxDepth = 1.0f;							// 뷰포트의 최대 깊이
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);	// [커맨드 버퍼에 뷰포트 설정 등록]

		// 시저 정보 입력
		VkRect2D scissor{};
		scissor.offset = {0, 0};							// 시저의 시작 좌표
		scissor.extent = swapChainExtent;					// 시저의 width, height
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);		// [커맨드 버퍼에 시저 설정 등록]

		// 버텍스 정보 입력
		VkBuffer vertexBuffers[] = {vertexBuffer};			// 버택스 버퍼 객체
		VkDeviceSize offsets[] = {0};						// 버텍스 버퍼 메모리의 시작 위치 offset
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets); // 커맨드 버퍼에 버텍스 버퍼 바인딩

		// 인덱스 정보 입력
		vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16); // 커맨드 버퍼에 인덱스 버퍼 바인딩 (4번째 매개변수 index 데이터 타입 uint16 설정)

		// 디스크립터 셋을 커맨드 버퍼에 바인딩
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);

		// [Drawing 작업을 요청하는 명령 기록]
		vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0); // index로 drawing 하는 명령 기록

		/*
			[렌더 패스 종료]
			1. 자원의 정리 및 레이아웃 전환 (최종 작업을 위해 attachment에 정의된 finalLayout 설정)
			2. Load, Store 작업 (각 attachment에 정해진 load, store 작업 실행)
			3. 렌더 패스의 종료를 GPU에 알려 자원 재활용 등이 가능해짐
		*/ 
		vkCmdEndRenderPass(commandBuffer);

		// [커맨드 버퍼 기록 종료]
		if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
			throw std::runtime_error("failed to record command buffer!");
		}
	}

	/*
		[동기화 오브젝트 생성]
		세마포어 - GPU, GPU 작업간 동기화
		펜스 - CPU, GPU 작업간 동기화
	*/
	void createSyncObjects() {
		// 세마포어, 펜스 vector 동시에 처리할 최대 프레임 버퍼 수만큼 할당
		imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

		// 세마포어 생성 설정 값 준비
		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		// 펜스 생성 설정 값 준비
		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;         // signal 등록된 상태로 생성 (시작하자마자 wait으로 시작하므로 필요한 FLAG)

		// 세마포어, 펜스 생성
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
				vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
				vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
				throw std::runtime_error("failed to create synchronization objects for a frame!");
			}
		}
	}

	// Uniform 변수에 해당하는 값을 구한 후 매핑된 GPU 메모리에 복사
    void updateUniformBuffer(uint32_t currentImage) {
        static auto startTime = std::chrono::high_resolution_clock::now();

        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

		// 이번 프레임의 유니폼 변수 값 구하기
		// 1초에 90도씩 회전하는 model view projection 변환 생성
        UniformBufferObject ubo{};
        ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.proj = glm::perspective(glm::radians(45.0f), swapChainExtent.width / (float) swapChainExtent.height, 0.1f, 10.0f);
        ubo.proj[1][1] *= -1;

		// 유니폼 변수를 매핑된 GPU 메모리에 복사
        memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
    }

	/*
		[다중 Frame 방식으로 그리기]
		동시에 작업 가능한 최대 Frame 개수만큼 자원을 생성하여 사용 (semaphore, fence, commandBuffer)
		작업할 Frame에 대한 커맨드 버퍼에 명령을 기록하여 GPU에 작업들을(렌더링 + 프레젠테이션) 맡기고 다음 Frame을 Draw 하러 이동
		Frame 작업을 병렬로 실행 (최대 Frame 개수의 작업이 진행 중이면 다음 작업은 Fence의 signal을 기다리며 대기)
	*/
	void drawFrame() {
		// [이전 GPU 작업 대기]
		// 동시에 작업 가능한 최대 Frame 개수만큼 작업 중인 경우 대기 (가장 먼저 시작한 Frame 작업이 끝나서 Fence에 signal을 보내기를 기다림)
		vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
 
		// [작업할 image 준비]
		// 이번 Frame 에서 사용할 이미지 준비 및 해당 이미지 index 받아오기 (준비가 끝나면 signal 보낼 세마포어 등록)
		// vkAcquireNextImageKHR 함수는 CPU에서 swapChain과 surface의 호환성을 확인하고 GPU에 이미지 준비 명령을 내리는 함수
		// 만약 image가 프레젠테이션 큐에 작업이 진행 중이거나 대기 중이면 해당 image는 사용하지 않고 대기한다.
		uint32_t imageIndex;
		VkResult result = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

		// image 준비 실패로 인한 오류 처리
		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			// 스왑 체인이 surface 크기와 호환되지 않는 경우로(창 크기 변경), 스왑 체인 재생성 후 다시 draw
			recreateSwapChain();
			return;
		} else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
			// 진짜 오류 gg
			throw std::runtime_error("failed to acquire swap chain image!");
		}

		// Uniform buffer 업데이트
		updateUniformBuffer(currentFrame);

		// [Fence 초기화]
		// Fence signal 상태 not signaled 로 초기화
		vkResetFences(device, 1, &inFlightFences[currentFrame]);

		// [Command Buffer에 명령 기록]
		// 커맨드 버퍼 초기화 및 명령 기록
		vkResetCommandBuffer(commandBuffers[currentFrame], /*VkCommandBufferResetFlagBits*/ 0); // 두 번째 매개변수인 Flag 를 0으로 초기화하면 기본 초기화 진행
		recordCommandBuffer(commandBuffers[currentFrame], imageIndex); // 현재 작업할 image의 index와 commandBuffer를 전송

		// [렌더링 Command Buffer 제출]
		// 렌더링 커맨드 버퍼 제출 정보 객체 생성
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		// 작업 실행 신호를 받을 대기 세마포어 설정 (해당 세마포어가 signal 상태가 되기 전엔 대기)
		VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};				
		VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT}; 	
		submitInfo.waitSemaphoreCount = 1;														// 대기 세마포어 개수
		submitInfo.pWaitSemaphores = waitSemaphores;											// 대기 세마포어 등록
		submitInfo.pWaitDstStageMask = waitStages;												// 대기할 시점 등록 (그 전까지는 세마포어 상관없이 그냥 진행)	

		// 커맨드 버퍼 등록
		submitInfo.commandBufferCount = 1;														// 커맨드 버퍼 개수 등록
		submitInfo.pCommandBuffers = &commandBuffers[currentFrame];								// 커매드 버퍼 등록

		// 작업이 완료된 후 신호를 보낼 세마포어 설정 (작업이 끝나면 해당 세마포어 signal 상태로 변경)
		VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
		submitInfo.signalSemaphoreCount = 1;													// 작업 끝나고 신호를 보낼 세마포어 개수
		submitInfo.pSignalSemaphores = signalSemaphores;										// 작업 끝나고 신호를 보낼 세마포어 등록

		// 커맨드 버퍼 제출
		if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
			throw std::runtime_error("failed to submit draw command buffer!");
		}

		// [프레젠테이션 Command Buffer 제출]
		// 프레젠테이션 커맨드 버퍼 제출 정보 객체 생성
		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		// 작업 실행 신호를 받을 대기 세마포어 설정
		presentInfo.waitSemaphoreCount = 1;														// 대기 세마포어 개수
		presentInfo.pWaitSemaphores = signalSemaphores;											// 대기 세마포어 등록

		// 제출할 스왑 체인 설정
		VkSwapchainKHR swapChains[] = {swapChain};
		presentInfo.swapchainCount = 1;															// 스왑체인 개수
		presentInfo.pSwapchains = swapChains;													// 스왑체인 등록
		presentInfo.pImageIndices = &imageIndex;												// 스왑체인에서 표시할 이미지 핸들 등록

		// 프레젠테이션 큐에 이미지 제출
		result = vkQueuePresentKHR(presentQueue, &presentInfo);

		// 프레젠테이션 실패 오류 발생 시
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
			// 스왑 체인 크기와 surface의 크기가 호환되지 않는 경우
			framebufferResized = false;
			recreateSwapChain(); 	// 변경된 surface에 맞는 SwapChain, ImageView, FrameBuffer 생성 
		} else if (result != VK_SUCCESS) {
			// 진짜 오류 gg
			throw std::runtime_error("failed to present swap chain image!");
		}

		// [프레임 인덱스 증가]
		// 다음 작업할 프레임 변경
		currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	}

	/*
	매개변수로 받은 쉐이더 파일을 shader module로 만들어 줌
	shader module은 쉐이더 파일을 객체화 한 것임
	*/ 
	VkShaderModule createShaderModule(const std::vector<char>& code) {
		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size();									// 코드 길이 입력
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());	// 코드 내용 입력

		// 쉐이더 모듈 생성
		VkShaderModule shaderModule;
		if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
			throw std::runtime_error("failed to create shader module!");
		}

		return shaderModule;
	}

	/* 
	지원하는 포맷중 선호하는 포맷 1개 반환
	선호하는 포맷이 없을 시 가장 앞에 있는 포맷 반환
	*/
	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
		for (const auto& availableFormat : availableFormats) {
			// 만약 선호하는 포맷이 존재할 경우 그 포맷을 반환
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
				availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
				return availableFormat;
			}
		}
		// 선호하는 포맷이 없는 경우 첫 번째 포맷 반환
		return availableFormats[0];
	}

	/*
	지원하는 프레젠테이션 모드 중 선호하는 모드 선택
	선호하는 모드가 없을 시 기본 값인 VK_PRESENT_MODE_FIFO_KHR 반환
	*/
	VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
		for (const auto& availablePresentMode : availablePresentModes) {
			// 선호하는 mode가 존재하면 해당 mode 반환 
			if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
				return availablePresentMode;
			}
		}
		// 선호하는 mode가 존재하지 않으면 기본 값인 VK_PRESENT_MODE_FIFO_KHR 반환
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	// 스왑 체인 이미지의 해상도 결정
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
		// 만약 currentExtent의 width가 std::numeric_limits<uint32_t>::max()가 아니면, 시스템이 이미 권장하는 스왑체인 크기를 제공하는 것
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
			return capabilities.currentExtent; // 권장 사항 사용
		} else {
			// 그렇지 않은 경우, 창의 현재 프레임 버퍼 크기를 사용하여 스왑체인 크기를 결정
			int width, height;
			glfwGetFramebufferSize(window, &width, &height);

			VkExtent2D actualExtent = {
				static_cast<uint32_t>(width),
				static_cast<uint32_t>(height)
			};

			// width, height를 capabilites의 min, max 범위로 clamping
			actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
			actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

			return actualExtent;
		}
	}
	
	// GPU와 surface가 호환하는 SwapChain 정보를 반환
	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {
		SwapChainSupportDetails details;

		// GPU와 surface가 호환할 수 있는 capability 정보 쿼리
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

		// device에서 surface 객체를 지원하는 format이 존재하는지 확인 
		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

		if (formatCount != 0) {
			details.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
		}

		// device에서 surface 객체를 지원하는 presentMode가 있는지 확인 
		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

		if (presentModeCount != 0) {
			details.presentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
		}

		return details;
	}

	// 그래픽, 프레젠테이션 큐 패밀리가 존재하는지, GPU와 surface가 호환하는 SwapChain이 존재하는지 검사
	bool isDeviceSuitable(VkPhysicalDevice device) {
		// 큐 패밀리 확인
		QueueFamilyIndices indices = findQueueFamilies(device);
		
		// 스왑 체인 확장을 지원하는지 확인
		bool extensionsSupported = checkDeviceExtensionSupport(device);
		bool swapChainAdequate = false;

		// 스왑 체인 확장이 존재하는 경우
		if (extensionsSupported) {
			// 물리 디바이스와 surface가 호환하는 SwapChain 정보를 가져옴
			SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
			// GPU와 surface가 지원하는 format과 presentMode가 존재하면 통과
			swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
		}

		return indices.isComplete() && extensionsSupported && swapChainAdequate;
	}

	// 디바이스가 지원하는 확장 중 
	bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

		// 스왑 체인 확장이 존재하는지 확인
		std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
		for (const auto& extension : availableExtensions) {
			// 지원 가능한 확장들 목록을 순회하며 제거
			requiredExtensions.erase(extension.extensionName);
		}

		// 만약 기존에 있던 확장이 제거되면 true
		// 기존에 있던 확장이 그대로면 지원을 안 하는 것이므로 false
		return requiredExtensions.empty();
	}

	/*
	GPU가 지원하는 큐패밀리 인덱스 가져오기
	그래픽스 큐패밀리, 프레젠테이션 큐패밀리 인덱스를 저장
	해당 큐패밀리가 없으면 optional 객체에 정보가 empty 상태
	*/
	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
		QueueFamilyIndices indices;

		// GPU가 지원하는 큐 패밀리 개수 가져오기
		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		// GPU가 지원하는 큐 패밀리 리스트 가져오기
		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		// 그래픽 큐 패밀리 검색
		int i = 0;
		for (const auto& queueFamily : queueFamilies) {
			// 그래픽 큐 패밀리 찾기 성공한 경우 indices에 값 생성
			if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				indices.graphicsFamily = i;
			}

			// GPU의 i 인덱스 큐 패밀리가 surface에서 프레젠테이션을 지원하는지 확인
			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

			// 프레젠테이션 큐 패밀리 등록
			if (presentSupport) {
				indices.presentFamily = i;
			}

			// 그래픽 큐 패밀리 찾은 경우 break
			if (indices.isComplete()) {
				break;
			}

			i++;
		}
		// 그래픽 큐 패밀리를 못 찾은 경우 값이 없는 채로 반환 됨
		return indices;
	}

	/*
	GLFW 라이브러리에서 Vulkan 인스턴스를 생성할 때 필요한 인스턴스 확장 목록을 반환
	(디버깅 모드시 메시지 콜백 확장 추가)
	*/
	std::vector<const char*> getRequiredExtensions() {
		// 필요한 확장 목록 가져오기
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
		std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

		// 디버깅 모드이면 VK_EXT_debug_utils 확장 추가 (메세지 콜백 확장)
		if (enableValidationLayers) {
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}
		
		return extensions;
	}

	// 검증 레이어가 사용 가능한 레이어 목록에 있는지 확인
	bool checkValidationLayerSupport() {
		// Vulkan 인스턴스에서 사용 가능한 레이어들 목록 생성
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		// 필요한 검증 레이어들이 사용 가능 레이어에 포함 되어있는지 확인
		for (const char* layerName : validationLayers) {
			bool layerFound = false;
			for (const auto& layerProperties : availableLayers) {
				if (strcmp(layerName, layerProperties.layerName) == 0) {
					// 포함 확인!
					layerFound = true;
					break;
				}
			}
			if (!layerFound) {
				return false;  // 필요한 레이어가 없다면 false 반환
			}
		}
		return true;  // 모든 레이어가 지원되면 true 반환
	}

	// shader 파일인 SPIR-V 파일을 바이너리 형태로 읽어오는 함수
	static std::vector<char> readFile(const std::string& filename) {
		std::ifstream file(filename, std::ios::ate | std::ios::binary);

		if (!file.is_open()) {
			throw std::runtime_error("failed to open file!");
		}

		size_t fileSize = (size_t) file.tellg();
		std::vector<char> buffer(fileSize);

		file.seekg(0);
		file.read(buffer.data(), fileSize);

		file.close();

		return buffer;
	}

	// 디버그 메시지 콜백 함수
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback( VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
														VkDebugUtilsMessageTypeFlagsEXT messageType,
														const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
														void* pUserData) {
		// 메시지 내용만 출력
		std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
		// VK_TRUE 반환시 프로그램 종료됨
		return VK_FALSE;
	}
};

int main() {
	HelloTriangleApplication app;

	try {
		app.run();
	} catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}