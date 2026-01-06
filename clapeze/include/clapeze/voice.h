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

template <typename TProcessor, typename TVoice, size_t TMaxVoices>
class PolyphonicVoicePool {
   public:
    explicit PolyphonicVoicePool(TProcessor& p, size_t numVoices = TMaxVoices) : mProcessor(p), mVoices() {
        SetNumVoices(numVoices);
    }

    void SetNumVoices(size_t numVoices) {
        if (numVoices != mVoices.size()) {
            assert(numVoices <= TMaxVoices);
            StopAllVoices();
            mVoices.clear();
            for (size_t idx = 0; idx < numVoices; ++idx) {
                mVoices.emplace_back(mProcessor);
            }
            mVoicesByLastUsed.clear();
        }
    }
    void ProcessNoteOn(const NoteTuple& note, float velocity) {
        // retrigger existing voice, if there is one
        VoiceIndex voiceIndex = SIZE_MAX;
        for (VoiceIndex idx = 0; idx < mVoices.size(); idx++) {
            auto& data = mVoices[idx];
            if (data.activeNote && data.activeNote->Match(note)) {
                voiceIndex = idx;
                SendNoteEnd(*(data.activeNote));
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
            auto& data = mVoices[voiceIndex];
            SendNoteEnd(*(data.activeNote));
        }

        mVoicesByLastUsed.push(voiceIndex);
        mVoices[voiceIndex].activeNote = note;
        mVoices[voiceIndex].voice.ProcessNoteOn(note, velocity);
    }
    void ProcessNoteOff(const NoteTuple& note) {
        for (VoiceIndex idx = 0; idx < mVoices.size(); idx++) {
            auto& data = mVoices[idx];
            if (data.activeNote && data.activeNote->Match(note)) {
                data.voice.ProcessNoteOff();
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
    etl::vector<VoiceData, TMaxVoices> mVoices;
    etl::circular_buffer<VoiceIndex, TMaxVoices> mVoicesByLastUsed{};
};

template <typename TProcessor, typename VoiceType>
class MonophonicVoicePool {
   public:
    explicit MonophonicVoicePool(TProcessor& p) : mProcessor(p), mVoice(p) {}

    void ProcessNoteOn(const NoteTuple& note, float velocity) {
        if (!mActiveNotes.empty()) {
            // existing note playing, let's stash it
            mVoice.ProcessNoteOff();
        }

        if (mActiveNotes.full()) {
            // we're past the internal note tracking limit
            return;
        }
        mActiveNotes.push_back({note, velocity});
        mVoice.ProcessNoteOn(note, velocity);
        mPlaying = true;
    }
    void ProcessNoteOff(const NoteTuple& note) {
        // if the top note is playing, we'll have to stop it later
        bool isPlayingNote = !mActiveNotes.empty() && mActiveNotes.back().first.Match(note);

        mActiveNotes.erase(std::remove_if(mActiveNotes.begin(), mActiveNotes.end(),
                                          [&](const auto& activeNote) { return activeNote.first.Match(note); }),
                           mActiveNotes.end());

        if (isPlayingNote) {
            mVoice.ProcessNoteOff();

            // if any notes are left, then we should retrigger them
            if (!mActiveNotes.empty()) {
                const auto& [lastNote, velocity] = mActiveNotes.back();
                mVoice.ProcessNoteOn(lastNote, velocity);
            }
        }
    }
    void ProcessNoteChoke(const NoteTuple& note) { StopAllVoices(); }
    void ProcessAudio(StereoAudioBuffer& out) {
        std::fill(out.left.begin(), out.left.end(), 0);
        std::fill(out.right.begin(), out.right.end(), 0);
        if (mPlaying && !mVoice.ProcessAudio(out)) {
            if (!mActiveNotes.empty()) {
                SendNoteEnd(mActiveNotes.back().first);
                mActiveNotes.pop_back();
            }
            mPlaying = !mActiveNotes.empty();
        }
    }

    void StopAllVoices() { StopVoice(); }

   private:
    void StopVoice() {
        if (mPlaying) {
            while (!mActiveNotes.empty()) {
                SendNoteEnd(mActiveNotes.back().first);
                mActiveNotes.pop_back();
            }
            mVoice.ProcessChoke();
        }
        mPlaying = false;
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

    TProcessor& mProcessor;
    VoiceType mVoice;
    etl::vector<std::pair<NoteTuple, float>, 16> mActiveNotes{};
    bool mPlaying = false;
};
}  // namespace clapeze