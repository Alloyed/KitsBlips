#pragma once

#include <cmath>
#include <cstring>
#include <etl/queue_spsc_atomic.h>
#include <filesystem>
#include <fmt/format.h>
#include <iomanip>
#include <kitdsp/samplePlayer.h>
#include <regex>
#include <vector>

#include "shared/dr_flac.h"
#include "shared/dr_wav.h"

#if KITSBLIPS_ENABLE_GUI
#include <imgui.h>
#include <imgui_internal.h>
#include <kitgui/context.h>
#include <kitgui/wrap_nfd.h>
#include <misc/cpp/imgui_stdlib.h>
#include <implot.h>
#endif

namespace layersynth {

namespace impl {
// here be copy-paste: these should probably go into a utility file at some point
inline std::string FormatBytes(uint64_t bytes, int precision = 1) {
    static const char* units[] = {"B", "KB", "MB", "GB", "TB", "PB", "EB"};

    double size = static_cast<double>(bytes);
    size_t unitIndex = 0;

    while (size >= 1024.0 && unitIndex < std::size(units) - 1) {
        size /= 1024.0;
        ++unitIndex;
    }

    std::ostringstream ss;

    if (unitIndex == 0) {
        ss << bytes << ' ' << units[unitIndex];
    } else {
        ss << std::fixed << std::setprecision(precision) << size << ' ' << units[unitIndex];
    }

    return ss.str();
}

inline std::string FormatMidiNote(int midiNote) {
    if (midiNote < 0 || midiNote > 127)
    {
        return "";
    }

    static const char* names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

    int octave = midiNote / 12 - 1;
    int note = midiNote % 12;

    return std::string(names[note]) + std::to_string(octave);
}

inline int TryParseMidiNoteFromFilename(const std::string& filename) {
    namespace fs = std::filesystem;

    std::string stem = fs::path(filename).stem().string();

    // Match trailing note name, e.g. C4, C#4, Db3, A-1
    static const std::regex re(R"(([A-Ga-g])([#b]?)(-?\d+)$)");

    std::smatch match;
    if (!std::regex_search(stem, match, re)) {
        return -1;
    }

    std::string note;
    note += static_cast<char>(std::toupper(match[1].str()[0]));
    note += match[2].str();

    int octave = std::stoi(match[3].str());

    static const std::unordered_map<std::string, int> semitone = {
        {"C", 0},  {"C#", 1}, {"Db", 1}, {"D", 2},  {"D#", 3}, {"Eb", 3},  {"E", 4},   {"F", 5}, {"F#", 6},
        {"Gb", 6}, {"G", 7},  {"G#", 8}, {"Ab", 8}, {"A", 9},  {"A#", 10}, {"Bb", 10}, {"B", 11}};

    auto it = semitone.find(note);
    if (it == semitone.end()) {
        return -1;
    }

    return (octave + 1) * 12 + it->second;
}
}  // namespace impl

template <size_t TNumSamples>
class RawSampleLoader {
   public:
    struct File {
        std::string path;
        float* samples = nullptr;
        size_t numSamples = 0;
        float sampleRate = 0.0f;
        float baseFrequency = 261.626f;
        size_t loopStart = 0;
        size_t loopEnd = 0;
        kitdsp::SampleLoopDirection loopDirection = kitdsp::SampleLoopDirection::Forward;
        bool useLastSampleData = false;

        File Clone() const {
            File cloned = *this;
            if (this->samples && this->numSamples > 0) {
                cloned.samples = new float[this->numSamples];
                memcpy(cloned.samples, this->samples, this->numSamples * sizeof(float));
            }
            return cloned;
        }
    };
    using FileQueue = etl::queue_spsc_atomic<std::pair<size_t, File*>, 200, etl::memory_model::MEMORY_MODEL_SMALL>;

    struct SamplePlaybackUpdate {
        size_t fileIndex;
        uint32_t playbackId;
        bool isPlaying;
        float positionInSamples;
    };
    using PlaybackQueue = etl::queue_spsc_atomic<SamplePlaybackUpdate, 50, etl::memory_model::MEMORY_MODEL_SMALL>;

    class AudioHandle {
       public:
        AudioHandle(FileQueue& mainToAudio, FileQueue& audioToMain, PlaybackQueue& playbackFileQueue)
            : mMainToAudioAcquire(mainToAudio), mAudioToMainRelease(audioToMain), mAudioToMainPlayback(playbackFileQueue) {
            mFiles.fill(nullptr);
        }
        bool OnAudioUpdate() {
            bool changed = false;
            std::pair<size_t, File*> pair;
            while (mMainToAudioAcquire.pop(pair)) {
                auto [index, file] = pair;
                if (mFiles[index]) {
                    if(file->useLastSampleData) {
                        file->samples = mFiles[index]->samples;
                        file->numSamples = mFiles[index]->numSamples;
                        file->useLastSampleData = false;
                    }
                    // release
                    mAudioToMainRelease.push({index, mFiles[index]});
                }
                mFiles[index] = file;
                changed = true;
            }
            return changed;
        }
        const File* GetSampleData(size_t idx) const {
            if (idx < mFiles.size()) {
                return mFiles[idx];
            }
            return nullptr;
        }
        size_t GetNumSampleDatas() const { return mFiles.size(); }

       private:
        FileQueue& mMainToAudioAcquire;
        FileQueue& mAudioToMainRelease;
        PlaybackQueue& mAudioToMainPlayback;
        etl::array<File*, TNumSamples> mFiles;
    };

    RawSampleLoader() : mAudioHandle(mMainToAudioAcquire, mAudioToMainRelease, mAudioToMainPlayback) {}

    void LoadSample(size_t idx, const std::string& path) {
        File f;
        f.path = path;
        int midiNote = impl::TryParseMidiNoteFromFilename(path);

        if (path.ends_with(".flac")) {
            drflac* pFlac = drflac_open_file(path.c_str(), nullptr);
            if (pFlac == nullptr) {
                return;
            }

            size_t numSamples = pFlac->totalPCMFrameCount;
            f.sampleRate = narrow_cast<float>(pFlac->sampleRate);
            f.numSamples = numSamples;
            f.loopEnd = numSamples;
            f.samples = new float[numSamples];
            std::vector<float> raw(numSamples * pFlac->channels);
            size_t numSamplesRead = drflac_read_pcm_frames_f32(pFlac, numSamples, raw.data());
            if (numSamplesRead != numSamples) {
                return;
            }
            for (size_t idx = 0; idx < numSamples; ++idx) {
                f.samples[idx] = raw[idx * pFlac->channels];
            }

            drflac_close(pFlac);
        } else if (path.ends_with(".wav")) {
            drwav wav;
            if (!drwav_init_file_with_metadata(&wav, path.c_str(), 0, nullptr)) {
                return;
            }

            size_t numSamples = wav.totalPCMFrameCount;
            f.sampleRate = narrow_cast<float>(wav.sampleRate);
            f.numSamples = numSamples;
            f.loopEnd = numSamples;
            f.samples = new float[numSamples];
            {
                std::vector<float> raw(numSamples * wav.channels);
                size_t numSamplesRead = drwav_read_pcm_frames_f32(&wav, wav.totalPCMFrameCount, raw.data());
                if (numSamplesRead != numSamples) {
                    delete[] f.samples;
                    f.samples = nullptr;
                    drwav_uninit(&wav);
                    return;
                }
                for (size_t idx = 0; idx < numSamples; ++idx) {
                    f.samples[idx] = raw[idx * wav.channels];
                }
            }

            for (drwav_uint32 mi = 0; mi < wav.metadataCount; ++mi) {
                auto& meta = wav.pMetadata[mi];
                if (meta.type == drwav_metadata_type_smpl) {
                    auto& smpl = meta.data.smpl;
                    midiNote = static_cast<int>(smpl.midiUnityNote);
                    if (smpl.sampleLoopCount > 0 && smpl.pLoops) {
                        f.loopStart = smpl.pLoops[0].firstSampleOffset;
                        f.loopEnd = smpl.pLoops[0].lastSampleOffset;
                        switch (smpl.pLoops[0].type) {
                            case drwav_smpl_loop_type_forward: {
                                f.loopDirection = kitdsp::SampleLoopDirection::Forward;
                                break;
                            }
                            case drwav_smpl_loop_type_backward: {
                                f.loopDirection = kitdsp::SampleLoopDirection::Backward;
                                break;
                            }
                            case drwav_smpl_loop_type_pingpong: {
                                f.loopDirection = kitdsp::SampleLoopDirection::PingPong;
                                break;
                            }
                        }
                    }
                    break;
                }
            }

            drwav_uninit(&wav);
        } else {
            return;
        }

        if (midiNote >= 0) {
            f.baseFrequency = kitdsp::midiToFrequency(narrow_cast<float>(midiNote));
        }

        mOriginalFiles[idx] = f.Clone();
        mMainFiles[idx] = std::move(f);
        SendFullSampleToAudio(idx);
        mSamplePaths[idx] = path;
    }

    std::string GetSamplePath(size_t idx) { return mSamplePaths[idx]; }

    bool ImguiSampleSelect(const char* id, size_t* sampleIndex) {
        ImGui::BeginGroup();
        ImGui::PushID(id);
        bool changed = false;
        int numColumns = 4;
        for (size_t idx = 0; idx < TNumSamples; ++idx) {
            ImGui::PushID(narrow_cast<int>(idx));
            if (idx > 0 && idx % numColumns != 0) ImGui::SameLine();
            std::string stem = std::filesystem::path(mSamplePaths[idx]).stem().string();
            if(stem.empty()) { stem = "<empty>";}
            if (ImGui::Button(stem.c_str())) {
                *sampleIndex = idx;
                changed = true;
            }
            ImGui::PopID();
        }
        ImGui::PopID();
        ImGui::EndGroup();
        return changed;
    }

    void SendSampleParamsToAudio(size_t idx) {
        File& sourceFile = mMainFiles[idx];
        File* audioFile = new File();
        *audioFile = sourceFile;
        audioFile->samples = nullptr;
        audioFile->numSamples = 0;
        audioFile->useLastSampleData = true;
        mMainToAudioAcquire.push({idx, audioFile});
    }

    void SendFullSampleToAudio(size_t idx) {
        File& sourceFile = mMainFiles[idx];
        File* audioFile = new File();
        *audioFile = sourceFile.Clone();
        mMainToAudioAcquire.push({idx, audioFile});
    }

    void OnMainUpdate() {
        std::pair<size_t, File*> pair;
        while (mAudioToMainRelease.pop(pair)) {
            auto* file = pair.second;
            if(file->samples) {
                delete[] file->samples;
            }
            delete file;
        }
    }
    AudioHandle& GetAudioHandle() { return mAudioHandle; }

    void OnImGui(kitgui::Context& ctx) {
        size_t totalBytes = 0;
        for (size_t i = 0; i < TNumSamples; ++i) {
            if (mMainFiles[i].samples) {
                totalBytes += mMainFiles[i].numSamples * sizeof(float);
            }
        }
        ImGui::Text("Total sample memory: %s", impl::FormatBytes(totalBytes).c_str());

        ImguiSampleSelect("##currentSample", &mCurrentSampleIndex);

        size_t i = mCurrentSampleIndex;
        File& file = mMainFiles[i];
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
        if (file.samples) {
            int note = narrow_cast<int>(kitdsp::frequencyToMidi(file.baseFrequency));
            // TODO: print note name
            if (ImGui::SliderInt("Note", &note, 0, 127)) {
                file.baseFrequency = kitdsp::midiToFrequency(narrow_cast<float>(note));
                SendSampleParamsToAudio(i);
            }

            auto sliderIndex = [](const char* label, size_t* data, size_t min, size_t max) {
                static_assert(sizeof(size_t) == sizeof(uint64_t));
                return ImGui::SliderScalar(label, ImGuiDataType_U64, (void*)data, (void*)&min, (void*)&max);
            };

            if (sliderIndex("Loop Start", &file.loopStart, 0, file.numSamples)) {
                SendSampleParamsToAudio(i);
            }

            if (sliderIndex("Loop End", &file.loopEnd, 0, file.numSamples)) {
                SendSampleParamsToAudio(i);
            }

            const char* loopNames[] = {"No Loop", "Forward", "Backward", "Ping Pong"};
            int dir = static_cast<int>(file.loopDirection);
            if (ImGui::Combo("Loop Dir", &dir, loopNames, IM_ARRAYSIZE(loopNames))) {
                file.loopDirection = static_cast<kitdsp::SampleLoopDirection>(dir);
                SendSampleParamsToAudio(i);
            }

            if (ImGui::Button("Reset")) {
                mMainFiles[i] = mOriginalFiles[i].Clone();
                SendFullSampleToAudio(i);
            }

            if (ImPlot::BeginPlot("Waveform", ImVec2(-1.0f, 200.0f))) {
                ImPlot::PlotShaded("##waveform", file.samples, static_cast<int>(file.numSamples));
                double markers[2] = {static_cast<double>(file.loopStart),
                                     static_cast<double>(file.loopEnd)};
                ImPlot::PlotInfLines("##loop", markers, 2);
                ImPlot::EndPlot();
            }
        }
        ImGui::PopID();
    }

   private:
    FileQueue mMainToAudioAcquire{};
    FileQueue mAudioToMainRelease{};
    PlaybackQueue mAudioToMainPlayback{};
    AudioHandle mAudioHandle;
    etl::array<std::string, TNumSamples> mSamplePaths{};
    size_t mCurrentSampleIndex = 0;
    etl::array<std::string, TNumSamples> mSampleStatus{};
    etl::array<File, TNumSamples> mMainFiles{};
    etl::array<File, TNumSamples> mOriginalFiles{};
};
using SampleLoader = RawSampleLoader<4>;
}  // namespace layersynth