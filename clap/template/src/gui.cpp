#include "gui.h"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_platform.h>

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3.h"

#include "PluginHost.h"


bool Gui::IsApiSupported(Gui::WindowingApi api, bool isFloating) {
	// All APIs supported, but floating windows NYI
	return api != Gui::WindowingApi::None && isFloating != true;
}
bool Gui::GetPreferredApi(Gui::WindowingApi& outApi, bool& outIsFloating) {
	std::string_view platformName = SDL_GetPlatform();
	if(platformName == "Windows") { outApi = Gui::WindowingApi::Win32; }
	else if(platformName == "macOS") { outApi = Gui::WindowingApi::Cocoa; }
	else if(platformName == "Linux") { outApi = Gui::WindowingApi::X11; }

	outIsFloating = false;
	return true;
}

bool Gui::OnAppInit() {
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        printf("Error: SDL_Init(): %s\n", SDL_GetError());
        return false;
    }

    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    return true;
}

bool Gui::OnAppQuit() {
    SDL_Quit();
    return true;
}

Gui::Gui(PluginHost* host, Gui::WindowingApi api, bool isFloating): mHost(host) {
	CreatePluginWindow();
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	// Setup Platform/Renderer backends
	ImGui_ImplSDL3_InitForOpenGL(mWindowHandle.get(), mCtx);
	ImGui_ImplOpenGL3_Init();

	// Setup main loop
	mHost->AddTimer(16, [this]() {
		// TODO: deltatime
		this->Update(1.0f/60.0f);
	});
}

Gui::~Gui() {
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();
	SDL_GL_DestroyContext(mCtx);
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
	CreateParentWindow(api, windowPointer);
	return SDL_SetWindowParent(mWindowHandle.get(), mParentHandle.get());
}
bool Gui::SetTransient(Gui::WindowingApi api, void* windowPointer){
	// Do nothing
	return true;
}
void Gui::SuggestTitle(const char* title){
	SDL_SetWindowTitle(mWindowHandle.get(), title);
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
	if(mWindowHandle) {
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();


		// app code
		OnGui();
		// end app code


		ImGui::Render();
		glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
		glClearColor(0, 0, 0, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		SDL_GL_SwapWindow(mWindowHandle.get());
	}
}

void Gui::CreatePluginWindow() {
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_WindowFlags window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
	SDL_Window* window = SDL_CreateWindow(kDefaultWindowTitle, kDefaultWindowWidth, kDefaultWindowHeight, window_flags);
	if (window == nullptr)
	{
		printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
		return;
	}
	SDL_GLContext gl_context = SDL_GL_CreateContext(window);
	if (gl_context == nullptr)
	{
		printf("Error: SDL_GL_CreateContext(): %s\n", SDL_GetError());
		return;
	}

	SDL_GL_MakeCurrent(window, gl_context);

	mWindowHandle = c_unique_ptr<SDL_Window>(window);
	mCtx = gl_context;
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


void Gui::OnGui() {
	ImGui::ShowDemoWindow();
}
