// dear imgui: Platform Backend for PUGL
// This needs to be used along with a Renderer (e.g. SDL_GPU, DirectX11, OpenGL3, Vulkan..)

#pragma once
#include "imgui.h"  // IMGUI_IMPL_API
#ifndef IMGUI_DISABLE

#include <pugl/gl.h>
#include <pugl/pugl.h>

// Follow "Getting Started" link and check examples/ folder to learn about using backends!
IMGUI_IMPL_API bool ImGui_ImplPugl_InitForOpenGL(PuglWorld* world, PuglView* view);
IMGUI_IMPL_API void ImGui_ImplPugl_Shutdown();
IMGUI_IMPL_API void ImGui_ImplPugl_NewFrame();
IMGUI_IMPL_API bool ImGui_ImplPugl_ProcessEvent(const PuglEvent* event);

#endif  // #ifndef IMGUI_DISABLE
