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
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundFacade.h"
#include "MetasoundPrimitives.h"

#define LOCTEXT_NAMESPACE "TKMaximizer_MaximizerNode"

namespace Metasound
{
	/* Mid-Side Encoder */
	namespace MaximizerVertexNames
	{
		METASOUND_PARAM(InputAudio, "Audio", "Incoming audio signal to compress.");
		METASOUND_PARAM(OutputCeilingDb, "Output Ceileing dB", "Gain reduction applied after maximizing and limiting");
		METASOUND_PARAM(InputThresholdDb, "Threshold dB", "Amplitude threshold above which gain will be reduced.");
		METASOUND_PARAM(InputReleaseTime, "Release Time", "How long it takes for audio below the threshold to return to its original volume level.");
		METASOUND_PARAM(InputKneeMode, "Knee", "Whether the limiter uses a hard or soft knee.");

		METASOUND_PARAM(OutputAudio, "Audio", "The output audio signal.");
	}

	enum class EMaximizerKneeMode
	{
		Hard = 0,
		Soft,
	};

	DECLARE_METASOUND_ENUM(EMaximizerKneeMode, EMaximizerKneeMode::Hard, TKMAXIMIZER_API,
	FEnumKneeMode, FEnumKneeModeInfo, FKneeModeReadRef, FEnumKneeModeWriteRef);

	DEFINE_METASOUND_ENUM_BEGIN(EMaximizerKneeMode, FEnumKneeMode, "KneeMode")
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
			const FKneeModeReadRef& InKneeMode)
			: Inputs(InInputBuffers)
			, InGainDbInput(InGainDb)
			, ThresholdDbInput(InThresholdDb)
			, ReleaseTimeInput(InReleaseTime)
			, KneeModeInput(InKneeMode)
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

		static const FNodeClassMetadata& GetNodeInfo()
		{
			auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
				{
					FVertexInterface NodeInterface = DeclareVertexInterface();

					FNodeClassMetadata Metadata
					{
						FNodeClassName{TEXT("MaximizerNode"), TEXT("Maximizer"), TEXT("")},
						0, // Major Version
						1, // Minor Version
						INVTEXT("Maximizer"),
						INVTEXT("Maximizes a signal while limiting its output"),
						TEXT("NeuroVision & Amir Ben-Kiki"),
						PluginNodeMissingPrompt,
						NodeInterface,
						{NodeCategories::Dynamics},
						{},
						FNodeDisplayStyle{}
					};

					return Metadata;
				};

			static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
			return Metadata;
		}

		static const FVertexInterface& DeclareVertexInterface()
		{
			using namespace MaximizerVertexNames;

			auto CreateDefaultInterface = []()-> FVertexInterface
				{
					
					
					FInputVertexInterface InputInterface;
					//dynamics controls

					InputInterface.Add(TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputCeilingDb), 0.0f));
					InputInterface.Add(TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputThresholdDb), 0.0f));


					// audio inputs inputs
					for (uint32 ChanIndex = 0; ChanIndex < NumChannels; ++ChanIndex)
					{
						const FDataVertexMetadata AudioInputMetadata;
						InputInterface.Add(TInputDataVertex<FAudioBuffer>(GetAudioInputName(ChanIndex), AudioInputMetadata));
					}

			
//		,
//		TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputReleaseTime), 0.1f),
//		TInputDataVertex<FEnumKneeMode>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputKneeMode), (int32)EMaximizerKneeMode::Hard)

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



			//static const FVertexInterface Interface(
			//	FInputVertexInterface(
			//		//TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputAudio)),
			//		TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputCeilingDb), 0.0f),
			//		TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputThresholdDb), 0.0f),
			//		TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputReleaseTime), 0.1f),
			//		TInputDataVertex<FEnumKneeMode>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputKneeMode), (int32)EMaximizerKneeMode::Hard)
			//	),
			//	(
			//	FOutputVertexInterface()
			//	//	TOutputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputAudio))
			//	)

	

		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace MaximizerVertexNames;

			//InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputAudio), AudioInput);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputCeilingDb), InGainDbInput);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputThresholdDb), ThresholdDbInput);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputReleaseTime), ReleaseTimeInput);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputKneeMode), KneeModeInput);



			for (uint32 Chan = 0; Chan < NumChannels; ++Chan)
			{
				InOutVertexData.BindReadVertex(GetAudioInputName(Chan), Inputs[Chan]);
			}

				//InOutVertexData.BindReadVertex(GainInputNames[i], Gains[i]);
		
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace MaximizerVertexNames;

			//InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputAudio), AudioOutput);
		}

		virtual FDataReferenceCollection GetInputs() const override
		{
			// This should never be called. Bind(...) is called instead. This method
			// exists as a stop-gap until the API can be deprecated and removed.
			checkNoEntry();
			return {};
		}

		virtual FDataReferenceCollection GetOutputs() const override
		{
			// This should never be called. Bind(...) is called instead. This method
			// exists as a stop-gap until the API can be deprecated and removed.
			checkNoEntry();
			return {};
		}

		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
		{
			using namespace MaximizerVertexNames;

			const FInputVertexInterfaceData& InputData = InParams.InputData;

			TArray<FAudioBufferReadRef> InputBuffers;
			TArray<FFloatReadRef> InputGains;
			
			//const FInputVertexInterfaceData& InputData = InParams.InputData;

			FAudioBufferReadRef AudioIn = InputData.GetOrConstructDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputAudio), InParams.OperatorSettings);
			FFloatReadRef InGainDbIn = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(OutputCeilingDb), InParams.OperatorSettings);
			FFloatReadRef ThresholdDbIn = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputThresholdDb), InParams.OperatorSettings);
			FTimeReadRef ReleaseTimeIn = InputData.GetOrCreateDefaultDataReadReference<FTime>(METASOUND_GET_PARAM_NAME(InputReleaseTime), InParams.OperatorSettings);
			FKneeModeReadRef KneeModeIn = InputData.GetOrCreateDefaultDataReadReference<FEnumKneeMode>(METASOUND_GET_PARAM_NAME(InputKneeMode), InParams.OperatorSettings);

			return MakeUnique<TMaximizerOperator<NumChannels>>(InParams, MoveTemp(InputBuffers), InGainDbIn, ThresholdDbIn, ReleaseTimeIn, KneeModeIn);
		}

		void Reset(const IOperator::FResetParams& InParams)
		{
			//AudioOutput->Zero();

			const float ClampedInGainDb = FMath::Min(*InGainDbInput, MaxInputGain);
			const float ClampedReleaseTime = FMath::Max(FTime::ToMilliseconds(*ReleaseTimeInput), 0.0);

			Limiter.Init(InParams.OperatorSettings.GetSampleRate(), 1);
			Limiter.SetProcessingMode(Audio::EDynamicsProcessingMode::Limiter);
			Limiter.SetInputGain(ClampedInGainDb);
			Limiter.SetThreshold(*ThresholdDbInput);
			Limiter.SetAttackTime(0.0f);
			Limiter.SetReleaseTime(ClampedReleaseTime);
			Limiter.SetPeakMode(Audio::EPeakMode::Peak);

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
			float ClampedInGainDb = FMath::Min(*InGainDbInput, MaxInputGain);
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

			//Limiter.ProcessAudio(AudioInput->GetData(), AudioInput->Num(), AudioOutput->GetData());
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


		FFloatReadRef InGainDbInput;
		FFloatReadRef ThresholdDbInput;
		FTimeReadRef ReleaseTimeInput;
		FKneeModeReadRef KneeModeInput;

		//Add multichannel support

		TArray<FAudioBufferReadRef> Inputs;
		TArray<FAudioBufferWriteRef> Outputs;

		
		// Internal DSP Limiter
		Audio::FDynamicsProcessor Limiter;

		// Cached variables
		float PrevInGainDb;
		float PrevThresholdDb;
		double PrevReleaseTime;
		EMaximizerKneeMode PrevKneeMode;
	};

	//// Node Class
	//class FMaximizerNode : public FNodeFacade
	//{
	//public:
	//	FMaximizerNode(const FNodeInitData& InitData)
	//		: FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<TMaximizerOperator<2>>())
	//	{
	//	}
	//};

	//// Register node
	//METASOUND_REGISTER_NODE(FMaximizerNode)




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
	//	REGISTER_AUDIOMAXIMIZER_NODE(1)
		//REGISTER_AUDIOMAXIMIZER_NODE(2)
		
		REGISTER_AUDIOMAXIMIZER_NODE(4)
	//REGISTER_AUDIOMAXIMIZER_NODE(6)
		//REGISTER_AUDIOMAXIMIZER_NODE(8)
}

#undef LOCTEXT_NAMESPACE
