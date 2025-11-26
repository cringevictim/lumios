#pragma once

#include "../graphics_backend.h"
#include <memory>

// Forward declarations to avoid including DirectX headers
struct ID3D12Device;
struct ID3D12CommandQueue;
struct IDXGISwapChain3;
struct GLFWwindow;

namespace lumios::graphics {

    class LUMIOS_API DirectX12Backend : public GraphicsBackend {
    public:
        DirectX12Backend();
        ~DirectX12Backend() override;

        // GraphicsBackend interface
        BackendResult Initialize(const GraphicsConfig& config, GLFWwindow* window) override;
        void Shutdown() override;
        bool IsInitialized() const override { return m_IsInitialized; }

        BackendResult BeginFrame() override;
        BackendResult EndFrame() override;
        BackendResult Present() override;

        BackendResult HandleResize(uint32_t width, uint32_t height) override;
        BackendResult RecreateSwapchain() override;

        void WaitIdle() override;
        bool IsDeviceLost() const override { return m_DeviceLost; }

        std::string GetAPIName() const override { return "DirectX 12"; }
        std::string GetDeviceName() const override;
        std::string GetDriverVersion() const override;
        RenderStats GetRenderStats() const override;

        const GraphicsConfig& GetConfig() const override { return m_Config; }
        bool SupportsFeature(const std::string& feature) const override;

    private:
        // DirectX 12 objects (as void pointers to avoid including headers)
        void* m_Device = nullptr;              // ID3D12Device*
        void* m_CommandQueue = nullptr;        // ID3D12CommandQueue*
        void* m_SwapChain = nullptr;          // IDXGISwapChain3*
        void* m_CommandAllocator = nullptr;    // ID3D12CommandAllocator*
        void* m_CommandList = nullptr;         // ID3D12GraphicsCommandList*
        void* m_RenderTargetHeap = nullptr;    // ID3D12DescriptorHeap*
        void* m_RenderTargets[3] = { nullptr }; // ID3D12Resource*[3]
        void* m_Fence = nullptr;               // ID3D12Fence*

        // State
        GraphicsConfig m_Config;
        GLFWwindow* m_Window = nullptr;
        bool m_IsInitialized = false;
        bool m_DeviceLost = false;
        uint32_t m_CurrentBackBuffer = 0;
        uint64_t m_FenceValue = 0;
        void* m_FenceEvent = nullptr; // HANDLE

        // Stats
        mutable RenderStats m_Stats;

        // Initialization helpers (all return stub implementations)
        BackendResult CreateDevice();
        BackendResult CreateCommandQueue();
        BackendResult CreateSwapChain();
        BackendResult CreateRenderTargets();
        BackendResult CreateCommandObjects();
        BackendResult CreateSynchronizationObjects();

        // Helper functions
        void CleanupRenderTargets();
        void WaitForGPU();

        // Stub message for unimplemented functionality
        void LogNotImplemented(const std::string& function_name) const;
    };

} // namespace lumios::graphics
