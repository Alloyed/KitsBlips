#pragma once

#include <etl/queue_spsc_atomic.h>
#include <fmt/format.h>
#include <kitdsp/sampling/samplePlayer.h>
#include <kitdsp/pitch/zeroCrossingPitchDetector.h>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <regex>
#include <vector>

#include "kitdsp/util/macros.h"
#include "kitdsp/math/interpolate.h"
#include "kitdsp/math/units.h"
#include "shared/dr_flac.h"
#include "shared/dr_wav.h"

#if KITSBLIPS_ENABLE_GUI
#include <imgui.h>
#include <imgui_internal.h>
#include <implot.h>
#include <kitgui/context.h>
#include <kitgui/wrap_nfd.h>
#include <misc/cpp/imgui_stdlib.h>
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

    // interesting limits
    // sourced from: https://www.copetti.org/writings/consoles/
    // snes:  64kb psram (audio-only, reduced by delay)
    // psx:  508kb dram (audio-only, reduced by reverb)
    // ps2:    2MB ram  (audio-only, reduced by reverb)
    // n64:    4MB rdram (unified mem)
    // nds:    4MB psram (unified mem)
    // gc:    16MB aram (built for audio but other systems can steal memory)
    // 3ds:  256kb cache, 128MB FCRAM (shared)

    std::ostringstream ss;

    if (unitIndex == 0) {
        ss << bytes << ' ' << units[unitIndex];
    } else {
        ss << std::fixed << std::setprecision(precision) << size << ' ' << units[unitIndex];
    }

    return ss.str();
}

inline std::string FormatMidiNote(int midiNote) {
    if (midiNote < 0 || midiNote > 127) {
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
    static inline const size_t kNumSamples = TNumSamples;
    /* File information send to the audio thread*/
    struct AudioFile {
        float* samples = nullptr;
        size_t numSamples = 0;
        float sampleRate = 0.0f;
        float baseFrequency = 261.626f;
        size_t loopStart = 0;
        size_t loopEnd = 0;
        kitdsp::SampleLoopDirection loopDirection = kitdsp::SampleLoopDirection::Forward;
        bool useLastSampleData = false;
    };
    /* Source file information used to prepare AudioFiles */
    struct SourceFile {
        std::string path;
        std::vector<std::vector<float>> rawSamples;
        std::vector<float> processedSamples;
        float rawSampleRate = 0.0f;
        float baseFrequency = 261.626f;

        size_t sampleStart = 0;
        size_t sampleEnd = 0;

        float loopStart = 0.0f;
        float loopEnd = 1.0f;
        kitdsp::SampleLoopDirection loopDirection = kitdsp::SampleLoopDirection::Forward;

        bool enableLofi = false;
        float lofiSampleRate = 0.0f;
        int32_t lofiBitDepth = 32;
        float lofiStretchSemis = 0.0f;

        bool useLastSampleData = false;

        AudioFile Clone() const {
            AudioFile cloned = CloneLite();
            if (this->processedSamples.size() > 0) {
                cloned.samples = new float[this->processedSamples.size()];
                memcpy(cloned.samples, this->processedSamples.data(), this->processedSamples.size() * sizeof(float));
            }
            cloned.numSamples = this->processedSamples.size();
            cloned.useLastSampleData = false;
            return cloned;
        }

        AudioFile CloneLite() const {
            AudioFile cloned{};
            if(enableLofi) {
                cloned.baseFrequency = this->baseFrequency * kitdsp::midiToRatio(this->lofiStretchSemis);
                cloned.sampleRate = this->lofiSampleRate;
            } else {
                cloned.baseFrequency = this->baseFrequency;
                cloned.sampleRate = this->rawSampleRate;
            }
            cloned.loopStart = narrow_cast<size_t>(this->loopStart * this->processedSamples.size());
            cloned.loopEnd = narrow_cast<size_t>(this->loopEnd * this->processedSamples.size());
            cloned.loopDirection = this->loopDirection;
            cloned.useLastSampleData = true;
            return cloned;
        }

        SourceFile CloneSource() const {
            SourceFile cloned = *this;
            return cloned;
        }

        void Preprocess() {
            if(rawSamples.empty()) 
            {
                processedSamples.resize(0);
                return;
            }
            float rateAdvance = kitdsp::midiToRatio(lofiStretchSemis) * rawSampleRate / lofiSampleRate;

            if(enableLofi) {
                size_t numRawSamples = sampleEnd - sampleStart;
                size_t numProcessedSamples = narrow_cast<size_t>(narrow_cast<float>(numRawSamples) / rateAdvance);

                // TODO: channel sum/pick right channel
                auto Read = [&](size_t idx) { return idx >= rawSamples[0].size() ? 0 : rawSamples[0][idx]; };
                float rawIdx = narrow_cast<float>(sampleStart);
                processedSamples.resize(numProcessedSamples);
                for (auto& out : processedSamples) {
                    size_t idx = narrow_cast<size_t>(rawIdx);
                    float idxFrac = rawIdx - std::floor(rawIdx);
                    out = kitdsp::interpolate::hermite4pt3oX(Read(idx - 1), Read(idx), Read(idx + 1), Read(idx + 2), idxFrac);
                    // bitcrush
                    float bitmax = narrow_cast<float>(1ul << (lofiBitDepth - 1));  // assuming signed format
                    out = std::floor(out * bitmax) / bitmax;
                    rawIdx += rateAdvance;
                }
            } else {
                processedSamples = rawSamples[0];
            }
            sAnySampleChanged = true;
        }
    };
    using FileQueue = etl::queue_spsc_atomic<std::pair<size_t, AudioFile*>, 200, etl::memory_model::MEMORY_MODEL_SMALL>;

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
            : mMainToAudioAcquire(mainToAudio),
              mAudioToMainRelease(audioToMain),
              mAudioToMainPlayback(playbackFileQueue) {
            mFiles.fill(nullptr);
        }
        bool OnAudioUpdate() {
            bool changed = false;
            std::pair<size_t, AudioFile*> pair;
            while (mMainToAudioAcquire.pop(pair)) {
                auto [index, file] = pair;
                if (mFiles[index]) {
                    if (file->useLastSampleData) {
                        // move buffer to new file
                        file->samples = mFiles[index]->samples;
                        file->numSamples = mFiles[index]->numSamples;
                        file->useLastSampleData = false;
                        mFiles[index]->samples = nullptr;
                        mFiles[index]->numSamples = 0;
                    }
                    // release
                    mAudioToMainRelease.push({index, mFiles[index]});
                }
                mFiles[index] = file;
                changed = true;
            }
            return changed;
        }
        const AudioFile* GetSampleData(size_t idx) const {
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
        etl::array<AudioFile*, TNumSamples> mFiles;
    };

    RawSampleLoader() : mAudioHandle(mMainToAudioAcquire, mAudioToMainRelease, mAudioToMainPlayback) {}

    void LoadSample(size_t idx, const std::string& path) {
        SourceFile& f = mMainFiles[idx];
        f.path = path;
        int midiNote = impl::TryParseMidiNoteFromFilename(path);

        if (path.ends_with(".flac")) {
            drflac* pFlac = drflac_open_file(path.c_str(), nullptr);
            if (pFlac == nullptr) {
                return;
            }

            size_t numSamples = pFlac->totalPCMFrameCount;
            f.rawSampleRate = narrow_cast<float>(pFlac->sampleRate);
            f.lofiSampleRate = f.rawSampleRate;
            f.lofiBitDepth = pFlac->bitsPerSample;
            f.sampleEnd = numSamples;
            {
                std::vector<float> raw(numSamples * pFlac->channels);
                size_t numSamplesRead = drflac_read_pcm_frames_f32(pFlac, numSamples, raw.data());
                if (numSamplesRead != numSamples) {
                    return;
                }
                // de-interleave inputs
                f.rawSamples.clear();
                for (size_t channelIdx = 0; channelIdx < pFlac->channels; ++channelIdx) {
                    f.rawSamples.emplace_back(numSamples);
                }
                size_t rawIdx = 0;
                for (size_t idx = 0; idx < numSamples; ++idx) {
                    for (size_t channelIdx = 0; channelIdx < pFlac->channels; ++channelIdx) {
                        f.rawSamples[channelIdx][idx] = raw[rawIdx++];
                    }
                }
            }

            drflac_close(pFlac);
        } else if (path.ends_with(".wav")) {
            drwav wav;
            if (!drwav_init_file_with_metadata(&wav, path.c_str(), 0, nullptr)) {
                return;
            }

            size_t numSamples = wav.totalPCMFrameCount;
            f.rawSampleRate = narrow_cast<float>(wav.sampleRate);
            f.lofiSampleRate = f.rawSampleRate;
            f.lofiBitDepth = wav.bitsPerSample;
            f.sampleEnd = numSamples;
            {
                std::vector<float> raw(numSamples * wav.channels);
                size_t numSamplesRead = drwav_read_pcm_frames_f32(&wav, wav.totalPCMFrameCount, raw.data());
                if (numSamplesRead != numSamples) {
                    drwav_uninit(&wav);
                    return;
                }
                // de-interleave inputs
                f.rawSamples.clear();
                for (size_t channelIdx = 0; channelIdx < wav.channels; ++channelIdx) {
                    f.rawSamples.emplace_back(numSamples);
                }
                size_t rawIdx = 0;
                for (size_t idx = 0; idx < numSamples; ++idx) {
                    for (size_t channelIdx = 0; channelIdx < wav.channels; ++channelIdx) {
                        f.rawSamples[channelIdx][idx] = raw[rawIdx++];
                    }
                }
            }

            for (drwav_uint32 mi = 0; mi < wav.metadataCount; ++mi) {
                auto& meta = wav.pMetadata[mi];
                if (meta.type == drwav_metadata_type_smpl) {
                    auto& smpl = meta.data.smpl;
                    midiNote = static_cast<int>(smpl.midiUnityNote);
                    if (smpl.sampleLoopCount > 0 && smpl.pLoops) {
                        f.loopStart = narrow_cast<float>(smpl.pLoops[0].firstSampleOffset) / narrow_cast<float>(f.rawSamples[0].size());
                        f.loopEnd = narrow_cast<float>(smpl.pLoops[0].lastSampleOffset) / narrow_cast<float>(f.rawSamples[0].size());
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
        } else {
            // guess base frequency
            kitdsp::pitch::ZeroCrossingPitchDetector detect(f.rawSampleRate);
            detect.Process({f.rawSamples[0].data(), f.rawSamples.size()});
            f.baseFrequency = detect.GetFrequency();
            if(f.baseFrequency == 0.0f) {
                // final fallback
                f.baseFrequency = kitdsp::midiToFrequency(60);
            }
        }

        f.Preprocess();
        mOriginalFiles[idx] = f.CloneSource();
        SendFullSampleToAudio(idx);
        mSamplePaths[idx] = path;
    }

    SourceFile& GetSourceFile(size_t idx) { return mMainFiles[idx]; }

    void SendSampleParamsToAudio(size_t idx) {
        SourceFile& sourceFile = mMainFiles[idx];
        AudioFile* audioFile = new AudioFile();
        *audioFile = sourceFile.CloneLite();
        audioFile->samples = nullptr;
        audioFile->numSamples = 0;
        audioFile->useLastSampleData = true;
        mMainToAudioAcquire.push({idx, audioFile});
    }

    void SendFullSampleToAudio(size_t idx) {
        SourceFile& sourceFile = mMainFiles[idx];
        AudioFile* audioFile = new AudioFile();
        *audioFile = sourceFile.Clone();
        mMainToAudioAcquire.push({idx, audioFile});
    }

    void OnMainUpdate() {
        std::pair<size_t, AudioFile*> pair;
        while (mAudioToMainRelease.pop(pair)) {
            auto* file = pair.second;
            if (file->samples) {
                delete[] file->samples;
            }
            delete file;
        }
    }
    AudioHandle& GetAudioHandle() { return mAudioHandle; }

    bool ImguiSampleSelect(const char* id, size_t* sampleIndex) {
        ImGui::BeginGroup();
        ImGui::PushID(id);
        bool changed = false;
        int numColumns = 4;
        auto region = ImGui::GetContentRegionAvail();
        float columnWidth =  region.x / numColumns;
        for (size_t idx = 0; idx < TNumSamples; ++idx) {
            ImGui::PushID(narrow_cast<int>(idx));
            if (idx > 0 && idx % numColumns != 0)
                ImGui::SameLine();
            std::string stem = std::filesystem::path(mSamplePaths[idx]).stem().string();
            if (stem.empty()) {
                stem = "<empty>";
            }
            if (ImGui::Selectable(stem.c_str(), *sampleIndex == idx, 0, ImVec2(columnWidth, 0.0f))) {
                *sampleIndex = idx;
                changed = true;
            }
            ImGui::PopID();
        }
        ImGui::PopID();
        ImGui::EndGroup();
        return changed;
    }

    void OnImGui(kitgui::Context& ctx) {
        size_t totalBytes = 0;
        for (size_t i = 0; i < TNumSamples; ++i) {
            totalBytes += mMainFiles[i].processedSamples.size() * mMainFiles[i].lofiBitDepth / 8;
        }
        ImGui::Text("Total sample memory: %s", impl::FormatBytes(totalBytes).c_str());

        ImguiSampleSelect("##currentSample", &mCurrentSampleIndex);

        size_t i = mCurrentSampleIndex;
        SourceFile& file = mMainFiles[i];
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
        int note = narrow_cast<int>(kitdsp::frequencyToMidi(file.baseFrequency));
        // TODO: print note name
        if (ImGui::SliderInt("Note", &note, 0, 127, impl::FormatMidiNote(note).c_str())) {
            file.baseFrequency = kitdsp::midiToFrequency(narrow_cast<float>(note));
            SendSampleParamsToAudio(i);
        }

        auto sliderIndex = [](const char* label, size_t* data, size_t min, size_t max) {
            static_assert(sizeof(size_t) == sizeof(uint64_t));
            return ImGui::SliderScalar(label, ImGuiDataType_U64, (void*)data, (void*)&min, (void*)&max);
        };

        if (sliderIndex("Sample Start", &file.sampleStart, 0, file.rawSamples.empty() ? 0 : file.rawSamples[0].size())) {
            file.Preprocess();
            SendFullSampleToAudio(i);
        }

        if (sliderIndex("Sample End", &file.sampleEnd, 0, file.rawSamples.empty() ? 0 : file.rawSamples[0].size())) {
            file.Preprocess();
            SendFullSampleToAudio(i);
        }


        const char* loopNames[] = {"No Loop", "Forward", "Backward", "Ping Pong"};
        int dir = static_cast<int>(file.loopDirection);
        if (ImGui::Combo("Loop Dir", &dir, loopNames, IM_ARRAYSIZE(loopNames))) {
            file.loopDirection = static_cast<kitdsp::SampleLoopDirection>(dir);
            SendSampleParamsToAudio(i);
        }

        if(file.loopDirection != kitdsp::SampleLoopDirection::NoLoop) {
            ImGui::Indent();
            if (ImGui::SliderFloat("Loop Start", &file.loopStart, 0, 1.0, std::to_string(narrow_cast<int32_t>(file.loopStart * file.processedSamples.size())).c_str())) {
                SendSampleParamsToAudio(i);
            }

            if (ImGui::SliderFloat("Loop End", &file.loopEnd, 0, 1.0, std::to_string(narrow_cast<int32_t>(file.loopEnd * file.processedSamples.size())).c_str())) {
                SendSampleParamsToAudio(i);
            }
            ImGui::Unindent();
        }

        if(ImGui::Checkbox("Lofi", &file.enableLofi)) {
            file.Preprocess();
            SendFullSampleToAudio(i);
        }

        if(file.enableLofi) {
            ImGui::Indent();
            if (ImGui::InputFloat("Lofi Sample Rate", &file.lofiSampleRate, 8000.0f)) {
                file.lofiSampleRate = kitdsp::clamp(std::floor(file.lofiSampleRate), 8000.0f, file.rawSampleRate);
                file.Preprocess();
                SendFullSampleToAudio(i);
            }

            if (ImGui::InputInt("Lofi Bit Depth", &file.lofiBitDepth, 8)) {
                file.lofiBitDepth = kitdsp::clamp(file.lofiBitDepth, 0, 32);
                file.Preprocess();
                SendFullSampleToAudio(i);
            }

            int32_t stretch = narrow_cast<int32_t>(file.lofiStretchSemis);
            if (ImGui::SliderInt("Lofi Stretch", &stretch, 0, 32)) {
                file.lofiStretchSemis = narrow_cast<float>(stretch);
                file.Preprocess();
                SendFullSampleToAudio(i);
            }
            ImGui::Unindent();
        }

        if (ImGui::Button("Reset")) {
            mMainFiles[i] = mOriginalFiles[i].CloneSource();
            SendFullSampleToAudio(i);
        }

        if(sAnySampleChanged) {
            ImPlot::SetNextAxesToFit();
            sAnySampleChanged = false;
        }
        if (ImPlot::BeginPlot("Waveform", ImVec2(-1.0f, 160.0f))) {
            if(!file.rawSamples.empty()) {
                ImPlot::SetupAxis(ImAxis_Y1, nullptr, ImPlotAxisFlags_NoTickLabels);
                ImPlot::SetupAxesLimits(0.0, narrow_cast<double>(file.processedSamples.size()), -1.0, 1.0);
                ImPlot::PlotShaded("##processedsamples", file.processedSamples.data(), static_cast<int>(file.processedSamples.size()));
            }
            // TODO: measured in processed sample rate not raw
            double markers[2] = {static_cast<double>(file.loopStart), static_cast<double>(file.loopEnd)};
            ImPlot::PlotInfLines("##loop", markers, 2);
            ImPlot::EndPlot();
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
    etl::array<SourceFile, TNumSamples> mMainFiles{};
    etl::array<SourceFile, TNumSamples> mOriginalFiles{};
    static inline bool sAnySampleChanged = false;
};
using SampleLoader = RawSampleLoader<8>;
}  // namespace layersynth