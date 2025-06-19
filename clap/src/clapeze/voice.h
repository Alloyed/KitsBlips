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

/*
 * Helper classes for building polyphonic synths that support clap's per note/per voice modulation options
 */

template <typename ProcessorType, typename VoiceType, size_t MaxVoices>
class PolyphonicVoicePool {
   public:
    PolyphonicVoicePool(ProcessorType& p, size_t numVoices = MaxVoices) : mProcessor(p), mVoices() {
        SetNumVoices(numVoices);
    }

    void SetNumVoices(size_t numVoices) {
        if (numVoices != mVoices.size()) {
            assert(numVoices <= MaxVoices);
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
            if (data.activeNote && !data.voice.ProcessAudio(out)) {
                SendNoteEnd(*data.activeNote);
                data.activeNote = std::nullopt;
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
        VoiceData(ProcessorType& p) : voice(p) {}
        VoiceType voice;
        std::optional<NoteTuple> activeNote;
    };

    ProcessorType& mProcessor;
    etl::vector<VoiceData, MaxVoices> mVoices;
    etl::circular_buffer<VoiceIndex, MaxVoices> mVoicesByLastUsed{};
};

template <typename ProcessorType, typename VoiceType>
class MonophonicVoicePool {
   public:
    MonophonicVoicePool(ProcessorType& p) : mProcessor(p), mVoice(p) {}

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
        for (size_t idx = mActiveNotes.size() - 1; idx >= 0; idx--) {
            if (mActiveNotes[idx].first.Match(note)) {
                mActiveNotes.erase(mActiveNotes.begin() + idx);
                if (idx == mActiveNotes.size() - 1) {
                    // this is the current note, so let's stop it and retrigger the last note if necessary
                    mVoice.ProcessNoteOff();
                    if (!mActiveNotes.empty()) {
                        const auto& [note, velocity] = mActiveNotes.back();
                        mVoice.ProcessNoteOn(note, velocity);
                    }
                }
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

    ProcessorType& mProcessor;
    VoiceType mVoice;
    etl::vector<std::pair<NoteTuple, float>, 16> mActiveNotes{};
    bool mPlaying = false;
};