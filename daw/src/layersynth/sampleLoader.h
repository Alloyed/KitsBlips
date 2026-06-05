#pragma once

#include "kitdsp/samplePlayer.h"
#include "shared/dr_flac.h"
#include "shared/dr_wav.h"

#if KITSBLIPS_ENABLE_GUI
#include <clapeze/features/assetsFeature.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <kitgui/app.h>
#include <kitgui/context.h>
#include <kitgui/gfx/scene.h>
#include <kitgui/wrap_nfd.h>
#include <misc/cpp/imgui_stdlib.h>
#include "gui/debugui.h"
#include "gui/kitguiFeature.h"
#include "gui/logger.h"
#include "gui/presetBrowser.h"
#endif

using namespace clapeze;
namespace layersynth {

template <size_t TNumSamples>
class RawSampleLoader {
   public:
    struct File {
        std::string path;
        std::vector<float> samples;
        float sampleRate;
        float baseFrequency;
        size_t loopStart;
        size_t loopEnd;
        SamplePlayer::LoopDirection loopDirection;
    };
    using Queue = etl::queue_spsc_atomic<std::pair<size_t, File*>, 200, etl::memory_model::MEMORY_MODEL_SMALL>;

    class AudioHandle {
       public:
        AudioHandle(Queue& mainToAudio, Queue& audioToMain) : mMainToAudio(mainToAudio), mAudioToMain(audioToMain) { mSampleData.fill(nullptr);}
        void ReleaseSample(size_t idx) {
            mAudioToMain.push({idx, mSampleData[idx]});
            mSampleData[idx] = nullptr;
        }
        bool OnAudioUpdate() {
            bool changed = false;
            std::pair<size_t, File*> pair;
            while (mMainToAudio.pop(pair)) {
                if (mSampleData[pair.first]) {
                    ReleaseSample(pair.first);
                }
                mSampleData[pair.first] = pair.second;
                changed = true;
            }
            return changed;
        }
        const File* GetSampleData(size_t idx) const {
            if(idx < mSampleData.size()) {
                return mSampleData[idx];
            }
            return nullptr;
        }
        size_t GetNumSampleDatas() const { return mSampleData.size(); }

       private:
        Queue& mMainToAudio;
        Queue& mAudioToMain;
        etl::array<File*, TNumSamples> mSampleData;
    };

    RawSampleLoader() : mAudioHandle(mMainToAudio, mAudioToMain) {}

    void LoadSample(size_t idx, const std::string& path) {
        File* f = new File();
        f->path = path;
        if (path.ends_with(".flac")) {
            drflac* pFlac = drflac_open_file(path.c_str(), nullptr);
            if (pFlac == nullptr) {
                return;
            }

            size_t numSamples = pFlac->totalPCMFrameCount;
            f->sampleRate = narrow_cast<float>(pFlac->sampleRate);
            f->samples.resize(numSamples);
            std::vector<float> raw(numSamples * pFlac->channels);
            size_t numSamplesRead = drflac_read_pcm_frames_f32(pFlac, numSamples, raw.data());
            if (numSamplesRead != numSamples) {
                return;
            }
            for (size_t idx = 0; idx < numSamples; ++idx) {
                // always take left channel for simplicity
                f->samples[idx] = raw[idx * pFlac->channels];
            }

            drflac_close(pFlac);
        } else if (path.ends_with(".wav")) {
            drwav wav;
            if (!drwav_init_file(&wav, path.c_str(), nullptr)) {
                return;
            }

            size_t numSamples = wav.totalPCMFrameCount;
            f->sampleRate = narrow_cast<float>(wav.sampleRate);
            f->samples.resize(numSamples);
            std::vector<float> raw(numSamples * wav.channels);
            size_t numSamplesRead = drwav_read_pcm_frames_f32(&wav, wav.totalPCMFrameCount, raw.data());
            if (numSamplesRead != numSamples) {
                return;
            }
            for (size_t idx = 0; idx < numSamples; ++idx) {
                // always take left channel for simplicity
                f->samples[idx] = raw[idx * wav.channels];
            }

            drwav_uninit(&wav);
        }

        if (f) {
            // TODO: pull from file, or let the user hand-customize
            f->baseFrequency = 261.626f;  // middle C
            f->loopDirection = SamplePlayer::LoopDirection::Forward;
            f->loopStart = 0;
            f->loopEnd = f->samples.size();

            mMainToAudio.push({idx, f});
            mSamplePaths[idx] = path;
        }
    }

    std::string GetSamplePath(size_t idx) { return mSamplePaths[idx]; }

    void OnMainUpdate() {
        std::pair<size_t, File*> pair;
        while (mAudioToMain.pop(pair)) {
            delete pair.second;
        }
    }
    AudioHandle& GetAudioHandle() { return mAudioHandle; }

    void OnImGui(kitgui::Context& ctx) {
        for (size_t i = 0; i < TNumSamples; ++i) {
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::InputText(fmt::format("Sample {}", i).c_str(), &mSamplePaths[i],
                                 ImGuiInputTextFlags_EnterReturnsTrue)) {
                LoadSample(i, mSamplePaths[i]);
            }
            ImGui::SameLine();
            if (ImGui::Button("Load")) {
                nfdu8char_t* outPath{};
                nfdu8filteritem_t filters[2] = {{"Audio", "wav"}};
                nfdopendialogu8args_t args{};
                args.filterList = filters;
                args.filterCount = 1;
                args.parentWindow = NFD_GetWindow(ctx.GetWindow());
                nfdresult_t result = NFD_OpenDialogU8_With(&outPath, &args);
                if (result == NFD_OKAY) {
                    LoadSample(i, outPath);
                    NFD_FreePathU8(outPath);
                } else if (result == NFD_CANCEL) {
                    mSampleStatus[i] = "<canceled>";
                } else {
                    mSampleStatus[i] = NFD_GetError();
                }
            }
            // ImGui::Text();
            ImGui::PopID();
        }
    }

   private:
    Queue mMainToAudio{};
    Queue mAudioToMain{};
    AudioHandle mAudioHandle;
    etl::array<std::string, TNumSamples> mSamplePaths{};
    etl::array<std::string, TNumSamples> mSampleStatus{};
};
using SampleLoader = RawSampleLoader<4>;
}