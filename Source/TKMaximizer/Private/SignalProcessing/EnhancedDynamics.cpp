#include "SignalProcessing/EnhancedDynamics.h"



void FEnhancedDynamicsProcessor::ProcessAudio(const FAudioBufferReadRefArray &Inputs, FAudioBufferRWriteRefArray &OutBuffer) {

    const int32 NumChannels = Inputs.Num();
    const int32 KeyNumChannels = GetKeyNumChannels();
    const int32 InNumSamples = Inputs[0]->Num();

    //printf("InputGain %.2f", InputGain);
    for (int32 sample_index=0; sample_index < InNumSamples; sample_index++){
        
        //Calc max(abs(each channel))
        float max_abs = fabsf(Inputs[0]->GetData()[sample_index]);
        for (int32 channel_index=0; channel_index < GetNumChannels(); channel_index++){
            const float sample = fabsf(Inputs[channel_index]->GetData()[sample_index]);
            max_abs = fmaxf(max_abs, sample);
        }
        
        const float release = 0.9995;
        const float attack = 0.9; // This should be calculate from time ms (take sample rate into account).
        

        if (max_abs > env_state){
            env_state = env_state * attack + (1-attack) * max_abs;
        } else {
            env_state = env_state * release + (1-release) * max_abs;
        }
        const float curve_value = 4;
        const float max_curve_value = atanf(curve_value + 0.01f);  // The maximum value possible for input values 0..1.
        float env_after_fitting_curve = atanf(curve_value * env_state + 0.01f); // This also should be a parameter.
        //Normlize to 0..1
        env_after_fitting_curve /= max_curve_value;
        
        //Use Input gain as linear factor from linear to curve
        env_after_fitting_curve = InputGain * env_after_fitting_curve + (1.0f - InputGain) * 1.0;
        //Smooth running gain
        if (env_after_fitting_curve<running_gain_state) {
            //Want to lower the gain?
            running_gain_state = running_gain_state * attack + (1.0f - attack) * env_after_fitting_curve;
        } else{
            running_gain_state = running_gain_state * release + (1.0f - release) * env_after_fitting_curve;
        }
        
        //float interploated_value = 1.0f / (InputGain * gain_fix + (1.0f - InputGain) * 1.0);
        //Clamp
        //interploated_value = fminf(interploated_value, )
        for (int32 channel_index=0; channel_index < GetNumChannels(); channel_index++){
            OutBuffer[channel_index]->GetData()[sample_index] = running_gain_state * Inputs[channel_index]->GetData()[sample_index]  ;
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
    

