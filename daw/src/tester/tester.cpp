#include <clap/events.h>
#include <clap/process.h>
#include <clapeze/basePlugin.h>
#include <clapeze/entryPoint.h>
#include <clapeze/features/assetsFeature.h>
#include <clapeze/pluginHost.h>
#include <clapeze/processor/baseProcessor.h>
#include <etl/queue_spsc_atomic.h>
#include <etl/string.h>
#include <fmt/base.h>
#include <kitgui/context.h>

#include "descriptor.h"

#if KITSBLIPS_ENABLE_GUI
#include <imgui.h>
#include <kitgui/app.h>
#include "gui/kitguiFeature.h"
#include "gui/logger.h"
#endif

namespace tester {
using namespace clapeze;
ExampleAppLog sLog;

class Processor : public clapeze::BaseProcessor {
   public:
    explicit Processor(clapeze::PluginHost& host) : clapeze::BaseProcessor(host) {}
    ~Processor() = default;

    void Activate(double sampleRate, size_t minBlockSize, size_t maxBlockSize) override {
        sLog.AddLog("Processor::Activate(%f, %lu, %lu)\n", sampleRate, minBlockSize, maxBlockSize);
    }
    void Deactivate() override { sLog.AddLog("Processor::Deactivate()\n"); }
    void ProcessEvent(const clap_event_header_t& event) override {
        switch (event.type) {
            case CLAP_EVENT_NOTE_ON: {
                sLog.AddLog("CLAP_EVENT_NOTE_ON\n");
                break;
            }
            case CLAP_EVENT_NOTE_OFF: {
                sLog.AddLog("CLAP_EVENT_NOTE_OFF\n");
                break;
            }
            default: {
                break;
            }
        }
    }
    clapeze::ProcessStatus ProcessAudio(const clap_process_t& process, size_t blockStart, size_t blockStop) override {
        return clapeze::ProcessStatus::Sleep;
    }
    void ProcessFlush(const clap_process_t& process) override {}
    void ProcessReset() override { sLog.AddLog("Processor::ProcessReset()\n"); }
};

#if KITSBLIPS_ENABLE_GUI
class GuiApp : public kitgui::BaseApp {
   public:
    explicit GuiApp(kitgui::Context& ctx, clapeze::PluginHost& host) : kitgui::BaseApp(ctx), mHost(host) {}
    void OnUpdate() override {
        static bool showDebugLog{};
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("Debug")) {
                ImGui::MenuItem("AppLog", nullptr, &sLog.Show);
                ImGui::MenuItem("ImGui Debug Log", nullptr, &showDebugLog);
            }
        }
        if (ImGui::Button("Create CLAP_SUPPORT.csv")) {
            mHost.LogSupportMatrix();
        }
        sLog.Draw("AppLog");

        if (showDebugLog) {
            ImGui::ShowDebugLogWindow(&showDebugLog);
        }
    }

   private:
    clapeze::PluginHost& mHost;
};
#endif

class Plugin : public clapeze::BasePlugin {
   public:
    static const PluginEntry Entry;
    explicit Plugin(const clap_plugin_descriptor_t& meta) : clapeze::BasePlugin(meta) {}
    ~Plugin() = default;

   protected:
    void Config() override {
        sLog.Config(GetHost());
#if KITSBLIPS_ENABLE_GUI
        ConfigFeature<clapeze::AssetsFeature>();
        ConfigFeature<KitguiFeature>(
            GetHost(), [this](kitgui::Context& ctx) { return std::make_unique<GuiApp>(ctx, GetHost()); },
            kitgui::SizeConfig{1000, 600, false, true});
#endif

        ConfigProcessor<Processor>();
    }
};

CLAPEZE_REGISTER_PLUGIN(Plugin,
                        AudioEffectDescriptor("kitsblips.tester", "tester", "test plugin - used to diagnose host"));

}  // namespace tester
