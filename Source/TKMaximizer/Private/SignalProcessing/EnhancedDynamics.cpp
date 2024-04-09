#include "SignalProcessing/EnhancedDynamics.h"



void FEnhancedDynamicsProcessor::ProcessAudio(const FAudioBufferReadRefArray &Inputs, FAudioBufferRWriteRefArray &OutBuffer) {

    const int32 NumChannels = Inputs.Num();
    const int32 KeyNumChannels = GetKeyNumChannels();
    const int32 InNumSamples = Inputs[0]->Num();

    //printf("InputGain %.2f", InputGain);
    for (int32 sample_index=0; sample_index < InNumSamples; sample_index++){
        
        //Calc max(abs(each channel))
        float max_abs = fabsf(Inputs[0]->GetData()[sample_index] * InputGain);
        for (int32 channel_index=0; channel_index < GetNumChannels(); channel_index++){
            const float sample = fabsf(Inputs[channel_index]->GetData()[sample_index] * InputGain);
            max_abs = fmaxf(max_abs, sample);
        }
        
        const float release = 0.9995;
        const float attack = 0.9; // This should be calculate from time ms (take sample rate into account).
        

        if (max_abs > env_state){
            env_state = env_state * attack + (1-attack) * max_abs;
        } else {
            env_state = env_state * release + (1-release) * max_abs;
        }
        
        float gain_fix = atanf(3.0f * env_state + 0.01f); // This also should be a paramter.
        
        
        for (int32 channel_index=0; channel_index < GetNumChannels(); channel_index++){
            OutBuffer[channel_index]->GetData()[sample_index] = InputGain * Inputs[channel_index]->GetData()[sample_index] / gain_fix ;
        }
    }
    
}

void FEnhancedDynamicsProcessor::ProcessAudioFrame(const float* InFrame, float* OutFrame, const float* InKeyFrame)
    {
        const bool bKeyIsInput = InFrame == InKeyFrame;
        if (ProcessKeyFrame(InKeyFrame, OutFrame, bKeyIsInput))
        {
            const int32 NumChannels = GetNumChannels();
            for (int32 Channel = 0; Channel < NumChannels; ++Channel)
            {
                // Write and read into the look ahead delay line.
                // We apply the compression output of the direct input to the output of this delay line
                // This way sharp transients can be "caught" with the gain.
                float LookaheadOutput = LookaheadDelay[Channel].ProcessAudioSample(InFrame[Channel]);
                
                // Write into the output with the computed gain value
                OutFrame[Channel] = Gain[Channel] * LookaheadOutput * OutputGain * InputGain;
            }
        } else {
            printf("Error! This should not have happned.");
        }
    }
    

