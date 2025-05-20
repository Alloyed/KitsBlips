#pragma once

#include <SDL3/SDL_video.h>
#include <string>
#include <cstdint>
#include <memory>

// Forward declares
class PluginHost;

// from reddit
template<typename T>
struct c_deleter;
template<typename T>
using c_unique_ptr = std::unique_ptr<T, c_deleter<T>>;

template<>
struct c_deleter<SDL_Window> final {
	void operator()(SDL_Window* ptr) {
		SDL_DestroyWindow(ptr);
	}
};

/**
 * Gui is the main abstraction for windowing and graphics.
 * Right now we use SDL_Video to manage the window on all platforms, OpenGL for rendering, and imgui as the UI toolkit.
 * Future version may make this an abstract class that can have multiple different implementations for different toolkits.
 */
class Gui {
	// constants
	public:
		enum class WindowingApi {
			None = 0,
			X11,
			Wayland,
			Win32,
			Cocoa
		};
		struct ResizeHints {
			bool canResizeHorizontally;
			bool canResizeVertically;
			bool preserveAspectRatio;
			uint32_t aspectRatioWidth;
			uint32_t aspectRatioHeight;
		};
		static constexpr char kDefaultWindowTitle[] = "Template";
		static constexpr int32_t kDefaultWindowWidth = 400;
		static constexpr int32_t kDefaultWindowHeight = 400;

	// API methods
	public:
		static bool IsApiSupported(WindowingApi api, bool isFloating);
		static bool GetPreferredApi(WindowingApi& outApi, bool& outIsFloating);

		static bool OnAppInit();
		static bool OnAppQuit();

		Gui(PluginHost* host, WindowingApi api, bool isFloating); // Create
		~Gui(); // Destroy

		bool SetScale(double scale);
		bool GetSize(uint32_t& outWidth, uint32_t& outHeight);
		bool CanResize();
		bool GetResizeHints(ResizeHints& outResizeHints);
		bool AdjustSize(uint32_t& inOutWidth, uint32_t& inOutHeight);
		bool SetSize(uint32_t width, uint32_t height);
		bool SetParent(WindowingApi api, void* windowPointer);
		bool SetTransient(WindowingApi api, void* windowPointer);
		// _must_ be c-string
		void SuggestTitle(const char* title);
		bool Show();
		bool Hide();
		void Update(float dt);
		
		// implementation
	private:
		c_unique_ptr<SDL_Window> mWindowHandle;
		c_unique_ptr<SDL_Window> mParentHandle;
		PluginHost* mHost;
		SDL_GLContext mCtx;

		void CreateWindow();
		void CreateParentWindow(WindowingApi api, void* windowPointer);
		void OnGui();
};
