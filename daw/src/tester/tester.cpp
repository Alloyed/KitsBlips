#include <clap/events.h>
#include <clap/process.h>
#include <clapeze/basePlugin.h>
#include <clapeze/entryPoint.h>
#include <clapeze/processor/baseProcessor.h>
#include <etl/queue_spsc_atomic.h>
#include <etl/string.h>

#include <clapeze/pluginHost.h>
#include "descriptor.h"
#include "fmt/base.h"
#include "kitgui/context.h"
#include "tester/appLog.h"

#if KITSBLIPS_ENABLE_GUI
#include <imgui.h>
#include <kitgui/app.h>
#include "gui/kitguiFeature.h"
#endif

namespace tester {
using namespace clapeze;

using FixedMessage = std::pair<LogSeverity, etl::string<100>>;
using MessageQueue = etl::queue_spsc_atomic<FixedMessage, 100, etl::memory_model::MEMORY_MODEL_SMALL>;

class Processor : public clapeze::BaseProcessor {
   public:
    explicit Processor(MessageQueue& audioToMain) : mAudioToMain(audioToMain) {}
    ~Processor() = default;

    void Activate(double sampleRate, size_t minBlockSize, size_t maxBlockSize) override {
        LogFmt(LogSeverity::Info, "Processor::Activate({}, {}, {})", sampleRate, minBlockSize, maxBlockSize);
    }
    void Deactivate() override { LogFmt(LogSeverity::Info, "Processor::Deactivate()"); }
    void ProcessEvent(const clap_event_header_t& event) override {
        switch (event.type) {
            case CLAP_EVENT_NOTE_ON: {
                LogFmt(LogSeverity::Debug, "NOTE_ON()");
                break;
            }
            case CLAP_EVENT_NOTE_OFF: {
                LogFmt(LogSeverity::Debug, "NOTE_OFF()");
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
    void ProcessReset() override { LogFmt(LogSeverity::Info, "Processor::ProcessReset()"); }

    template <typename... Args>
    void LogFmt(LogSeverity severity, fmt::format_string<Args...> fmt, Args&&... args) const {
        static etl::string<100> buf;
        auto result = fmt::format_to_n(buf.data(), buf.max_size() - 1, fmt, std::forward<Args>(args)...);
        *result.out = '\0';
        buf.resize(result.size + 1);
        mAudioToMain.emplace(severity, buf);
    }

   private:
    MessageQueue& mAudioToMain;
};

namespace {
ExampleAppLog sLog;
MessageQueue sLogFromAudio;
}  // namespace

#if KITSBLIPS_ENABLE_GUI
class GuiApp : public kitgui::BaseApp {
   public:
    explicit GuiApp(kitgui::Context& ctx, clapeze::PluginHost& host) : kitgui::BaseApp(ctx), mHost(host) {}
    void OnUpdate() override {
        FixedMessage line;
        while (sLogFromAudio.pop(line)) {
            mHost.Log(line.first, line.second.c_str());
        }

        static bool showDebugLog{};
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("Tools")) {
                ImGui::MenuItem("ImGui Debug Log", nullptr, &showDebugLog);
            }
        }
        if (ImGui::Button("Create CLAP_SUPPORT.csv")) {
            mHost.LogSupportMatrix();
        }
        sLog.Draw();

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
        GetHost().SetLogFn([](auto severity, const std::string& line) { sLog.AddLog(line); });
#if KITSBLIPS_ENABLE_GUI
        ConfigFeature<KitguiFeature>(
            GetHost(), [this](kitgui::Context& ctx) { return std::make_unique<GuiApp>(ctx, GetHost()); },
            kitgui::SizeConfig{1000, 600, false, true});
#endif

        ConfigProcessor<Processor>(sLogFromAudio);
    }
};

CLAPEZE_REGISTER_PLUGIN(Plugin,
                        AudioEffectDescriptor("kitsblips.tester", "tester", "test plugin - used to diagnose host"));

}  // namespace tester
