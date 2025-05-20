#include "gui.h"

#include <SDL3/SDL_log.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_opengl.h>

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3.h"


bool Gui::IsApiSupported(Gui::WindowingApi api, bool isFloating) {
}
bool Gui::GetPreferredApi(Gui::WindowingApi& outApi, bool& outIsFloating) {
}

Gui::Gui(Gui::WindowingApi api, bool isFloating) {
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	// Setup Platform/Renderer backends
	ImGui_ImplSDL3_InitForOpenGL(window, YOUR_SDL_GL_CONTEXT);
	ImGui_ImplOpenGL3_Init();
}
Gui::~Gui() {
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();
}

bool Gui::SetScale(double scale) {
	// unused. should be communicated to ui toolkit
	return true;
}
bool Gui::GetSize(uint32_t& outWidth, uint32_t& outHeight) {
	int32_t w;
	int32_t h;
	bool success = SDL_GetWindowSize(mWindowHandle.get(), &w, &h);
	outWidth = static_cast<uint32_t>(w);
	outHeight = static_cast<uint32_t>(h);
	return success;
}
bool Gui::CanResize() {
	return false;
}
bool Gui::GetResizeHints(ResizeHints& outResizeHints){
	outResizeHints.canResizeHorizontally = false;
	outResizeHints.canResizeVertically = false;
	outResizeHints.preserveAspectRatio = false;
	outResizeHints.aspectRatioWidth = false;
	outResizeHints.aspectRatioHeight = false;
	return true;
}
bool Gui::AdjustSize(uint32_t& inOutWidth, uint32_t& inOutHeight){
	// good enough :p
	return true;
}
bool Gui::SetSize(uint32_t width, uint32_t height){
	return SDL_SetWindowSize(mWindowHandle.get(), width, height);
}
bool Gui::SetParent(Gui::WindowingApi api, void* windowPointer){
	// wrap windowPointer
	return SDL_SetWindowParent(mWindowHandle.get(), mParentHandle.get());
}
bool Gui::SetTransient(Gui::WindowingApi api, void* windowPointer){
	// Do nothing
	return true;
}
void Gui::SuggestTitle(std::string_view title){
	SDL_SetWindowTitle(mWindowHandle.get(), title.data());
}
bool Gui::Show(){
	return SDL_ShowWindow(mWindowHandle.get());
}
bool Gui::Hide(){
	return SDL_HideWindow(mWindowHandle.get());
}

void Gui::Update(float dt){
	ImGuiIO& io = ImGui::GetIO();
	static SDL_Event event;
	while (SDL_PollEvent(&event)) {
		ImGui_ImplSDL3_ProcessEvent(&event); // Forward your event to backend
	}

	// (After event loop)
	// Start the Dear ImGui frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();


	// app code
	ImGui::ShowDemoWindow();
	// end app code


	ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0, 0, 0, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(mWindowHandle.get());
}

void Gui::CreateWindow() {
}

void Gui::CreateParentWindow(Gui::WindowingApi api, void* windowPtr)
{
	SDL_PropertiesID props = SDL_CreateProperties();
	if(props == 0) {
		return;
	}

	switch(api)
	{
		case WindowingApi::X11:
			{
				SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X11_WINDOW_NUMBER, reinterpret_cast<std::uintptr_t>(windowPtr));
				break;
			}
		case WindowingApi::Wayland:
			{
				SDL_SetPointerProperty(props, SDL_PROP_WINDOW_CREATE_WAYLAND_WL_SURFACE_POINTER, windowPtr);
				break;
			}
		case WindowingApi::Win32:
			{
				SDL_SetPointerProperty(props, SDL_PROP_WINDOW_CREATE_WIN32_HWND_POINTER, windowPtr);
				break;
			}
		case WindowingApi::Cocoa:
			{
				SDL_SetPointerProperty(props, SDL_PROP_WINDOW_CREATE_COCOA_WINDOW_POINTER, windowPtr);
				break;
			}
	}

	SDL_Window *window = SDL_CreateWindowWithProperties(props);
	if(window == nullptr) {
		SDL_Log("Unable to create parent window: %s", SDL_GetError());
		return;
	}

	///... is this safe? or do i need to keep it alive?
	SDL_DestroyProperties(props);
}
