#pragma once

#include <etl/circular_buffer.h>
#include <etl/stack.h>
#include <etl/vector.h>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <optional>
#include "clap/events.h"
#include "clapeze/common.h"

namespace clapeze {

/*
 * Helper classes for building polyphonic synths that support clap's per note/per voice modulation options
 */

enum class VoiceStrategy {
    Poly,
    MonoLast,
};
template <typename TProcessor, typename TVoice, size_t TMaxVoices>
class VoicePool {
   public:
    explicit VoicePool(TProcessor& p, size_t numVoices = TMaxVoices) : mProcessor(p), mVoices() {
        SetNumVoices(numVoices);
    }

    void SetNumVoices(size_t numVoices) {
        if (numVoices != mVoices.size()) {
            assert(numVoices <= TMaxVoices);
            StopAllVoices();
            mPolyVoiceStrategy.Clear();
            mMonoLastVoiceStrategy.Clear();

            // deleting existing voices and reallocating
            mVoices.clear();
            for (size_t idx = 0; idx < numVoices; ++idx) {
                mVoices.emplace_back(mProcessor);
            }
        }
    }
    void SetStrategy(VoiceStrategy strategy) {
        if (strategy != mStrategy) {
            StopAllVoices();
            mPolyVoiceStrategy.Clear();
            mMonoLastVoiceStrategy.Clear();
            mStrategy = strategy;
        }
    }
    void ProcessNoteOn(const NoteTuple& note, float velocity) {
        switch (mStrategy) {
            case VoiceStrategy::Poly: {
                mPolyVoiceStrategy.ProcessNoteOn(*this, note, velocity);
                break;
            }
            case VoiceStrategy::MonoLast: {
                mMonoLastVoiceStrategy.ProcessNoteOn(*this, note, velocity);
                break;
            }
        }
    }
    void ProcessNoteOff(const NoteTuple& note) {
        switch (mStrategy) {
            case VoiceStrategy::Poly: {
                mPolyVoiceStrategy.ProcessNoteOff(*this, note);
                break;
            }
            case VoiceStrategy::MonoLast: {
                mMonoLastVoiceStrategy.ProcessNoteOff(*this, note);
                break;
            }
        }
    }
    void ProcessNoteChoke(const NoteTuple& note) {
        for (VoiceIndex idx = 0; idx < mVoices.size(); idx++) {
            auto& data = mVoices[idx];
            if (data.activeNote && data.activeNote->Match(note)) {
                StopVoice(idx);
            }
        }
    }
    void ProcessAudio(StereoAudioBuffer& out) {
        std::fill(out.left.begin(), out.left.end(), 0);
        std::fill(out.right.begin(), out.right.end(), 0);
        for (VoiceIndex idx = 0; idx < mVoices.size(); idx++) {
            auto& data = mVoices[idx];
            if (data.activeNote) {
                // TODO: this api forces voices to _add_ to the buffer, not replace it. this isn't super obvious from
                // the signature, so maybe we should wrap the StereoAudioBufferType? StereoSummingAudioBuffer or so
                bool playing = data.voice.ProcessAudio(out);
                if (!playing) {
                    SendNoteEnd(*data.activeNote);
                    data.activeNote = std::nullopt;
                }
            }
        }
    }

    void StopAllVoices() {
        for (VoiceIndex idx = 0; idx < mVoices.size(); idx++) {
            StopVoice(idx);
        }
    }

   private:
    using VoiceIndex = size_t;
    void StopVoice(VoiceIndex index) {
        auto& data = mVoices[index];
        if (data.activeNote) {
            SendNoteEnd(*data.activeNote);
            data.activeNote = std::nullopt;
            data.voice.ProcessChoke();
        }
    }

    void SendNoteEnd(const NoteTuple& note) {
        clap_event_note_t event = {};
        event.header.size = sizeof(event);
        event.header.time = 0;
        event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
        event.header.type = CLAP_EVENT_NOTE_END;
        event.header.flags = 0;
        event.note_id = note.id;
        event.channel = note.channel;
        event.key = note.key;
        event.port_index = note.port;
        mProcessor.SendEvent(event.header);
    }

    struct VoiceData {
        explicit VoiceData(TProcessor& p) : voice(p) {}
        TVoice voice;
        std::optional<NoteTuple> activeNote;
    };

    TProcessor& mProcessor;
    VoiceStrategy mStrategy;
    etl::vector<VoiceData, TMaxVoices> mVoices;

    struct PolyVoiceStrategy {
        void Clear() { mVoicesByLastUsed.clear(); }
        void ProcessNoteOn(VoicePool& pool, const NoteTuple& note, float velocity) {
            // retrigger existing voice, if there is one
            VoiceIndex voiceIndex = SIZE_MAX;
            for (VoiceIndex idx = 0; idx < pool.mVoices.size(); idx++) {
                auto& data = pool.mVoices[idx];
                if (data.activeNote && data.activeNote->Match(note)) {
                    voiceIndex = idx;
                    pool.SendNoteEnd(*(data.activeNote));
                    break;
                }
                if (data.activeNote == std::nullopt && voiceIndex == SIZE_MAX) {
                    voiceIndex = idx;
                }
            }
            // no voice available, time to steal
            if (voiceIndex == SIZE_MAX) {
                voiceIndex = mVoicesByLastUsed.front();
                mVoicesByLastUsed.pop();
                auto& data = pool.mVoices[voiceIndex];
                pool.SendNoteEnd(*(data.activeNote));
            }

            mVoicesByLastUsed.push(voiceIndex);
            pool.mVoices[voiceIndex].activeNote = note;
            pool.mVoices[voiceIndex].voice.ProcessNoteOn(note, velocity);
        }
        void ProcessNoteOff(VoicePool& pool, const NoteTuple& note) {
            auto& mVoices = pool.mVoices;
            for (VoiceIndex idx = 0; idx < mVoices.size(); idx++) {
                auto& data = mVoices[idx];
                if (data.activeNote && data.activeNote->Match(note)) {
                    data.voice.ProcessNoteOff();
                }
            }
        }
        etl::circular_buffer<VoiceIndex, TMaxVoices> mVoicesByLastUsed{};
    } mPolyVoiceStrategy;

    struct MonoLastVoiceStrategy {
        TVoice& Voice(VoicePool& pool) { return pool.mVoices[0].voice; }
        void Clear() { mActiveNotes.clear(); }

        void ProcessNoteOn(VoicePool& pool, const NoteTuple& note, float velocity) {
            if (!mActiveNotes.empty()) {
                // existing note playing, let's stash it
                Voice(pool).ProcessNoteOff();
            }

            if (mActiveNotes.full()) {
                // we're past the internal note tracking limit
                return;
            }
            mActiveNotes.push_back({note, velocity});
            pool.mVoices[0].activeNote = note;
            Voice(pool).ProcessNoteOn(note, velocity);
        }
        void ProcessNoteOff(VoicePool& pool, const NoteTuple& note) {
            // if the top note is playing, we'll have to stop it later
            bool isPlayingNote = !mActiveNotes.empty() && mActiveNotes.back().first.Match(note);

            mActiveNotes.erase(std::remove_if(mActiveNotes.begin(), mActiveNotes.end(),
                                              [&](const auto& activeNote) { return activeNote.first.Match(note); }),
                               mActiveNotes.end());

            if (isPlayingNote) {
                Voice(pool).ProcessNoteOff();

                // if any notes are left, then we should retrigger them
                if (!mActiveNotes.empty()) {
                    const auto& [lastNote, velocity] = mActiveNotes.back();
                    pool.mVoices[0].activeNote = lastNote;
                    Voice(pool).ProcessNoteOn(lastNote, velocity);
                }
            }
        }

        etl::vector<std::pair<NoteTuple, float>, TMaxVoices> mActiveNotes{};
    } mMonoLastVoiceStrategy;
};
}  // namespace clapeze