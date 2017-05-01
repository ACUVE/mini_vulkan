#include "VDeleter.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <fstream>
#include <glm/glm.hpp>
#include <iostream>
#include <vector>
#include <vulkan/vulkan.hpp>

#ifdef _MSC_VER
#pragma comment( lib, "vulkan-1" )
#pragma comment( lib, "glfw3" )
#endif

#ifdef NDEBUG
constexpr bool DEBUG_MODE = false;
#else
constexpr bool DEBUG_MODE = true;
#endif
constexpr std::size_t INVALID_INDEX = std::numeric_limits< std::size_t >::max();
constexpr unsigned int WIDTH = 800;
constexpr unsigned int HEIGHT = 600;

static VKAPI_ATTR vk::Bool32 VKAPI_CALL debug_callback(
    VkDebugReportFlagsEXT flags,
    VkDebugReportObjectTypeEXT objectType,
    std::uint64_t object,
    std::size_t location,
    std::int32_t messageCode,
    const char *pLayerPrefix,
    const char *pMessage,
    void *pUserData )
{
    std::clog << "-----------------------------" << std::endl;
    std::clog << pMessage << std::endl;

    return VK_TRUE;
}

vk::UniqueInstance create_instance(
    std::vector< char const * > const extension_names,
    std::vector< char const * > const layer_names )
{
    auto const instance_extension_properties =
        vk::enumerateInstanceExtensionProperties();
    std::clog << "Extensions:" << std::endl;
    for( auto const &v : instance_extension_properties )
        std::clog << v.extensionName << ": specVersion = " << v.specVersion
                  << std::endl;
    auto const instance_layer_properties =
        vk::enumerateInstanceLayerProperties();
    std::clog << "Layers:" << std::endl;
    for( auto const &v : instance_layer_properties )
        std::clog << v.layerName << ": specVersion = " << v.specVersion
                  << " : implementationVersion = " << v.implementationVersion
                  << " : description" << v.description << std::endl;
    std::clog << "---------------------------------------" << std::endl;

    vk::ApplicationInfo app_info;
    app_info.pApplicationName = "Test Vulkan";
    app_info.applicationVersion = VK_MAKE_VERSION( 1, 0, 0 );
    app_info.pEngineName = "No Engine";
    app_info.engineVersion = VK_MAKE_VERSION( 1, 0, 0 );
    app_info.apiVersion = VK_API_VERSION_1_0;
    vk::InstanceCreateInfo create_info;
    create_info.pApplicationInfo = &app_info;
    if( !extension_names.empty() )
    {
        create_info.enabledExtensionCount =
            static_cast< std::uint32_t >( extension_names.size() );
        create_info.ppEnabledExtensionNames = extension_names.data();
    }
    if( !layer_names.empty() )
    {
        create_info.enabledLayerCount =
            static_cast< std::uint32_t >( layer_names.size() );
        create_info.ppEnabledLayerNames = layer_names.data();
    }
    return vk::createInstanceUnique( create_info );
}

VDeleter< VkDebugReportCallbackEXT > create_debug_report(
    vk::Instance instance, PFN_vkDebugReportCallbackEXT callback )
{
    VDeleter< VkDebugReportCallbackEXT > dbg_callback;
    auto const createfunc =
        reinterpret_cast< PFN_vkCreateDebugReportCallbackEXT >(
            instance.getProcAddr( "vkCreateDebugReportCallbackEXT" ) );
    auto const deletefunc =
        reinterpret_cast< PFN_vkDestroyDebugReportCallbackEXT >(
            instance.getProcAddr( "vkDestroyDebugReportCallbackEXT" ) );
    if( createfunc && deletefunc )
    {
        dbg_callback = decltype( dbg_callback )(
            static_cast< VkInstance >( instance ), deletefunc );
        vk::DebugReportCallbackCreateInfoEXT dbg_callback_create_info;
        dbg_callback_create_info.flags =
            vk::DebugReportFlagBitsEXT::eInformation |
            vk::DebugReportFlagBitsEXT::eWarning |
            vk::DebugReportFlagBitsEXT::ePerformanceWarning |
            vk::DebugReportFlagBitsEXT::eError |
            vk::DebugReportFlagBitsEXT::eDebug;
        dbg_callback_create_info.pfnCallback = callback;
        // 返り値チェックすべき
        createfunc(
            static_cast< VkInstance >( instance ),
            reinterpret_cast< VkDebugReportCallbackCreateInfoEXT const * >(
                &dbg_callback_create_info ),
            nullptr,
            dbg_callback.replace() );
    }
    return std::move( dbg_callback );
}

vk::UniqueSurfaceKHR create_glfw_surface(
    vk::Instance instance,
    GLFWwindow *window,
    vk::Optional< const vk::AllocationCallbacks > allocator = nullptr )
{
    vk::SurfaceKHR surface;
    vk::Result result = static_cast< vk::Result >( glfwCreateWindowSurface(
        static_cast< VkInstance >( instance ),
        window,
        reinterpret_cast< const VkAllocationCallbacks * >(
            static_cast< const vk::AllocationCallbacks * >( allocator ) ),
        reinterpret_cast< VkSurfaceKHR * >( &surface ) ) );
    vk::SurfaceKHRDeleter deleter( instance, allocator );
    return vk::UniqueSurfaceKHR(
        createResultValue( result, surface, "create_glfw_surface" ), deleter );
}

std::size_t select_best_physical_device_index(
    std::vector< vk::PhysicalDevice > const &devs )
{
    assert( !devs.empty() );
    for( std::size_t i = 0u; i < devs.size(); ++i )
    {
        auto const &dev = devs[ i ];
        auto device_properties = dev.getProperties();
        auto device_features = dev.getFeatures();
        if( device_features.geometryShader &&
            device_properties.deviceType != vk::PhysicalDeviceType::eCpu )
        {
            return i;
        }
    }
    throw std::runtime_error( "select_best_physical_device_index: no device" );
}
std::size_t select_graphics_queue_family_index(
    std::vector< vk::QueueFamilyProperties > queue_familes )
{
    for( std::size_t i = 0u; i < queue_familes.size(); ++i )
    {
        auto &p = queue_familes[ i ];
        if( p.queueCount > 0 && p.queueFlags & vk::QueueFlagBits::eGraphics )
        {
            return i;
        }
    }
    throw std::runtime_error( "select_graphics_queue_family_index: no queue" );
}
std::size_t select_surface_queue_family_index(
    vk::PhysicalDevice device,
    vk::SurfaceKHR surface,
    std::vector< vk::QueueFamilyProperties > queue_familes )
{
    for( std::uint32_t i = 0u; i < queue_familes.size(); ++i )
    {
        auto &p = queue_familes[ i ];
        if( p.queueCount > 0 && device.getSurfaceSupportKHR( i, surface ) )
        {
            return i;
        }
    }
    throw std::runtime_error( "select_surface_queue_family_index: no queue" );
}
vk::SurfaceFormatKHR select_surface_format(
    std::vector< vk::SurfaceFormatKHR > const &surface_formats )
{
    if( surface_formats.size() == 1u &&
        surface_formats[ 0 ].format == vk::Format::eUndefined )
    {
        return {vk::Format::eB8G8R8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear};
    }
    for( auto const &format : surface_formats )
    {
        if( format.format == vk::Format::eB8G8R8Unorm &&
            format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear )
        {
            return format;
        }
    }
    return surface_formats[ 0 ];
}
vk::PresentModeKHR select_surface_present_mode(
    std::vector< vk::PresentModeKHR > const &surface_present_modes )
{
    for( auto const &present_mode : surface_present_modes )
    {
        if( present_mode == vk::PresentModeKHR::eMailbox )
        {
            return present_mode;
        }
    }
    return vk::PresentModeKHR::eFifo;
}
vk::Extent2D
calc_surface_extent( vk::SurfaceCapabilitiesKHR const &surface_capabilities )
{
    if( surface_capabilities.currentExtent.width !=
        std::numeric_limits< std::uint32_t >::max() )
    {
        return surface_capabilities.currentExtent;
    }
    vk::Extent2D extent;
    extent.width = std::min(
        std::max( WIDTH, surface_capabilities.minImageExtent.width ),
        surface_capabilities.maxImageExtent.width );
    extent.height = std::min(
        std::max( HEIGHT, surface_capabilities.minImageExtent.height ),
        surface_capabilities.maxImageExtent.height );
    return extent;
}

std::vector< char > read_file( std::string const &filename )
{
    std::ifstream file( filename, std::ios::ate | std::ios::binary );
    if( !file.is_open() )
    {
        throw std::runtime_error( "read_file: failed to open file!" );
    }
    std::size_t filesize = file.tellg();
    std::vector< char > buffer( filesize );
    file.seekg( 0u );
    file.read( buffer.data(), filesize );
    return std::move( buffer );
}

vk::UniqueShaderModule
create_shader_module( vk::Device device, std::vector< char > const &code )
{
    vk::ShaderModuleCreateInfo shader_module_info;
    shader_module_info.codeSize = code.size();
    std::vector< std::uint32_t > code_aligned(
        code.size() / sizeof( std::uint32_t ) + 1 );
    std::memcpy( code_aligned.data(), code.data(), code.size() );
    shader_module_info.pCode = code_aligned.data();
    return device.createShaderModuleUnique( shader_module_info );
}

int main() try
{
    glfwInit();

    glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );
    auto window = glfwCreateWindow( WIDTH, HEIGHT, "Vulkan", nullptr, nullptr );

    std::uint32_t glfw_extension_count;
    auto const glfw_extension_names =
        glfwGetRequiredInstanceExtensions( &glfw_extension_count );
    std::vector< char const * > extension_names( glfw_extension_count );
    for( auto i = 0u; i < glfw_extension_count; ++i )
        extension_names[ i ] = glfw_extension_names[ i ];
    if( DEBUG_MODE ) extension_names.push_back( "VK_EXT_debug_report" );
    std::vector< char const * > layer_names;
    if( DEBUG_MODE )
        layer_names.push_back( "VK_LAYER_LUNARG_standard_validation" );
    auto instance = create_instance( extension_names, layer_names );

    VDeleter< VkDebugReportCallbackEXT > dbg_callback;
    if( DEBUG_MODE )
        dbg_callback = create_debug_report( *instance, debug_callback );

    auto surface = create_glfw_surface( *instance, window );

    std::vector< char const * > device_extension_names = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    auto devices = instance->enumeratePhysicalDevices();
    auto device_index = select_best_physical_device_index( devices );
    auto &device = devices[ device_index ];
    auto queue_familes = device.getQueueFamilyProperties();
    auto graphics_family_index =
        select_graphics_queue_family_index( queue_familes );
    auto surface_family_index =
        select_surface_queue_family_index( device, *surface, queue_familes );

    vk::DeviceQueueCreateInfo queue_info[ 2 ];
    float queue_priority = 1.0f;
    queue_info[ 0 ].queueFamilyIndex =
        static_cast< std::uint32_t >( graphics_family_index );
    queue_info[ 0 ].queueCount = 1;
    queue_info[ 0 ].pQueuePriorities = &queue_priority;
    queue_info[ 1 ].queueFamilyIndex =
        static_cast< std::uint32_t >( surface_family_index );
    queue_info[ 1 ].queueCount = 1;
    queue_info[ 1 ].pQueuePriorities = &queue_priority;

    vk::PhysicalDeviceFeatures device_features;

    vk::DeviceCreateInfo device_info;
    device_info.pQueueCreateInfos = queue_info;
    device_info.queueCreateInfoCount =
        graphics_family_index == surface_family_index ? 1 : 2;
    device_info.pEnabledFeatures = &device_features;
    if( !device_extension_names.empty() )
    {
        device_info.enabledExtensionCount =
            static_cast< std::uint32_t >( device_extension_names.size() );
        device_info.ppEnabledExtensionNames = device_extension_names.data();
    }
    // 本当にいる？
    if( !layer_names.empty() )
    {
        device_info.enabledLayerCount =
            static_cast< std::uint32_t >( layer_names.size() );
        device_info.ppEnabledLayerNames = layer_names.data();
    }
    auto ldevice = device.createDeviceUnique( device_info );
    auto graphics_queue = ldevice->getQueue(
        static_cast< std::uint32_t >( graphics_family_index ), 0 );
    auto surface_queue = ldevice->getQueue(
        static_cast< std::uint32_t >( surface_family_index ), 0 );

    auto surface_capabilities = device.getSurfaceCapabilitiesKHR( *surface );
    auto surface_formats = device.getSurfaceFormatsKHR( *surface );
    auto surface_present_modes = device.getSurfacePresentModesKHR( *surface );

    auto surface_format = select_surface_format( surface_formats );
    auto surface_present_mode =
        select_surface_present_mode( surface_present_modes );
    auto surface_extent = calc_surface_extent( surface_capabilities );
    std::uint32_t image_count = surface_capabilities.minImageCount + 1;
    if( surface_capabilities.maxImageCount > 0 )
    {
        image_count =
            std::min( image_count, surface_capabilities.maxImageCount );
    }
    vk::SwapchainCreateInfoKHR swapchain_info;
    swapchain_info.surface = *surface;
    swapchain_info.minImageCount = image_count;
    swapchain_info.imageFormat = surface_format.format;
    swapchain_info.imageColorSpace = surface_format.colorSpace;
    swapchain_info.imageExtent = surface_extent;
    swapchain_info.imageArrayLayers = 1u;
    swapchain_info.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
    std::uint32_t indices[] = {
        static_cast< std::uint32_t >( graphics_family_index ),
        static_cast< std::uint32_t >( surface_family_index )};
    if( graphics_family_index == surface_family_index )
    {
        swapchain_info.imageSharingMode = vk::SharingMode::eExclusive;
    }
    else
    {
        swapchain_info.imageSharingMode = vk::SharingMode::eConcurrent;
        swapchain_info.queueFamilyIndexCount = 2;
        swapchain_info.pQueueFamilyIndices = indices;
    }
    swapchain_info.preTransform = surface_capabilities.currentTransform;
    swapchain_info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    swapchain_info.presentMode = surface_present_mode;
    swapchain_info.clipped = VK_TRUE;
    swapchain_info.oldSwapchain = nullptr;
    auto swapchain = ldevice->createSwapchainKHRUnique( swapchain_info );
    auto images = ldevice->getSwapchainImagesKHR( *swapchain );

    std::vector< vk::UniqueImageView > image_views( images.size() );
    for( std::size_t i = 0u; i < image_views.size(); ++i )
    {
        vk::ImageViewCreateInfo image_view_info;
        image_view_info.image = images[ i ];
        image_view_info.viewType = vk::ImageViewType::e2D;
        image_view_info.format = surface_format.format;
        image_view_info.components.r = vk::ComponentSwizzle::eIdentity;
        image_view_info.components.g = vk::ComponentSwizzle::eIdentity;
        image_view_info.components.b = vk::ComponentSwizzle::eIdentity;
        image_view_info.components.a = vk::ComponentSwizzle::eIdentity;
        image_view_info.subresourceRange.aspectMask =
            vk::ImageAspectFlagBits::eColor;
        image_view_info.subresourceRange.baseMipLevel = 0u;
        image_view_info.subresourceRange.levelCount = 1u;
        image_view_info.subresourceRange.baseArrayLayer = 0u;
        image_view_info.subresourceRange.layerCount = 1u;
        image_views[ i ] =
            ldevice->createImageViewUnique( image_view_info, nullptr );
    }

    auto vertexshader = read_file( "vert.spv" );
    auto fragmentshader = read_file( "frag.spv" );
    auto vertexshader_module = create_shader_module( *ldevice, vertexshader );
    auto fragmentshader_module =
        create_shader_module( *ldevice, fragmentshader );

    vk::PipelineShaderStageCreateInfo pipeline_shader_stage_info[ 2 ];
    pipeline_shader_stage_info[ 0 ].stage = vk::ShaderStageFlagBits::eVertex;
    pipeline_shader_stage_info[ 0 ].module = *vertexshader_module;
    pipeline_shader_stage_info[ 0 ].pName = "main";
    pipeline_shader_stage_info[ 1 ].stage = vk::ShaderStageFlagBits::eFragment;
    pipeline_shader_stage_info[ 1 ].module = *fragmentshader_module;
    pipeline_shader_stage_info[ 1 ].pName = "main";

    vk::PipelineVertexInputStateCreateInfo pipeline_vertex_imput_state_info;
    pipeline_vertex_imput_state_info.vertexBindingDescriptionCount = 0;
    pipeline_vertex_imput_state_info.vertexAttributeDescriptionCount = 0;

    vk::PipelineInputAssemblyStateCreateInfo pipeline_input_assembly_state_info;
    pipeline_input_assembly_state_info.topology =
        vk::PrimitiveTopology::eTriangleList;
    pipeline_input_assembly_state_info.primitiveRestartEnable = VK_FALSE;

    vk::Viewport viewport;
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast< float >( surface_extent.width );
    viewport.height = static_cast< float >( surface_extent.height );
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    vk::Rect2D scissor;
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent = surface_extent;

    vk::PipelineViewportStateCreateInfo pipeline_viewport_state_info;
    pipeline_viewport_state_info.viewportCount = 1u;
    pipeline_viewport_state_info.pViewports = &viewport;
    pipeline_viewport_state_info.scissorCount = 1u;
    pipeline_viewport_state_info.pScissors = &scissor;

    vk::PipelineRasterizationStateCreateInfo pipeline_rasterization_state_info;
    pipeline_rasterization_state_info.depthClampEnable = VK_FALSE;
    pipeline_rasterization_state_info.polygonMode = vk::PolygonMode::eFill;
    pipeline_rasterization_state_info.lineWidth = 1.0f;
    pipeline_rasterization_state_info.cullMode = vk::CullModeFlagBits::eBack;
    pipeline_rasterization_state_info.frontFace = vk::FrontFace::eClockwise;
    pipeline_rasterization_state_info.depthBiasEnable = VK_FALSE;

    vk::PipelineMultisampleStateCreateInfo pipeline_multisample_state_info;
    pipeline_multisample_state_info.sampleShadingEnable = VK_FALSE;
    pipeline_multisample_state_info.rasterizationSamples =
        vk::SampleCountFlagBits::e1;

    vk::PipelineColorBlendAttachmentState pipeline_color_blend_attachment_state;
    pipeline_color_blend_attachment_state.colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    pipeline_color_blend_attachment_state.blendEnable = VK_FALSE;

    vk::PipelineColorBlendStateCreateInfo pipeline_color_blend_state_info;
    pipeline_color_blend_state_info.logicOpEnable = VK_FALSE;
    pipeline_color_blend_state_info.attachmentCount = 1u;
    pipeline_color_blend_state_info.pAttachments =
        &pipeline_color_blend_attachment_state;

    vk::PipelineLayoutCreateInfo pipeline_layout_info;
    auto pipeline_layout =
        ldevice->createPipelineLayoutUnique( pipeline_layout_info );

    vk::AttachmentDescription attachment_description;
    attachment_description.format = surface_format.format;
    attachment_description.samples = vk::SampleCountFlagBits::e1;
    attachment_description.loadOp = vk::AttachmentLoadOp::eClear;
    attachment_description.storeOp = vk::AttachmentStoreOp::eStore;
    attachment_description.initialLayout = vk::ImageLayout::eUndefined;
    attachment_description.finalLayout = vk::ImageLayout::ePresentSrcKHR;

    vk::AttachmentReference attachment_reference;
    attachment_reference.attachment = 0u;
    attachment_reference.layout = vk::ImageLayout::eColorAttachmentOptimal;

    vk::SubpassDescription subpass_description;
    subpass_description.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass_description.colorAttachmentCount = 1u;
    subpass_description.pColorAttachments = &attachment_reference;

    vk::RenderPassCreateInfo render_pass_info;
    render_pass_info.attachmentCount = 1u;
    render_pass_info.pAttachments = &attachment_description;
    render_pass_info.subpassCount = 1u;
    render_pass_info.pSubpasses = &subpass_description;
    vk::SubpassDependency subpass_dependency;
    subpass_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    subpass_dependency.dstSubpass = 0u;
    subpass_dependency.srcStageMask =
        vk::PipelineStageFlagBits::eColorAttachmentOutput;
    // subpass_dependency.srcAccessMask = 0;
    subpass_dependency.dstStageMask =
        vk::PipelineStageFlagBits::eColorAttachmentOutput;
    subpass_dependency.dstAccessMask =
        vk::AccessFlagBits::eColorAttachmentRead |
        vk::AccessFlagBits::eColorAttachmentRead;
    render_pass_info.dependencyCount = 1u;
    render_pass_info.pDependencies = &subpass_dependency;
    auto render_pass = ldevice->createRenderPassUnique( render_pass_info );

    vk::GraphicsPipelineCreateInfo graphics_pipeline_info;
    graphics_pipeline_info.stageCount = 2u;
    graphics_pipeline_info.pStages = pipeline_shader_stage_info;
    graphics_pipeline_info.pVertexInputState =
        &pipeline_vertex_imput_state_info;
    graphics_pipeline_info.pInputAssemblyState =
        &pipeline_input_assembly_state_info;
    graphics_pipeline_info.pViewportState = &pipeline_viewport_state_info;
    graphics_pipeline_info.pRasterizationState =
        &pipeline_rasterization_state_info;
    graphics_pipeline_info.pMultisampleState = &pipeline_multisample_state_info;
    graphics_pipeline_info.pDepthStencilState = nullptr;
    graphics_pipeline_info.pColorBlendState = &pipeline_color_blend_state_info;
    graphics_pipeline_info.layout = *pipeline_layout;
    graphics_pipeline_info.renderPass = *render_pass;
    graphics_pipeline_info.subpass = 0u;
    auto graphics_pipeline = ldevice->createGraphicsPipelineUnique(
        nullptr, graphics_pipeline_info );

    std::vector< vk::UniqueFramebuffer > framebuffers( image_views.size() );
    for( std::size_t i = 0u; i < image_views.size(); ++i )
    {
        auto attachment = *image_views[ i ];
        vk::FramebufferCreateInfo framebuffer_info;
        framebuffer_info.renderPass = *render_pass;
        framebuffer_info.attachmentCount = 1u;
        framebuffer_info.pAttachments = &attachment;
        framebuffer_info.width = surface_extent.width;
        framebuffer_info.height = surface_extent.height;
        framebuffer_info.layers = 1u;
        framebuffers[ i ] =
            ldevice->createFramebufferUnique( framebuffer_info );
    }

    vk::CommandPoolCreateInfo command_pool_info;
    command_pool_info.queueFamilyIndex =
        static_cast< std::uint32_t >( graphics_family_index );
    auto command_pool = ldevice->createCommandPoolUnique( command_pool_info );

    vk::CommandBufferAllocateInfo command_buffer_allocation_info;
    command_buffer_allocation_info.commandPool = *command_pool;
    command_buffer_allocation_info.level = vk::CommandBufferLevel::ePrimary;
    command_buffer_allocation_info.commandBufferCount =
        static_cast< std::uint32_t >( framebuffers.size() );
    auto command_buffers =
        ldevice->allocateCommandBuffersUnique( command_buffer_allocation_info );

    for( std::size_t i = 0; i < command_buffers.size(); ++i )
    {
        vk::CommandBufferBeginInfo command_buffer_begin_info;
        command_buffer_begin_info.flags =
            vk::CommandBufferUsageFlagBits::eSimultaneousUse;
        command_buffers[ i ]->begin( command_buffer_begin_info );

        vk::RenderPassBeginInfo render_pass_begin_info;
        render_pass_begin_info.renderPass = *render_pass;
        render_pass_begin_info.framebuffer = *framebuffers[ i ];
        render_pass_begin_info.renderArea.offset.x = 0;
        render_pass_begin_info.renderArea.offset.y = 0;
        render_pass_begin_info.renderArea.extent = surface_extent;
        vk::ClearValue clear_value( vk::ClearColorValue{
            std::array< float, 4 >{0.0f, 0.0f, 0.0f, 1.0f}} );
        render_pass_begin_info.clearValueCount = 1u;
        render_pass_begin_info.pClearValues = &clear_value;
        command_buffers[ i ]->beginRenderPass(
            render_pass_begin_info, vk::SubpassContents::eInline );

        command_buffers[ i ]->bindPipeline(
            vk::PipelineBindPoint::eGraphics, *graphics_pipeline );
        command_buffers[ i ]->draw( 3, 1, 0, 0 );
        command_buffers[ i ]->endRenderPass();
        command_buffers[ i ]->end();
    }

    vk::SemaphoreCreateInfo semaphore_info;
    auto semaphore_image_available =
        ldevice->createSemaphoreUnique( semaphore_info );
    auto semaphore_render_finished =
        ldevice->createSemaphoreUnique( semaphore_info );

    while( true )
    {
        if( glfwWindowShouldClose( window ) ) break;
        glfwPollEvents();

        auto image_index = ldevice->acquireNextImageKHR(
            *swapchain,
            std::numeric_limits< std::uint64_t >::max(),
            *semaphore_image_available,
            nullptr );

        vk::SubmitInfo submit_info;
        vk::Semaphore wait_semaphores[] = {*semaphore_image_available};
        vk::Semaphore signal_semaphores[] = {*semaphore_render_finished};
        vk::PipelineStageFlags pipeline_stage_flags[] = {
            vk::PipelineStageFlagBits::eColorAttachmentOutput};
        vk::CommandBuffer submit_command_buffers[] = {
            *command_buffers[ image_index.value ]};
        submit_info.waitSemaphoreCount = 1u;
        submit_info.pWaitSemaphores = wait_semaphores;
        submit_info.pWaitDstStageMask = pipeline_stage_flags;
        submit_info.commandBufferCount = 1u;
        submit_info.pCommandBuffers = submit_command_buffers;
        submit_info.signalSemaphoreCount = 1u;
        submit_info.pSignalSemaphores = signal_semaphores;
        graphics_queue.submit( submit_info, nullptr );

        vk::PresentInfoKHR present_info;
        present_info.waitSemaphoreCount = 1u;
        present_info.pWaitSemaphores = signal_semaphores;
        vk::SwapchainKHR swapchains[] = {*swapchain};
        present_info.swapchainCount = 1u;
        present_info.pSwapchains = swapchains;
        present_info.pImageIndices = &image_index.value;
        surface_queue.presentKHR( present_info );
    }
    ldevice->waitIdle();
    glfwDestroyWindow( window );
    glfwTerminate();
}
catch( std::exception &e )
{
    std::cerr << e.what() << std::endl;
}
catch( ... )
{
    std::cerr << "Error" << std::endl;
}
