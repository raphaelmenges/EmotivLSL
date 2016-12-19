//	The MIT License (MIT)
//
//	Copyright(c) 2016 Raphael Menges
//
//	Permission is hereby granted, free of charge, to any person obtaining a copy
//	of this software and associated documentation files(the "Software"), to deal
//	in the Software without restriction, including without limitation the rights
//	to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
//	copies of the Software, and to permit persons to whom the Software is
//	furnished to do so, subject to the following conditions :
//
//	The above copyright notice and this permission notice shall be included in all
//	copies or substantial portions of the Software.
//
//	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
//	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//	SOFTWARE.

#include <conio.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <limits>

// Including for Emotiv
#include "IEmoStateDLL.h"
#include "Iedk.h"
#include "IEegData.h"
#include "IedkErrorCode.h"

// Including for LabStreamingLayer
#include "lsl_cpp.h"

// Defines
const float bufferInSeconds = 2; // buffer size in seconds for raw EEG data
const int sampleRateEEG = 128; // given by device
const long long sleepDurationInMiliseconds = 50; 

// List of EEG channels
IEE_DataChannel_t channelList[] =
{
	IED_AF3,
	IED_F7,
	IED_F3,
	IED_FC5,
	IED_T7,
	IED_P7,
	IED_O1,
	IED_O2,
	IED_P8,
	IED_T8,
	IED_FC6,
	IED_F4,
	IED_F8,
	IED_AF4,
};

// Corresponding EEG channel labels
const std::vector<std::string> channelLabels =
{
	"AF3",
	"F7",
	"F3",
	"FC5",
	"T7",
	"P7",
	"O1",
	"O2",
	"P8",
	"T8",
	"FC6",
	"F4",
	"F8",
	"AF4",
};

// Corresponding facial expression labels
const std::vector<std::string> facialExpressionLabels
{
	"BLINK",
	"WINK_LEFT",
	"WINK_RIGHT",
	// "HORIEYE",
	"SURPRISE",
	"FROWN",
	"CLENCH",
	"SMILE",
	//"LAUGH",
	//"SMIRK_LEFT",
	//"SMIRK_RIGHT",
	"NEUTRAL"
};

// Extract EEG channel count
unsigned int channelCount = sizeof(channelList) / sizeof(IEE_DataChannel_t);


// Counter for EEG in order to collect samples from a complete second
float EEGcounter = 0.f; // seconds

// Output streams
lsl::stream_info streamInfoEEG("EmotivLSL_EEG", "EEG", channelCount, sampleRateEEG, lsl::cf_float32, "source_id");
lsl::stream_info streamInfoFacialExpression("EmotivLSL_FacialExpression", "VALUE", facialExpressionLabels.size(), lsl::IRREGULAR_RATE, lsl::cf_float32, "source_id");
lsl::stream_info streamInfoPerformanceMetrics("EmotivLSL_PerformanceMetrics", "VALUE", 20, lsl::IRREGULAR_RATE, lsl::cf_float32, "source_id");

// Variables
bool readyToCollect = false; // indicator whether data collection can begin
int error = 0; // storage for error code
unsigned int userID = 0; // id of user

// Forward declaration
void CaculateScale(double& rawScore, double& maxScale, double& minScale, double& scaledScore);

// Main function
int main()
{
	// Prepare connection
	EmoEngineEventHandle eEvent = IEE_EmoEngineEventCreate();
	EmoStateHandle eState = IEE_EmoStateCreate();

	// Try to connect and send data to LabStreamingLayer
	try
	{
		// Welcoming
		std::cout << "===================================================================" << std::endl;
		std::cout << "====================== Welcome to EmotivLSL =======================" << std::endl;
		std::cout << "===================================================================" << std::endl;

		// Check connection
		if (IEE_EngineConnect() != EDK_OK)
		{
			throw std::runtime_error("Emotiv Driver Start Up Failed.");
		}

		// ##############################
		// ### EEG STREAM PREPARATION ###
		// ##############################

		// Start filling information about stream
		streamInfoEEG.desc().append_child_value("manufacturer", "Emotiv");

		// Save information about channels
		lsl::xml_element channels = streamInfoEEG.desc().append_child("channels");
		for (auto channelLabel : channelLabels)
		{
			channels.append_child("channel")
					.append_child_value("label", channelLabel)
					.append_child_value("unit", "microvolts")
					.append_child_value("type", "EEG");
		}

		// Create stream outlet with information header
		lsl::stream_outlet outletEEG(streamInfoEEG);

		// Data handle which holds the buffer
		DataHandle dataStream = IEE_DataCreate();
		IEE_DataSetBufferSizeInSec(bufferInSeconds);

		// #####################################
		// ### FACIAL EXPRESSION PREPARATION ###
		// #####################################

		// Start filling information about stream
		streamInfoFacialExpression.desc().append_child_value("manufacturer", "Emotiv");

		// Save information about facial expressions
		lsl::xml_element facialExpressions = streamInfoEEG.desc().append_child("channels");
		for (auto facialExpressionLabel : facialExpressionLabels)
		{
			facialExpressions.append_child("channel")
				.append_child_value("label", facialExpressionLabel);
		}

		// Create stream outlet with information header
		lsl::stream_outlet outletFacialExpression(streamInfoFacialExpression);

		// #######################################
		// ### PERFORMANCE METRICS PREPARATION ###
		// #######################################

		// Start filling information about stream
		streamInfoPerformanceMetrics.desc().append_child_value("manufacturer", "Emotiv");

		// Save information about performance metrics
		lsl::xml_element performanceMetrics = streamInfoPerformanceMetrics.desc().append_child("channels");

		// Stress
		performanceMetrics.append_child("channel")
			.append_child_value("label", "Stress raw score");
		performanceMetrics.append_child("channel")
			.append_child_value("label", "Stress min score");
		performanceMetrics.append_child("channel")
			.append_child_value("label", "Stress max score");
		performanceMetrics.append_child("channel")
			.append_child_value("label", "Stress scaled score");

		// Boredom
		performanceMetrics.append_child("channel")
			.append_child_value("label", "Engagement boredom raw score");
		performanceMetrics.append_child("channel")
			.append_child_value("label", "Engagement boredom min score");
		performanceMetrics.append_child("channel")
			.append_child_value("label", "Engagement boredom max score");
		performanceMetrics.append_child("channel")
			.append_child_value("label", "Engagement boredom scaled score");

		// Relaxation
		performanceMetrics.append_child("channel")
			.append_child_value("label", "Relaxation raw score");
		performanceMetrics.append_child("channel")
			.append_child_value("label", "Relaxation min score");
		performanceMetrics.append_child("channel")
			.append_child_value("label", "Relaxation max score");
		performanceMetrics.append_child("channel")
			.append_child_value("label", "Relaxation scaled score");

		// Excitement
		performanceMetrics.append_child("channel")
			.append_child_value("label", "Excitement raw score");
		performanceMetrics.append_child("channel")
			.append_child_value("label", "Excitement min score");
		performanceMetrics.append_child("channel")
			.append_child_value("label", "Excitement max score");
		performanceMetrics.append_child("channel")
			.append_child_value("label", "Excitement scaled score");

		// Interest
		performanceMetrics.append_child("channel")
			.append_child_value("label", "Interest raw score");
		performanceMetrics.append_child("channel")
			.append_child_value("label", "Interest min score");
		performanceMetrics.append_child("channel")
			.append_child_value("label", "Interest max score");
		performanceMetrics.append_child("channel")
			.append_child_value("label", "Interest scaled score");

		lsl::stream_outlet outletPerformanceMetrics(streamInfoPerformanceMetrics);

		// #######################
		// ### ENTER MAIN LOOP ###
		// #######################

		// Send information as long as no key has been hit
		while (!_kbhit())
		{
			// Fetch current Emotiv state
			error = IEE_EngineGetNextEvent(eEvent); // fills eEvent

			// When state is ok, check whether ready to collect
			if (error == EDK_OK)
			{
				// Extract current event
				IEE_Event_t eventType = IEE_EmoEngineEventGetType(eEvent); // fills eventType
				bool eStateUpdated = false;

				// React to event
				switch (eventType)
				{
				case IEE_UserAdded: // event tells about added user
					IEE_EmoEngineEventGetUserId(eEvent, &userID);
					IEE_DataAcquisitionEnable(userID, true);
					readyToCollect = true;
					std::cout << "User Successfully Added" << std::endl;
					break;

				case IEE_UserRemoved: // event tells about removed user
					readyToCollect = false;
					std::cout << "User Removed" << std::endl;
					break;

				case IEE_EmoStateUpdated: // event tells about updated emo state
					IEE_EmoEngineEventGetEmoState(eEvent, eState); // fills eState
					eStateUpdated = true;
					break;
				}

				// Since it is ready to collect, do it
				if (readyToCollect)
				{
					// ############################
					// ### EEG STREAM EXECUTION ###
					// ############################

					// Collect data after one seconds
					if (EEGcounter >= 1.f)
					{
						// Fetch samples and their count
						IEE_DataUpdateHandle(0, dataStream); // update data stream
						unsigned int sampleCount = 0;
						IEE_DataGetNumberOfSample(dataStream, &sampleCount);
						std::cout << "EEG Sample Count: " << std::to_string(sampleCount) << std::endl; // should be 128

						// Proceed when there are samples
						if (sampleCount != 0) {

							// Prepare local buffer for data
							double ** buffer = new double*[channelCount]; // create buffer for channel data
							for (int i = 0; i < (int)channelCount; i++)
							{
								buffer[i] = new double[sampleCount];
							}

							// Fetch data
							IEE_DataGetMultiChannels(dataStream, channelList, channelCount, buffer, sampleCount);

							// Output samples to LabStreamingLayer
							for (int sampleIdx = 0; sampleIdx < (int)sampleCount; sampleIdx++) // go over samples
							{
								// Copy data to std vector (some overhead but looks nicer)
								std::vector<float> values;
								for (int channelIdx = 0; channelIdx < (int)channelCount; channelIdx++) // go over channels
								{
									values.push_back((float)buffer[channelIdx][sampleIdx]);
								}
								outletEEG.push_sample(values);
							}

							// Delete buffer
							for (int i = 0; i < (int)channelCount; i++)
							{
								delete buffer[i];
							}
							delete buffer;
						}

						EEGcounter -= 1.f;
					}
					
					// Increment eeg counter by sleep time
					EEGcounter += (1000.f / sleepDurationInMiliseconds);

					// ##########################################
					// ### FACIAL EXPRESSION STREAM EXECUTION ###
					// ##########################################

					// TODO: what about the training stuff in the example code?
					if (eStateUpdated)
					{
						std::vector<float> values;

						// Get face information
						IEE_FacialExpressionAlgo_t upperFaceType = IS_FacialExpressionGetUpperFaceAction(eState);
						IEE_FacialExpressionAlgo_t lowerFaceType = IS_FacialExpressionGetLowerFaceAction(eState);
						float upperFaceAmp = IS_FacialExpressionGetUpperFaceActionPower(eState);
						float lowerFaceAmp = IS_FacialExpressionGetLowerFaceActionPower(eState);

						// Blink
						if (IS_FacialExpressionIsBlink(eState))
						{
							values.push_back(1.f);
						}
						else
						{
							values.push_back(0.f);
						}

						// Wink left
						if (IS_FacialExpressionIsLeftWink(eState))
						{
							values.push_back(1.f);
						}
						else
						{
							values.push_back(0.f);
						}

						// Wink right
						if (IS_FacialExpressionIsRightWink(eState))
						{
							values.push_back(1.f);
						}
						else
						{
							values.push_back(0.f);
						}

						// Suprise
						if (upperFaceAmp > 0.0 && upperFaceType == FE_SURPRISE)
						{
							values.push_back(1.f);
						}
						else
						{
							values.push_back(0.f);
						}

						// Frown
						if (upperFaceAmp > 0.0 && upperFaceType == FE_FROWN)
						{
							values.push_back(1.f);
						}
						else
						{
							values.push_back(0.f);
						}

						// Clench
						if (lowerFaceAmp > 0.0 && lowerFaceType = FE_CLENCH)
						{
							values.push_back(1.f);
						}
						else
						{
							values.push_back(0.f);
						}

						// Smile
						if (lowerFaceAmp > 0.0 && lowerFaceType = FE_SMILE)
						{
							values.push_back(1.f);
						}
						else
						{
							values.push_back(0.f);
						}

						// Neutral
						bool neutral = true; // if nothing else is set, set it to neutral
						for (const float& rValue : values) { if (rValue > 0.f) { neutral = false; break; } }
						if(neutral)
						{
							values.push_back(1.f);
						}
						else
						{
							values.push_back(0.f);
						}

						// Push back sample
						outletFacialExpression.push_sample(values);
					}

					// #######################################
					// ### PERFORMANCE METRICS PREPARATION ###
					// #######################################

					if (eStateUpdated)
					{
						std::vector<float> values;
						double rawScore = 0;
						double minScale = 0;
						double maxScale = 0;
						double scaledScore = 0;

						// Stress
						IS_PerformanceMetricGetStressModelParams(eState, &rawScore, &minScale,
							&maxScale);
						values.push_back(rawScore);
						values.push_back(minScale);
						values.push_back(maxScale);
						if (minScale == maxScale)
						{
							values.push_back(std::numeric_limits<double>::quiet_NaN());
						}
						else
						{
							CaculateScale(rawScore, maxScale, minScale, scaledScore);
							values.push_back(scaledScore);
						}

						// Boredom
						IS_PerformanceMetricGetEngagementBoredomModelParams(eState, &rawScore,
							&minScale, &maxScale);
						values.push_back(rawScore);
						values.push_back(minScale);
						values.push_back(maxScale);
						if (minScale == maxScale)
						{
							values.push_back(std::numeric_limits<double>::quiet_NaN());
						}
						else
						{
							CaculateScale(rawScore, maxScale, minScale, scaledScore);
							values.push_back(scaledScore);
						}

						// Relaxation
						IS_PerformanceMetricGetRelaxationModelParams(eState, &rawScore,
							&minScale, &maxScale);
						values.push_back(rawScore);
						values.push_back(minScale);
						values.push_back(maxScale);
						if (minScale == maxScale)
						{
							values.push_back(std::numeric_limits<double>::quiet_NaN());
						}
						else
						{
							CaculateScale(rawScore, maxScale, minScale, scaledScore);
							values.push_back(scaledScore);
						}

						// Excitement
						IS_PerformanceMetricGetInstantaneousExcitementModelParams(eState,
							&rawScore, &minScale,
							&maxScale);
						values.push_back(rawScore);
						values.push_back(minScale);
						values.push_back(maxScale);
						if (minScale == maxScale)
						{
							values.push_back(std::numeric_limits<double>::quiet_NaN());
						}
						else {
							CaculateScale(rawScore, maxScale, minScale, scaledScore);
							values.push_back(scaledScore);
						}

						// Interest
						IS_PerformanceMetricGetInterestModelParams(eState, &rawScore,
							&minScale, &maxScale);
						values.push_back(rawScore);
						values.push_back(minScale);
						values.push_back(maxScale);
						if (minScale == maxScale)
						{
							values.push_back(std::numeric_limits<double>::quiet_NaN());
						}
						else {
							CaculateScale(rawScore, maxScale, minScale, scaledScore);
							values.push_back(scaledScore);
						}

						// Push back sample
						outletPerformanceMetrics.push_sample(values);
					}

					// #############
					// ### SLEEP ###
					// #############

					// Sleep for a second to collect further data
					std::this_thread::sleep_for(std::chrono::milliseconds(sleepDurationInMiliseconds));
				}
			}
		}

		// Free data
		IEE_DataFree(dataStream);
	}
	catch (const std::runtime_error& e) // some exception occured
	{
		std::cerr << e.what() << std::endl;
		std::cout << "Press Any Key To Exit..." << std::endl;
		getchar();
	}

	// Disconnect from Emotiv device
	IEE_EngineDisconnect();
	IEE_EmoStateFree(eState);
	IEE_EmoEngineEventFree(eEvent);

	return 0;
}

void CaculateScale(double& rawScore, double& maxScale, double& minScale, double& scaledScore)
{
	if (rawScore < minScale)
	{
		scaledScore = 0;
	}
	else if (rawScore > maxScale)
	{
		scaledScore = 1;
	}
	else
	{
		scaledScore = (rawScore - minScale) / (maxScale - minScale);
	}
}