#pragma once

#include <etl/circular_buffer.h>
#include <etl/vector.h>
#include <etl/stack.h>
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
    PolyphonicVoicePool(size_t numVoices = MaxVoices) : mVoices(numVoices) {}

    void SetNumVoices(ProcessorType& p, size_t numVoices) {
        if(numVoices != mVoices.size())
        {
            assert(numVoices <= MaxVoices);
            StopAllVoices(p);
            mVoices.resize(numVoices);
            mVoicesByLastUsed.clear();
        }
    }
    void ProcessNoteOn(ProcessorType& p, const NoteTuple& note, float velocity) {
        // retrigger existing voice, if there is one
        VoiceIndex voiceIndex = SIZE_MAX;
        for (VoiceIndex idx = 0; idx < mVoices.size(); idx++) {
            auto& data = mVoices[idx];
            if (data.activeNote && data.activeNote->Match(note)) {
                voiceIndex = idx;
                SendNoteEnd(p, *data.activeNote);
                break;
            }
            if(data.activeNote == std::nullopt && voiceIndex == SIZE_MAX)
            {
                voiceIndex = idx;
            }
        }
        // no voice available, time to steal
        if (voiceIndex == SIZE_MAX) {
            voiceIndex = mVoicesByLastUsed.front();
            mVoicesByLastUsed.pop();
            auto& data = mVoices[voiceIndex];
            SendNoteEnd(p, *data.activeNote);
        }

        mVoicesByLastUsed.push(voiceIndex);
        mVoices[voiceIndex].activeNote = note;
        mVoices[voiceIndex].voice.ProcessNoteOn(p, note, velocity);
    }
    void ProcessNoteOff(ProcessorType& p, const NoteTuple& note) {
        for (VoiceIndex idx = 0; idx < mVoices.size(); idx++) {
            auto& data = mVoices[idx];
            if (data.activeNote && data.activeNote->Match(note)) {
                data.voice.ProcessNoteOff(p);
            }
        }
    }
    void ProcessNoteChoke(ProcessorType& p, const NoteTuple& note) {
        for (VoiceIndex idx = 0; idx < mVoices.size(); idx++) {
            auto& data = mVoices[idx];
            if (data.activeNote && data.activeNote->Match(note)) {
                StopVoice(p, idx);
            }
        }
    }
    void ProcessAudio(ProcessorType& p, StereoAudioBuffer& out) {
        std::fill(out.left.begin(), out.left.end(), 0);
        std::fill(out.right.begin(), out.right.end(), 0);
        for (VoiceIndex idx = 0; idx < mVoices.size(); idx++) {
            auto& data = mVoices[idx];
            if (data.activeNote && !data.voice.ProcessAudio(p, out)) {
                SendNoteEnd(p, *data.activeNote);
                data.activeNote = std::nullopt;
            }
        }
    }

    void StopAllVoices(ProcessorType& p) {
        for (VoiceIndex idx = 0; idx < mVoices.size(); idx++) {
            StopVoice(p, idx);
        }
    }

   private:
    using VoiceIndex = size_t;
    void StopVoice(ProcessorType& p, VoiceIndex index) {
        auto& data = mVoices[index];
        if (data.activeNote) {
            SendNoteEnd(p, *data.activeNote);
            data.activeNote = std::nullopt;
            data.voice.ProcessChoke(p);
        }
    }

    void SendNoteEnd(ProcessorType &p, const NoteTuple& note) {
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
        p.SendEvent(event.header);
    }

    struct VoiceData {
        VoiceType voice;
        std::optional<NoteTuple> activeNote;
    };

    etl::vector<VoiceData, MaxVoices> mVoices;
    etl::circular_buffer<VoiceIndex, MaxVoices> mVoicesByLastUsed {};
};

template <typename ProcessorType, typename VoiceType>
class MonophonicVoicePool {
   public:
    MonophonicVoicePool() {}

    void ProcessNoteOn(ProcessorType& p, const NoteTuple& note, float velocity) {
        if(!mActiveNotes.empty())
        {
            // existing note playing, let's stash it
            mVoice.ProcessNoteOff(p);
        }

        if(mActiveNotes.full())
        {
            // we're past the internal note tracking limit
            return;
        }
        mActiveNotes.push_back(note);
        mVoice.ProcessNoteOn(p, note, velocity);
        mPlaying = true;
    }
    void ProcessNoteOff(ProcessorType& p, const NoteTuple& note) {
        for(size_t idx = mActiveNotes.size() - 1; idx >= 0; idx--)
        {
            if (mActiveNotes[idx].Match(note)) {
                mActiveNotes.erase(mActiveNotes.begin()+idx);
                if(idx == mActiveNotes.size() - 1)
                {
                    // this is the current note, so let's stop it and retrigger the last note if necessary
                    mVoice.ProcessNoteOff(p);
                    if(!mActiveNotes.empty())
                    {
                        NoteTuple note = mActiveNotes.back();
                        //TODO: whoops we need to track the velocity too
                        mVoice.ProcessNoteOn(p, note, 1.0f);
                    }
                }
            }
        }
    }
    void ProcessNoteChoke(ProcessorType& p, const NoteTuple& note) {
        StopAllVoices(p);
    }
    void ProcessAudio(ProcessorType& p, StereoAudioBuffer& out) {
        std::fill(out.left.begin(), out.left.end(), 0);
        std::fill(out.right.begin(), out.right.end(), 0);
        if (mPlaying && !mVoice.ProcessAudio(p, out)) {
            if(!mActiveNotes.empty())
            {
                SendNoteEnd(p, mActiveNotes.back());
                mActiveNotes.pop_back();
            }
            mPlaying = !mActiveNotes.empty();
        }
    }

    void StopAllVoices(ProcessorType& p) {
        StopVoice(p);
    }

   private:
    void StopVoice(ProcessorType& p) {
        if (mPlaying) {
            while(!mActiveNotes.empty())
            {
                SendNoteEnd(p, mActiveNotes.back());
                mActiveNotes.pop_back();
            }
            mVoice.ProcessChoke(p);
        }
        mPlaying = false;
    }

    void SendNoteEnd(ProcessorType &p, const NoteTuple& note) {
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
        p.SendEvent(event.header);
    }

    VoiceType mVoice {};
    etl::vector<NoteTuple, 16> mActiveNotes {};
    bool mPlaying = false;
};