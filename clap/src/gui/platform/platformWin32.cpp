#ifdef _WIN32
namespace platformGui {
    bool create()
    {
        if (globalOpenGUICount++ == 0) {
            WNDCLASS windowClass = {};
            windowClass.lpfnWndProc = GUIWindowProcedure;
            windowClass.cbWndExtra = sizeof(MyPlugin *);
            windowClass.lpszClassName = pluginDescriptor.id;
            windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
            windowClass.style = CS_DBLCLKS;
            RegisterClass(&windowClass);
        }

        plugin->gui->window = CreateWindow(pluginDescriptor.id, pluginDescriptor.name, WS_CHILDWINDOW | WS_CLIPSIBLINGS, 
                CW_USEDEFAULT, 0, GUI_WIDTH, GUI_HEIGHT, GetDesktopWindow(), NULL, NULL, NULL);
        plugin->gui->bits = (uint32_t *) calloc(1, GUI_WIDTH * GUI_HEIGHT * 4);
        SetWindowLongPtr(plugin->gui->window, 0, (LONG_PTR) plugin);
    }
    
    void destroy() {
	assert(plugin->gui);
	DestroyWindow(plugin->gui->window);
	free(plugin->gui->bits);
	free(plugin->gui);
	plugin->gui = nullptr;

	if (--globalOpenGUICount == 0) {
		UnregisterClass(pluginDescriptor.id, NULL);
	}
    }

    #define GUISetParent(plugin, parent) SetParent((plugin)->gui->window, (HWND) (parent)->win32)
}
#endif