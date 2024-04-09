// Copyright Epic Games, Inc. All Rights Reserved.

#include "Internationalization/Text.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundEnumRegistrationMacro.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundPrimitives.h"
#include "MetasoundStandardNodesNames.h"
#include "MetasoundAudioBuffer.h"
#include "MetasoundStandardNodesCategories.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "DSP/Dsp.h"
#include "DSP/DynamicsProcessor.h"

#include "SignalProcessing/EnhancedDynamics.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundFacade.h"
#include "MetasoundPrimitives.h"

#define LOCTEXT_NAMESPACE "TKMaximizer_MaximizerNode"

namespace Metasound
{
	namespace MaximizerVertexNames
	{
		METASOUND_PARAM(InputAudio, "Audio", "Incoming audio signal to compress.");
		METASOUND_PARAM(InputGainDb, "Input Gain dB", "Gain reduction applied before limiting");
		METASOUND_PARAM(InputThresholdDb, "Threshold dB", "Amplitude threshold above which gain will be reduced.");
		METASOUND_PARAM(InputReleaseTime, "Release Time", "How long it takes for audio below the threshold to return to its original volume level.");
		METASOUND_PARAM(InputKneeMode, "Knee", "Whether the limiter uses a hard or soft knee.");

		
	}

	enum class EMaximizerKneeMode
	{
		Hard = 0,
		Soft,
	};

	DECLARE_METASOUND_ENUM(EMaximizerKneeMode, EMaximizerKneeMode::Hard, TKMAXIMIZER_API,
	FEnumMaximizerKneeMode, FEnumMaximizerKneeModeInfo, FMaximizerKneeModeReadRef, FEnumMaximizerKneeModeWriteRef);

	DEFINE_METASOUND_ENUM_BEGIN(EMaximizerKneeMode, FEnumMaximizerKneeMode, "MaximizerKneeMode")
		DEFINE_METASOUND_ENUM_ENTRY(EMaximizerKneeMode::Hard, "KneeModeHardDescription", "Hard", "KneeModeHardDescriptionTT", "Only audio strictly above the threshold is affected by the limiter."),
		DEFINE_METASOUND_ENUM_ENTRY(EMaximizerKneeMode::Soft, "KneeModeSoftDescription", "Soft", "KneeModeSoftDescriptionTT", "Limiter activates more smoothly near the threshold."),
		DEFINE_METASOUND_ENUM_END()

	// Operator Class
	template<uint32 NumChannels>
	class TMaximizerOperator : public TExecutableOperator<TMaximizerOperator<NumChannels>>
	{
	public:

		static constexpr float HardKneeBandwitdh = 0.0f;
		static constexpr float SoftKneeBandwitdh = 10.0f;
		static constexpr float MaxInputGain = 100.0f;

		TMaximizerOperator(const FBuildOperatorParams& InParams,
			const TArray<FAudioBufferReadRef>&& InInputBuffers,
			const FFloatReadRef& InGainDb,
			const FFloatReadRef& InThresholdDb,
			const FTimeReadRef& InReleaseTime,
			const FMaximizerKneeModeReadRef& InKneeMode)
			: InInputGainDb(InGainDb)
            , ThresholdDbInput(InThresholdDb)
			, ReleaseTimeInput(InReleaseTime)
			, KneeModeInput(InKneeMode)
            , Inputs(InInputBuffers)
			, Limiter()
			, PrevInGainDb(*InGainDb)
			, PrevThresholdDb(*InThresholdDb)
			, PrevReleaseTime(FMath::Max(FTime::ToMilliseconds(*InReleaseTime), 0.0))
		{
			// create write refs
			for (uint32 i = 0; i < NumChannels; ++i)
			{
				Outputs.Add(FAudioBufferWriteRef::CreateNew(InParams.OperatorSettings));
			}

			Reset(InParams);
		}

		static FNodeClassMetadata CreateNodeClassMetadata(const FName& InOperatorName, const FText& InDisplayName, const FText& InDescription, const FVertexInterface& InDefaultInterface)
		{
			FNodeClassMetadata Metadata
			{
				FNodeClassName { "AudioMaximizer", InOperatorName, FName() },
				0, // Major Version
				2, // Minor Version
				InDisplayName,
				InDescription,
				TEXT("Neurovision & Amir Ben-Kiki"),
				PluginNodeMissingPrompt,
				InDefaultInterface,
				{ NodeCategories::Mix },
				{ INVTEXT("Dynamics")},
				FNodeDisplayStyle{}
			};

			return Metadata;
		}

		static const FNodeClassMetadata& GetNodeInfo()
		{
			// used if NumChannels == 1
			auto CreateNodeClassMetadataMono = []() -> FNodeClassMetadata
				{
					FName OperatorName = *FString::Printf(TEXT("Audio Maximizer (Mono)"));
					FText NodeDisplayName = INVTEXT("Mono Maximizer");
					const FText NodeDescription = INVTEXT("Limits a signal and boosts it");
					FVertexInterface NodeInterface = DeclareVertexInterface();

					return CreateNodeClassMetadata(OperatorName, NodeDisplayName, NodeDescription, NodeInterface);
				};

			// used if NumChannels == 2
			auto CreateNodeClassMetadataStereo = []() -> FNodeClassMetadata
				{
					FName OperatorName = *FString::Printf(TEXT("Audio Maximizer (Stereo)"));
					FText NodeDisplayName = INVTEXT("Stereo Maximizer");
					const FText NodeDescription = INVTEXT("Limits a signal and boosts it");
					FVertexInterface NodeInterface = DeclareVertexInterface();

					return  CreateNodeClassMetadata(OperatorName, NodeDisplayName, NodeDescription, NodeInterface);
				};

			// used if NumChannels > 2
			auto CreateNodeClassMetadataMultiChan = []() -> FNodeClassMetadata
				{
					FName OperatorName = *FString::Printf(TEXT("Audio Maximizer (%d-Channel"), NumChannels);
					FText NodeDisplayName = FText::FromString(FString::Printf(TEXT("Audio Maximizer (%d-Channel)"), NumChannels));
					const FText NodeDescription = INVTEXT("Limits a signal and boosts it");
					FVertexInterface NodeInterface = DeclareVertexInterface();

					return  CreateNodeClassMetadata(OperatorName, NodeDisplayName, NodeDescription, NodeInterface);
				};

			static const FNodeClassMetadata Metadata = (NumChannels == 1) ? CreateNodeClassMetadataMono()
				: (NumChannels == 2) ? CreateNodeClassMetadataStereo() : CreateNodeClassMetadataMultiChan();

			
			return Metadata;
		}

		static const FVertexInterface& DeclareVertexInterface()
		{
			using namespace MaximizerVertexNames;

			auto CreateDefaultInterface = []()-> FVertexInterface
				{
					
					
					FInputVertexInterface InputInterface;
					//dynamics controls

					InputInterface.Add(TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputGainDb), 0.0f));
					InputInterface.Add(TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputThresholdDb), 0.0f));
					InputInterface.Add(TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputReleaseTime), 0.1f));
					InputInterface.Add(TInputDataVertex<FEnumMaximizerKneeMode>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputKneeMode), (int32)EMaximizerKneeMode::Hard));


					// audio inputs inputs
					for (uint32 ChanIndex = 0; ChanIndex < NumChannels; ++ChanIndex)
					{
						const FDataVertexMetadata AudioInputMetadata;
						InputInterface.Add(TInputDataVertex<FAudioBuffer>(GetAudioInputName(ChanIndex), AudioInputMetadata));
					}


					// outputs
					FOutputVertexInterface OutputInterface;
					for (uint32 i = 0; i < NumChannels; ++i)
					{
						const FDataVertexMetadata AudioOutputMetadata;
						OutputInterface.Add(TOutputDataVertex<FAudioBuffer>(AudioOutputNames[i], AudioOutputMetadata));
					}

					return FVertexInterface(InputInterface, OutputInterface);

				};// end lambda: CreateDefaultInterface()

			static const FVertexInterface DefaultInterface = CreateDefaultInterface();
			return DefaultInterface;


			
		}; 


	

		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace MaximizerVertexNames;

			//InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputAudio), AudioInput);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputGainDb), InInputGainDb);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputThresholdDb), ThresholdDbInput);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputReleaseTime), ReleaseTimeInput);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputKneeMode), KneeModeInput);



			for (uint32 Chan = 0; Chan < NumChannels; ++Chan)
			{
				InOutVertexData.BindReadVertex(GetAudioInputName(Chan), Inputs[Chan]);
			}
		
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace MaximizerVertexNames;

			for (uint32 i = 0; i < NumChannels; ++i)
			{
				InOutVertexData.BindReadVertex(AudioOutputNames[i], Outputs[i]);
			}

		}


		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
		{
			using namespace MaximizerVertexNames;

			const FInputVertexInterfaceData& InputData = InParams.InputData;

			TArray<FAudioBufferReadRef> InputBuffers;

			for (uint32 Chan = 0; Chan < NumChannels; ++Chan)
			{
				InputBuffers.Add(InputData.GetOrConstructDataReadReference<FAudioBuffer>(GetAudioInputName(Chan), InParams.OperatorSettings));
			}
					
			//const FInputVertexInterfaceData& InputData = InParams.InputData;

			FFloatReadRef InOutCeileingDb = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputGainDb), InParams.OperatorSettings);
			FFloatReadRef ThresholdDbIn = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputThresholdDb), InParams.OperatorSettings);
			FTimeReadRef ReleaseTimeIn = InputData.GetOrCreateDefaultDataReadReference<FTime>(METASOUND_GET_PARAM_NAME(InputReleaseTime), InParams.OperatorSettings);
			FMaximizerKneeModeReadRef KneeModeIn = InputData.GetOrCreateDefaultDataReadReference<FEnumMaximizerKneeMode>(METASOUND_GET_PARAM_NAME(InputKneeMode), InParams.OperatorSettings);

			return MakeUnique<TMaximizerOperator<NumChannels>>(InParams, MoveTemp(InputBuffers), InOutCeileingDb, ThresholdDbIn, ReleaseTimeIn, KneeModeIn);
		}

		void Reset(const IOperator::FResetParams& InParams)
		{
			//AudioOutput->Zero();

			const float ClampedInGainDb = FMath::Min(*InInputGainDb, MaxInputGain);
			const float ClampedReleaseTime = FMath::Max(FTime::ToMilliseconds(*ReleaseTimeInput), 0.0);

			if (NumChannels == 2)
			{
				Limiter.Init(InParams.OperatorSettings.GetSampleRate(), 4);
			}
			else
			{
				Limiter.Init(InParams.OperatorSettings.GetSampleRate(), NumChannels);
			}

			Limiter.SetProcessingMode(Audio::EDynamicsProcessingMode::Limiter);
			Limiter.SetChannelLinkMode(Audio::EDynamicsProcessorChannelLinkMode::Peak);
			//Limiter.SetLookaheadMsec(10.0f);
			Limiter.SetInputGain(0.0f);
			Limiter.SetThreshold(*ThresholdDbInput);
			Limiter.SetAttackTime(0.0f);
			Limiter.SetReleaseTime(ClampedReleaseTime);
			Limiter.SetPeakMode(Audio::EPeakMode::Peak);
			//Limiter.SetAnalogMode(true);

			switch (*KneeModeInput)
			{
			default:
			case EMaximizerKneeMode::Hard:
				Limiter.SetKneeBandwidth(HardKneeBandwitdh);
				break;
			case EMaximizerKneeMode::Soft:
				Limiter.SetKneeBandwidth(SoftKneeBandwitdh);
				break;
			}

			PrevInGainDb = ClampedInGainDb;
			PrevReleaseTime = ClampedReleaseTime;
			PrevThresholdDb = *ThresholdDbInput;
			PrevKneeMode = *KneeModeInput;
		}

		void Execute()
		{
			
			
			/* Update parameters */
			float ClampedInGainDb = 0.0f;
			
			if (!FMath::IsNearlyEqual(ClampedInGainDb, PrevInGainDb))
			{
				Limiter.SetInputGain(ClampedInGainDb);
				PrevInGainDb = ClampedInGainDb;
			}

			if (!FMath::IsNearlyEqual(*ThresholdDbInput, PrevThresholdDb))
			{
				Limiter.SetThreshold(*ThresholdDbInput);
				PrevThresholdDb = *ThresholdDbInput;
			}
			// Release time cannot be negative
			double CurrRelease = FMath::Max(FTime::ToMilliseconds(*ReleaseTimeInput), 0.0f);
			if (!FMath::IsNearlyEqual(CurrRelease, PrevReleaseTime))
			{
				Limiter.SetReleaseTime(CurrRelease);
				PrevReleaseTime = CurrRelease;
			}

			if (PrevKneeMode != *KneeModeInput)
			{
				switch (*KneeModeInput)
				{
				default:
				case EMaximizerKneeMode::Hard:
					Limiter.SetKneeBandwidth(HardKneeBandwitdh);
					break;
				case EMaximizerKneeMode::Soft:
					Limiter.SetKneeBandwidth(SoftKneeBandwitdh);
					break;
				}			
				PrevKneeMode = *KneeModeInput;
			}

			
			for (int ChannelIndex = 0; ChannelIndex < NumChannels; ++ChannelIndex)
			{
				Limiter.ProcessAudio(Inputs[ChannelIndex]->GetData(), Inputs[ChannelIndex]->Num(), Outputs[ChannelIndex]->GetData());
			}
			Limiter.SetOutputGain(*ThresholdDbInput * -1.0f);
			//
		}

	private:

		static TArray<FVertexName> InitializeAudioInputNames()
		{
			TStringBuilder<32> InputNameStr;
			TArray<FVertexName> Names;
			Names.AddUninitialized(NumChannels);


				for (int ChannelIndex = 0; ChannelIndex < NumChannels; ++ChannelIndex)
				{
					InputNameStr << "In "; //<< ChannelIndex;

					if (NumChannels == 1)
					{
					}
					else if (NumChannels == 2)
					{
						InputNameStr << ' ' << ((!ChannelIndex) ? 'L' : 'R');
					}
					else
					{
						InputNameStr << " " << ChannelIndex;
					}

					UE_LOG(LogTemp, Log, TEXT("Initializing Input Names %s"), *InputNameStr)

					Names[ChannelIndex] = *InputNameStr;
					InputNameStr.Reset();
				}


			return Names;
		}

		static TArray<FVertexName> InitializeAudioOutputNames()
		{
			TStringBuilder<32> AudioOutNameStr;
			TArray<FVertexName> Names;
			Names.AddUninitialized(NumChannels);

			for (int ChanIndex = 0; ChanIndex < NumChannels; ++ChanIndex)
			{
				AudioOutNameStr << "Out";

				if (NumChannels == 1)
				{
				}
				else if (NumChannels == 2)
				{
					AudioOutNameStr << ' ' << ((!ChanIndex) ? 'L' : 'R');
				}
				else
				{
					AudioOutNameStr << ' ' << ChanIndex;
				}

				Names[ChanIndex] = *AudioOutNameStr;
				AudioOutNameStr.Reset();
			}

			return Names;
		}


		static const FVertexName& GetAudioInputName(uint32 ChannelIndex)
		{
			return AudioInputNames[ChannelIndex];
		}




		static inline const TArray<FVertexName> AudioInputNames = InitializeAudioInputNames();
		static inline const TArray<FVertexName> AudioOutputNames = InitializeAudioOutputNames();


		FFloatReadRef InInputGainDb;
		FFloatReadRef ThresholdDbInput;
		FTimeReadRef ReleaseTimeInput;
		FMaximizerKneeModeReadRef KneeModeInput;

		//Add multichannel support

		TArray<FAudioBufferReadRef> Inputs;
		TArray<FAudioBufferWriteRef> Outputs;

		
		// Internal DSP Limiter
		FEnhancedDynamicsProcessor Limiter;

		// Cached variables
		float PrevInGainDb;
		float PrevThresholdDb;
		double PrevReleaseTime;
		EMaximizerKneeMode PrevKneeMode;
	};


		template<uint32 NumChannels>
	class TKMAXIMIZER_API TAudioMaximizerNode : public FNodeFacade
	{
	public:
		/**
		 * Constructor used by the Metasound Frontend.
		 */
		TAudioMaximizerNode(const FNodeInitData& InInitData)
			: FNodeFacade(InInitData.InstanceName, InInitData.InstanceID, TFacadeOperatorClass<TMaximizerOperator<NumChannels>>())
		{}

		virtual ~TAudioMaximizerNode() = default;
	};


#define REGISTER_AUDIOMAXIMIZER_NODE(A) \
		using FAudioMaximizerNode##A = TAudioMaximizerNode<A>; \
		METASOUND_REGISTER_NODE(FAudioMaximizerNode##A) \



		// stereo
	REGISTER_AUDIOMAXIMIZER_NODE(1)
	REGISTER_AUDIOMAXIMIZER_NODE(2)
	REGISTER_AUDIOMAXIMIZER_NODE(4)
	REGISTER_AUDIOMAXIMIZER_NODE(6)
	REGISTER_AUDIOMAXIMIZER_NODE(8)
}

#undef LOCTEXT_NAMESPACE
