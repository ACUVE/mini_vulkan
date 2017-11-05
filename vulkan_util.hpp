#pragma once

#include <array>
#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

namespace vulkan
{

    template < typename T, typename... Args >
    inline vk::VertexInputBindingDescription
    get_binding_description( std::uint32_t binding = 0u )
    {
        vk::VertexInputBindingDescription desc;
        desc.binding = binding;
        desc.stride = sizeof( T );
        desc.inputRate = vk::VertexInputRate::eVertex;
        return std::move( desc );
    }

    namespace detail
    {
        template < typename >
        struct get_vulkan_format;
        template <>
        struct get_vulkan_format< float >
        {
            static constexpr vk::Format format = vk::Format::eR32Sfloat;
        };
        template < glm::precision P >
        struct get_vulkan_format< glm::tvec1< float, P > >
        {
            static constexpr vk::Format format = vk::Format::eR32Sfloat;
        };
        template < glm::precision P >
        struct get_vulkan_format< glm::tvec2< float, P > >
        {
            static constexpr vk::Format format = vk::Format::eR32G32Sfloat;
        };
        template < glm::precision P >
        struct get_vulkan_format< glm::tvec3< float, P > >
        {
            static constexpr vk::Format format = vk::Format::eR32G32B32Sfloat;
        };
        template < glm::precision P >
        struct get_vulkan_format< glm::tvec4< float, P > >
        {
            static constexpr vk::Format format =
                vk::Format::eR32G32B32A32Sfloat;
        };
        template <>
        struct get_vulkan_format< double >
        {
            static constexpr vk::Format format = vk::Format::eR64Sfloat;
        };
        template < glm::precision P >
        struct get_vulkan_format< glm::tvec1< double, P > >
        {
            static constexpr vk::Format format = vk::Format::eR64Sfloat;
        };
        template < glm::precision P >
        struct get_vulkan_format< glm::tvec2< double, P > >
        {
            static constexpr vk::Format format = vk::Format::eR64G64Sfloat;
        };
        template < glm::precision P >
        struct get_vulkan_format< glm::tvec3< double, P > >
        {
            static constexpr vk::Format format = vk::Format::eR64G64B64Sfloat;
        };
        template < glm::precision P >
        struct get_vulkan_format< glm::tvec4< double, P > >
        {
            static constexpr vk::Format format =
                vk::Format::eR64G64B64A64Sfloat;
        };
        template <>
        struct get_vulkan_format< int >
        {
            static constexpr vk::Format format = vk::Format::eR32Sint;
        };
        template < glm::precision P >
        struct get_vulkan_format< glm::tvec1< int, P > >
        {
            static constexpr vk::Format format = vk::Format::eR32Sint;
        };
        template < glm::precision P >
        struct get_vulkan_format< glm::tvec2< int, P > >
        {
            static constexpr vk::Format format = vk::Format::eR32G32Sint;
        };
        template < glm::precision P >
        struct get_vulkan_format< glm::tvec3< int, P > >
        {
            static constexpr vk::Format format = vk::Format::eR32G32B32Sint;
        };
        template < glm::precision P >
        struct get_vulkan_format< glm::tvec4< int, P > >
        {
            static constexpr vk::Format format = vk::Format::eR32G32B32A32Sint;
        };
        template <>
        struct get_vulkan_format< glm::uint >
        {
            static constexpr vk::Format format = vk::Format::eR32Uint;
        };
        template < glm::precision P >
        struct get_vulkan_format< glm::tvec1< glm::uint, P > >
        {
            static constexpr vk::Format format = vk::Format::eR32Uint;
        };
        template < glm::precision P >
        struct get_vulkan_format< glm::tvec2< glm::uint, P > >
        {
            static constexpr vk::Format format = vk::Format::eR32G32Uint;
        };
        template < glm::precision P >
        struct get_vulkan_format< glm::tvec3< glm::uint, P > >
        {
            static constexpr vk::Format format = vk::Format::eR32G32B32Uint;
        };
        template < glm::precision P >
        struct get_vulkan_format< glm::tvec4< glm::uint, P > >
        {
            static constexpr vk::Format format = vk::Format::eR32G32B32A32Uint;
        };

        template < typename T, std::size_t NUM >
        inline void get_attribute_description_impl(
            std::array< vk::VertexInputAttributeDescription, NUM > &ret,
            std::uint32_t binding,
            std::size_t index )
        {
        }
        template <
            typename T,
            std::size_t NUM,
            typename Member,
            typename... Args >
        inline void get_attribute_description_impl(
            std::array< vk::VertexInputAttributeDescription, NUM > &ret,
            std::uint32_t binding,
            std::size_t index,
            Member member,
            Args... args )
        {
            using Type =
                std::decay_t< decltype( std::declval< T >().*member ) >;
            auto &ri = ret[ index ];
            ri.binding = binding;
            ri.location = index; // Ç±Ç±ê≥ÇµÇ≠Ç»Ç¢Ç∆évÇ§
            ri.format = get_vulkan_format< Type >::format;
            ri.offset = static_cast< std::uint32_t >(
                reinterpret_cast< std::uintptr_t >(
                    &( reinterpret_cast< T * >( NULL )->*member ) ) -
                reinterpret_cast< std::uintptr_t >(
                    reinterpret_cast< void * >( NULL ) ) );
            get_attribute_description_impl< T >(
                ret, binding, index + 1u, std::forward< Args >( args )... );
        }
    }

    template < typename T, typename... Args >
    inline std::array< vk::VertexInputAttributeDescription, sizeof...( Args ) >
    get_attribute_description( std::uint32_t binding, Args... args )
    {
        std::array< vk::VertexInputAttributeDescription, sizeof...( Args ) >
            ret;
        detail::get_attribute_description_impl< T >(
            ret, binding, 0u, std::forward< Args >( args )... );
        return std::move( ret );
    }

    template < typename T, typename... Args >
    inline auto
    get_vertex_input_description( std::uint32_t binding, Args... args )
    {
        return std::make_tuple(
            get_binding_description< T >( binding ),
            get_attribute_description< T >(
                binding, std::forward< Args >( args )... ) );
    }

} // namespace vulkan
