#pragma once

#include <SDL3/SDL_video.h>
#include <string>
#include <cstdint>
#include <memory>

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

class Gui {
	// constants
	public:
		enum class WindowingApi {
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

	// API methods
	public:
		static bool IsApiSupported(WindowingApi api, bool isFloating);
		static bool GetPreferredApi(WindowingApi& outApi, bool& outIsFloating);

		Gui(WindowingApi api, bool isFloating); // Create
		~Gui(); // Destroy

		bool SetScale(double scale);
		bool GetSize(uint32_t& outWidth, uint32_t& outHeight);
		bool CanResize();
		bool GetResizeHints(ResizeHints& outResizeHints);
		bool AdjustSize(uint32_t& inOutWidth, uint32_t& inOutHeight);
		bool SetSize(uint32_t width, uint32_t height);
		bool SetParent(WindowingApi api, void* windowPointer);
		bool SetTransient(WindowingApi api, void* windowPointer);
		void SuggestTitle(std::string_view title);
		bool Show();
		bool Hide();
		void Update(float dt);
		
		// implementation
	private:
		c_unique_ptr<SDL_Window> mWindowHandle;
		c_unique_ptr<SDL_Window> mParentHandle;

		void CreateWindow();
		void CreateParentWindow(WindowingApi api, void* windowPointer);
};
