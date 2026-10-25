#include "VulkanLoader.h"

#include "DynamicLibrary.h"
#include "Logger.h"
#include "Platform.h"

#include <stdexcept>
#include <string>
#include <string_view>

extern "C"
{
#define FLING_VK_GLOBAL(name) PFN_##name name = nullptr;
#define FLING_VK_GLOBAL_OPTIONAL(name) PFN_##name name = nullptr;
#define FLING_VK_INSTANCE(name) PFN_##name name = nullptr;
#define FLING_VK_INSTANCE_OPTIONAL(name) PFN_##name name = nullptr;
#define FLING_VK_DEVICE(name) PFN_##name name = nullptr;
#define FLING_VK_DEVICE_OPTIONAL(name) PFN_##name name = nullptr;
#include "VulkanFunctions.inl"
#undef FLING_VK_GLOBAL
#undef FLING_VK_GLOBAL_OPTIONAL
#undef FLING_VK_INSTANCE
#undef FLING_VK_INSTANCE_OPTIONAL
#undef FLING_VK_DEVICE
#undef FLING_VK_DEVICE_OPTIONAL
}

namespace
{
    Fling::DynamicLibrary g_Loader;

    [[noreturn]] void FatalVulkan(const std::string& t_Message)
    {
        if (const auto Console = Fling::Logger::GetCurrentConsole())
        {
            Console->error(t_Message);
            Console->flush();
        }
        if (const auto File = Fling::Logger::GetCurrentLogFile())
        {
            File->error(t_Message);
        }
        throw std::runtime_error(t_Message);
    }

    template <typename T_Fn>
    void LoadGlobal(const char* t_Name, T_Fn& t_Out)
    {
        // Vulkan 1.0 and 1.1 do not return this pointer from itself.
        // DynamicLibrary::Symbol already stored the export.
        if (std::string_view(t_Name) == "vkGetInstanceProcAddr")
        {
            return;
        }
        t_Out = reinterpret_cast<T_Fn>(vkGetInstanceProcAddr(nullptr, t_Name));
    }

    template <typename T_Fn>
    void LoadInstanceProc(VkInstance t_Instance, const char* t_Name, T_Fn& t_Out)
    {
        t_Out = reinterpret_cast<T_Fn>(vkGetInstanceProcAddr(t_Instance, t_Name));
    }

    template <typename T_Fn>
    void LoadDeviceProc(VkDevice t_Device, const char* t_Name, T_Fn& t_Out)
    {
        t_Out = reinterpret_cast<T_Fn>(vkGetDeviceProcAddr(t_Device, t_Name));
    }

    template <typename T_Fn>
    void RequireProc(const char* t_Name, const T_Fn& t_Fn)
    {
        if (t_Fn == nullptr)
        {
            FatalVulkan(std::string("Required Vulkan function is null: ") + t_Name);
        }
    }

    template <typename T_Fn>
    void ClearProc(T_Fn& t_Fn)
    {
        t_Fn = nullptr;
    }
} // namespace

namespace Fling
{
    bool VulkanLoader::LoadLoader(std::string* t_Error)
    {
#if FLING_WINDOWS
        DynamicLibrary Library = DynamicLibrary::Open("vulkan-1.dll", t_Error);
#elif FLING_LINUX
        const std::string_view Names[] = {"libvulkan.so.1", "libvulkan.so"};
        DynamicLibrary Library = DynamicLibrary::OpenFirst(Names, t_Error);
#else
#error VulkanLoader supports Windows and Linux only
#endif
        if (!Library)
        {
            return false;
        }

        const PFN_vkGetInstanceProcAddr GetInstanceProcAddr =
            Library.SymbolAs<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
        if (GetInstanceProcAddr == nullptr)
        {
            if (t_Error != nullptr)
            {
                *t_Error = "vkGetInstanceProcAddr is missing from the Vulkan loader";
            }
            Library.Close();
            return false;
        }

        g_Loader = std::move(Library);
        vkGetInstanceProcAddr = GetInstanceProcAddr;

#define FLING_VK_GLOBAL(name) LoadGlobal(#name, name);
#define FLING_VK_GLOBAL_OPTIONAL(name) LoadGlobal(#name, name);
#include "VulkanFunctions.inl"
#undef FLING_VK_GLOBAL
#undef FLING_VK_GLOBAL_OPTIONAL

#define FLING_VK_GLOBAL(name) RequireProc(#name, name);
#include "VulkanFunctions.inl"
#undef FLING_VK_GLOBAL

        return true;
    }

    void VulkanLoader::LoadInstance(VkInstance t_Instance)
    {
#define FLING_VK_INSTANCE(name) LoadInstanceProc(t_Instance, #name, name);
#define FLING_VK_INSTANCE_OPTIONAL(name) LoadInstanceProc(t_Instance, #name, name);
#include "VulkanFunctions.inl"
#undef FLING_VK_INSTANCE
#undef FLING_VK_INSTANCE_OPTIONAL

#define FLING_VK_INSTANCE(name) RequireProc(#name, name);
#include "VulkanFunctions.inl"
#undef FLING_VK_INSTANCE
    }

    void VulkanLoader::LoadDevice(VkDevice t_Device)
    {
        if (vkGetDeviceProcAddr == nullptr)
        {
            FatalVulkan("Required Vulkan function is null: vkGetDeviceProcAddr");
        }

#define FLING_VK_DEVICE(name) LoadDeviceProc(t_Device, #name, name);
#define FLING_VK_DEVICE_OPTIONAL(name) LoadDeviceProc(t_Device, #name, name);
#include "VulkanFunctions.inl"
#undef FLING_VK_DEVICE
#undef FLING_VK_DEVICE_OPTIONAL

#define FLING_VK_DEVICE(name) RequireProc(#name, name);
#include "VulkanFunctions.inl"
#undef FLING_VK_DEVICE
    }

    void VulkanLoader::UnloadLoader()
    {
#define FLING_VK_GLOBAL(name) ClearProc(name);
#define FLING_VK_GLOBAL_OPTIONAL(name) ClearProc(name);
#define FLING_VK_INSTANCE(name) ClearProc(name);
#define FLING_VK_INSTANCE_OPTIONAL(name) ClearProc(name);
#define FLING_VK_DEVICE(name) ClearProc(name);
#define FLING_VK_DEVICE_OPTIONAL(name) ClearProc(name);
#include "VulkanFunctions.inl"
#undef FLING_VK_GLOBAL
#undef FLING_VK_GLOBAL_OPTIONAL
#undef FLING_VK_INSTANCE
#undef FLING_VK_INSTANCE_OPTIONAL
#undef FLING_VK_DEVICE
#undef FLING_VK_DEVICE_OPTIONAL

        g_Loader.Close();
    }
} // namespace Fling
