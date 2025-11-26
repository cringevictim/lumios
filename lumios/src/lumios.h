#pragma once

#include "defines.h"
#include "core/utils/logger.h"

#include <memory>
#include <functional>

// Forward declarations
namespace lumios::graphics {
    class GraphicsFrontend;
    class GraphicsConfig;
}

namespace lumios {

    // Engine configuration
    struct EngineConfig {
        bool enable_graphics = true;
        bool enable_audio = true;
        bool enable_physics = true;
        bool enable_networking = false;
        bool enable_scripting = false;
        utils::LogLevel log_level = utils::LogLevel::LUM_FATAL;
        bool enable_log_colors = true;
    };

    // Forward declarations for engine systems
    class Engine;
    class Application;
    class Scene;
    class GameObject;
    class Transform;
    class Renderer;
    class Camera;
    class Input;
    class Time;
    class Resources;
    class Audio;
    class Physics;

    // Application interface that users implement
    class LUMIOS_API Application {
    public:
        Application() = default;
        virtual ~Application() = default;

        // Core application lifecycle
        virtual bool Initialize() { return true; }
        virtual void Update(float deltaTime) {}
        virtual void Render() {}
        virtual void Shutdown() {}

        // Event handling
        virtual void OnWindowResize(int width, int height) {}
        virtual void OnKeyPressed(int key) {}
        virtual void OnKeyReleased(int key) {}
        virtual void OnMousePressed(int button) {}
        virtual void OnMouseReleased(int button) {}
        virtual void OnMouseMoved(float x, float y) {}

        // Scene management
        virtual void OnSceneLoad() {}
        virtual void OnSceneUnload() {}

    private:
        friend class Engine;
        Engine* m_Engine = nullptr;
    };

    // Main engine class
    class LUMIOS_API Engine {
    public:
        Engine();
        ~Engine();

        // Core engine lifecycle
        bool Initialize(const EngineConfig& config = EngineConfig{});
        void Run(Application* app);
        void Shutdown();

        // System access
        graphics::GraphicsFrontend* GetGraphics() const { return m_Graphics.get(); }
        Input* GetInput() const { return m_Input.get(); }
        Time* GetTime() const { return m_Time.get(); }
        Resources* GetResources() const { return m_Resources.get(); }
        Audio* GetAudio() const { return m_Audio.get(); }
        Physics* GetPhysics() const { return m_Physics.get(); }

        // State queries
        bool IsRunning() const { return m_IsRunning; }
        bool ShouldClose() const;
        void RequestShutdown() { m_ShouldShutdown = true; }

        // Configuration
        const EngineConfig& GetConfig() const { return m_Config; }

    private:
        EngineConfig m_Config;
        bool m_IsInitialized = false;
        bool m_IsRunning = false;
        bool m_ShouldShutdown = false;

        // Core systems
        std::unique_ptr<graphics::GraphicsFrontend> m_Graphics;
        std::unique_ptr<Input> m_Input;
        std::unique_ptr<Time> m_Time;
        std::unique_ptr<Resources> m_Resources;
        std::unique_ptr<Audio> m_Audio;
        std::unique_ptr<Physics> m_Physics;

        // Current application
        Application* m_CurrentApp = nullptr;

        // Internal methods
        bool InitializeSystems();
        void UpdateSystems(float deltaTime);
        void RenderSystems();
        void ShutdownSystems();
    };

    // Input system
    class LUMIOS_API Input {
    public:
        Input() = default;
        virtual ~Input() = default;

        // Keyboard
        virtual bool IsKeyPressed(int key) const { return false; }
        virtual bool IsKeyJustPressed(int key) const { return false; }
        virtual bool IsKeyJustReleased(int key) const { return false; }

        // Mouse
        virtual bool IsMouseButtonPressed(int button) const { return false; }
        virtual bool IsMouseButtonJustPressed(int button) const { return false; }
        virtual bool IsMouseButtonJustReleased(int button) const { return false; }
        virtual void GetMousePosition(float& x, float& y) const { x = y = 0.0f; }
        virtual void GetMouseDelta(float& dx, float& dy) const { dx = dy = 0.0f; }

        // Gamepad (placeholder)
        virtual bool IsGamepadConnected(int gamepad) const { return false; }
        virtual bool IsGamepadButtonPressed(int gamepad, int button) const { return false; }
        virtual float GetGamepadAxis(int gamepad, int axis) const { return 0.0f; }

        // Internal update
        virtual void Update() {}
    };

    // Time system
    class LUMIOS_API Time {
    public:
        Time() = default;
        virtual ~Time() = default;

        virtual float GetDeltaTime() const { return m_DeltaTime; }
        virtual float GetTotalTime() const { return m_TotalTime; }
        virtual uint64_t GetFrameCount() const { return m_FrameCount; }
        virtual float GetFPS() const { return m_FPS; }

        // Internal update
        virtual void Update(float deltaTime) {
            m_DeltaTime = deltaTime;
            m_TotalTime += deltaTime;
            m_FrameCount++;
            m_FPS = deltaTime > 0.0f ? 1.0f / deltaTime : 0.0f;
        }

    private:
        float m_DeltaTime = 0.0f;
        float m_TotalTime = 0.0f;
        uint64_t m_FrameCount = 0;
        float m_FPS = 0.0f;
    };

    // Resource management system
    class LUMIOS_API Resources {
    public:
        Resources() = default;
        virtual ~Resources() = default;

        // Texture loading (placeholder)
        virtual uint32_t LoadTexture(const char* path) { return 0; }
        virtual void UnloadTexture(uint32_t textureId) {}

        // Model loading (placeholder)
        virtual uint32_t LoadModel(const char* path) { return 0; }
        virtual void UnloadModel(uint32_t modelId) {}

        // Shader loading (placeholder)
        virtual uint32_t LoadShader(const char* vertexPath, const char* fragmentPath) { return 0; }
        virtual void UnloadShader(uint32_t shaderId) {}

        // Audio loading (placeholder)
        virtual uint32_t LoadSound(const char* path) { return 0; }
        virtual void UnloadSound(uint32_t soundId) {}
    };

    // Audio system (placeholder)
    class LUMIOS_API Audio {
    public:
        Audio() = default;
        virtual ~Audio() = default;

        virtual void PlaySound(uint32_t soundId) {}
        virtual void PlayMusic(uint32_t musicId) {}
        virtual void StopSound(uint32_t soundId) {}
        virtual void StopMusic() {}
        virtual void SetMasterVolume(float volume) {}
        virtual void SetSoundVolume(float volume) {}
        virtual void SetMusicVolume(float volume) {}
    };

    // Physics system (placeholder)
    class LUMIOS_API Physics {
    public:
        Physics() = default;
        virtual ~Physics() = default;

        virtual void Update(float deltaTime) {}
        virtual void SetGravity(float x, float y, float z) {}
        virtual uint32_t CreateRigidBody() { return 0; }
        virtual void DestroyRigidBody(uint32_t bodyId) {}
    };

    // Scene management (placeholder)
    class LUMIOS_API Scene {
    public:
        Scene() = default;
        virtual ~Scene() = default;

        virtual void Load() {}
        virtual void Unload() {}
        virtual void Update(float deltaTime) {}
        virtual void Render() {}
    };

    // GameObject system (placeholder)
    class LUMIOS_API GameObject {
    public:
        GameObject() = default;
        virtual ~GameObject() = default;

        virtual uint32_t GetId() const { return m_Id; }
        virtual Transform* GetTransform() { return nullptr; }
        virtual void Update(float deltaTime) {}
        virtual void Render() {}

    private:
        uint32_t m_Id = 0;
    };

    // Transform component (placeholder)
    class LUMIOS_API Transform {
    public:
        Transform() = default;
        virtual ~Transform() = default;

        virtual void SetPosition(float x, float y, float z) {}
        virtual void SetRotation(float x, float y, float z) {}
        virtual void SetScale(float x, float y, float z) {}
        virtual void GetPosition(float& x, float& y, float& z) const { x = y = z = 0.0f; }
        virtual void GetRotation(float& x, float& y, float& z) const { x = y = z = 0.0f; }
        virtual void GetScale(float& x, float& y, float& z) const { x = y = z = 1.0f; }
    };

    // Camera system (placeholder)
    class LUMIOS_API Camera {
    public:
        Camera() = default;
        virtual ~Camera() = default;

        virtual void SetPosition(float x, float y, float z) {}
        virtual void SetTarget(float x, float y, float z) {}
        virtual void SetFOV(float fov) {}
        virtual void SetAspectRatio(float ratio) {}
        virtual void SetNearPlane(float nearPlane) {}
        virtual void SetFarPlane(float farPlane) {}
    };

    // Renderer system (placeholder)
    class LUMIOS_API Renderer {
    public:
        Renderer() = default;
        virtual ~Renderer() = default;

        virtual void Clear(float r, float g, float b, float a) {}
        virtual void DrawTriangle(float x1, float y1, float x2, float y2, float x3, float y3) {}
        virtual void DrawQuad(float x, float y, float width, float height) {}
        virtual void DrawTexture(uint32_t textureId, float x, float y, float width, float height) {}
        virtual void DrawModel(uint32_t modelId) {}
    };

    // Global engine instance access
    LUMIOS_API Engine* GetEngine();
    LUMIOS_API void SetEngine(Engine* engine);

    // Convenience functions for common operations
    LUMIOS_API bool InitializeEngine(const EngineConfig& config = EngineConfig{});
    LUMIOS_API void RunApplication(Application* app);
    LUMIOS_API void ShutdownEngine();

    // Legacy functions for backward compatibility
    LUMIOS_API int32_t initialize();
    LUMIOS_API int32_t run();

} // namespace lumios