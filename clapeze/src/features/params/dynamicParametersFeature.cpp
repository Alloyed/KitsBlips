#include "clapeze/features/params/dynamicParametersFeature.h"
#include <etl/algorithm.h>
#include "clapeze/common.h"

namespace clapeze::params {

DynamicAudioHandle::DynamicAudioHandle(std::vector<std::unique_ptr<BaseParam>>& ref,
                                       size_t numParams,
                                       Queue& mainToAudio,
                                       Queue& audioToMain)
    : mParamsRef(ref), mValues(numParams, 0.0f), mModulations(), mMainToAudio(mainToAudio), mAudioToMain(audioToMain) {}

bool DynamicAudioHandle::ProcessEvent(const clap_event_header_t& event) {
    if (event.space_id == CLAP_CORE_EVENT_SPACE_ID) {
        switch (event.type) {
            case CLAP_EVENT_PARAM_VALUE: {
                const auto& paramChange = reinterpret_cast<const clap_event_param_value_t&>(event);
                SetRawValue(paramChange.param_id, paramChange.value);
                return true;
            }
            case CLAP_EVENT_PARAM_MOD: {
                const auto& paramChange = reinterpret_cast<const clap_event_param_mod_t&>(event);
                NoteTuple nt{paramChange.note_id, paramChange.port_index, paramChange.channel, paramChange.key};
                SetRawModulation(paramChange.param_id, nt, paramChange.amount);
                return true;
            }
            default: {
                break;
            }
        }
    }
    return false;
}

void DynamicAudioHandle::FlushEventsFromMain(BaseProcessor& processor, const clap_output_events_t* out) {
    (void)processor;
    // Send events queued from us to the host
    // Since these all happened on an independent thread, they do not have sample-accurate timing; we'll just
    // send them at the front of the queue.
    Change change;
    while (mMainToAudio.pop(change)) {
        switch (change.type) {
            case ChangeType::StartGesture:
            case ChangeType::StopGesture: {
                clap_event_param_gesture_t event = {};
                event.header.size = sizeof(event);
                event.header.time = 0;
                event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
                event.header.type =
                    static_cast<uint16_t>(change.type == ChangeType::StartGesture ? CLAP_EVENT_PARAM_GESTURE_BEGIN
                                                                                  : CLAP_EVENT_PARAM_GESTURE_END);
                event.header.flags = 0;
                event.param_id = change.id;
                out->try_push(out, &event.header);
                break;
            }
            case ChangeType::SetValue: {
                clap_id index = change.id;
                mValues[index] = change.value;
                if (mHandleChange) {
                    mHandleChange(index);
                }

                clap_event_param_value_t event = {};
                event.header.size = sizeof(event);
                event.header.time = 0;
                event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
                event.header.type = static_cast<uint16_t>(CLAP_EVENT_PARAM_VALUE);
                event.header.flags = 0;
                event.param_id = index;
                event.cookie = nullptr;
                event.note_id = -1;
                event.port_index = -1;
                event.channel = -1;
                event.key = -1;
                event.value = change.value;

                out->try_push(out, &event.header);
                break;
            }
            case ChangeType::SetModulation: {
                // this is if the plugin sets the modulation, i don't think this happens??
                assert(false);
                break;
            }
        }
    }
}
double DynamicAudioHandle::GetRawValue(clap_id id) const {
    if (id < mValues.size()) {
        return mValues[id];
    }
    return 0.0f;
}
void DynamicAudioHandle::SetRawValue(clap_id id, double newValue) {
    if (id < mValues.size()) {
        mValues[id] = newValue;
        mAudioToMain.push({ChangeType::SetValue, id, {}, newValue});
        if (mHandleChange) {
            mHandleChange(id);
        }
    }
}

double DynamicAudioHandle::GetRawModulation(clap_id id, const std::optional<NoteTuple>& note) const {
    // nullopt here means matches global only as opposed to the empty note, which matches all
    if (note == std::nullopt) {
        auto iter = mModulations.find({id, {}});
        if (iter != mModulations.end()) {
            const auto& [_, value] = *iter;
            return value;
        }
    } else {
        // iterating backwards, to go from most-specific to least specific.
        for (auto iter = mModulations.rbegin(); iter != mModulations.rend(); iter++) {
            const auto& [key, value] = *iter;
            const auto& [keyid, keynote] = key;
            if (keyid == id && keynote.Match(*note)) {
                return value;
            }
        }
    }
    return 0.0f;
}
void DynamicAudioHandle::SetRawModulation(clap_id id, const NoteTuple& note, double newModulation) {
    if (id < mValues.size()) {
        mModulations.insert({{id, note}, newModulation});
        mAudioToMain.push({ChangeType::SetModulation, id, note, newModulation});
    }
}

void DynamicAudioHandle::SetModulationMask(const NoteTuple& note) {
    mModulationMask = note;
}

void DynamicAudioHandle::ClearModulation(const NoteTuple& note) {
    for (auto it = mModulations.begin(); it != mModulations.end();) {
        if (it->first.second.Match(note)) {
            it = mModulations.erase(it);
        } else {
            ++it;
        }
    }
}

void DynamicAudioHandle::OnNoteStart(const NoteTuple& note) {
    auto iter = mActiveNotes.find(note);
    if (iter != mActiveNotes.end()) {
        iter->second += 1;
    } else {
        mActiveNotes.insert({note, 1});
    }
    ClearModulation(note);
}

void DynamicAudioHandle::OnNoteEnd(const NoteTuple& note) {
    auto iter = mActiveNotes.find(note);
    if (iter != mActiveNotes.end()) {
        auto& numVoices = iter->second;
        if (numVoices > 1) {
            numVoices -= 1;
        } else {
            mActiveNotes.erase(iter);
            ClearModulation(note);
        }
    }
}

DynamicMainHandle::DynamicMainHandle(size_t numParams, Queue& mainToAudio, Queue& audioToMain)
    : mValues(numParams, 0.0f), mModulations(numParams, 0.0f), mMainToAudio(mainToAudio), mAudioToMain(audioToMain) {}

double DynamicMainHandle::GetRawValue(clap_id id) const {
    if (id >= mValues.size()) {
        return 0.0f;
    }
    return mValues[id];
}

void DynamicMainHandle::SetRawValue(clap_id id, double newValue) {
    if (id < mValues.size()) {
        mValues[id] = newValue;
        mMainToAudio.push({ChangeType::SetValue, id, {}, newValue});
        if (mHandleChange) {
            mHandleChange(id);
        }
    }
}

void DynamicMainHandle::StartGesture(clap_id id) {
    mMainToAudio.push({ChangeType::StartGesture, id, {}, 0.0});
}

void DynamicMainHandle::StopGesture(clap_id id) {
    mMainToAudio.push({ChangeType::StopGesture, id, {}, 0.0});
}

void DynamicMainHandle::FlushFromAudio() {
    Change change;
    while (mAudioToMain.pop(change)) {
        clap_id index = change.id;
        switch (change.type) {
            case ChangeType::SetValue: {
                mValues[index] = change.value;
                if (mHandleChange) {
                    mHandleChange(index);
                }
                break;
            }
            case ChangeType::SetModulation: {
                mModulations[index] = change.value;
                if (mHandleChange) {
                    mHandleChange(index);
                }
                break;
            }
            case ChangeType::StartGesture:
            case ChangeType::StopGesture: {
                break;
            }
        }
    }
}

}  // namespace clapeze::params