#include "clapeze/params/enumParametersFeature.h"

namespace clapeze::params {

EnumMainHandle::EnumMainHandle(size_t numParams, Queue& mainToAudio, Queue& audioToMain)
    : mValues(numParams, 0.0f), mModulations(numParams, 0.0f), mMainToAudio(mainToAudio), mAudioToMain(audioToMain) {}

double EnumMainHandle::GetRawValue(Id id) const {
    clap_id index = static_cast<clap_id>(id);
    if (index >= mValues.size()) {
        return 0.0f;
    }
    return mValues[index];
}

void EnumMainHandle::SetRawValue(Id id, double newValue) {
    clap_id index = static_cast<clap_id>(id);
    if (index < mValues.size()) {
        mValues[index] = newValue;
        mMainToAudio.push({ChangeType::SetValue, id, newValue});
    }
}

void EnumMainHandle::FlushFromAudio() {
    Change change;
    while (mAudioToMain.pop(change)) {
        clap_id index = static_cast<clap_id>(change.id);
        switch (change.type) {
            case ChangeType::SetValue: {
                mValues[index] = change.value;
                break;
            }
            case ChangeType::SetModulation: {
                mModulations[index] = change.value;
                break;
            }
            case ChangeType::StartGesture:
            case ChangeType::StopGesture: {
                break;
            }
        }
    }
}

void EnumMainHandle::StartGesture(Id id) {
    mMainToAudio.push({ChangeType::StartGesture, id, 0.0});
}

void EnumMainHandle::StopGesture(Id id) {
    mMainToAudio.push({ChangeType::StopGesture, id, 0.0});
}

}  // namespace clapeze::params