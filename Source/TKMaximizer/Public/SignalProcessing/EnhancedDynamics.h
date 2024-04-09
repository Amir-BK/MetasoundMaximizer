
#pragma once

#include "CoreMinimal.h"
#include "DSP/Dsp.h"
#include "DSP/DynamicsProcessor.h"
#include "MetaSoundAudioBuffer.h"

using FAudioBufferReadRefArray = TArray<Metasound::FAudioBufferReadRef>;
using FAudioBufferRWriteRefArray = TArray<Metasound::FAudioBufferWriteRef>;


class FEnhancedDynamicsProcessor : public Audio::FDynamicsProcessor
{
    
public:
    float env_state = 0.0f;
    float running_gain_state = 1.0f;
    void ProcessAudioFrame(const float* InFrame, float* OutFrame, const float* InKeyFrame);
    void ProcessAudio(const FAudioBufferReadRefArray &Inputs, FAudioBufferRWriteRefArray &OutBuffer);
    
};

