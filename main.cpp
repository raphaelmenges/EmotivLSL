// Author: Raphael Menges
#include <conio.h>
#include <iostream>
#include <thread>
#include <chrono>

// Including for Emotiv
#include "IEmoStateDLL.h"
#include "Iedk.h"
#include "IEegData.h"
#include "IedkErrorCode.h"

// Including for LabStreamingLayer
#include "lsl_cpp.h"

// Defines
const float bufferInSeconds = 1; // buffer size in seconds for Emotiv

// List of available channels
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

// Corresponding channel labels
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

// Output stream
lsl::stream_info streamInfo("EmotivLSL", "EEG", 14, 128, lsl::cf_float32, "source_id");

// Variables
bool readyToCollect = false; // indicator whether data collection can begin
int state = 0; // current state of the Emotiv device
unsigned int userID = 0; // id of user

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

		// Start filling information about stream
		streamInfo.desc().append_child_value("manufacturer", "Emotiv");

		// Save information about channels
		lsl::xml_element channels = streamInfo.desc().append_child("channels");
		for (auto channelLabel : channelLabels)
		{
			channels.append_child("channel")
					.append_child_value("label", channelLabel)
					.append_child_value("unit", "microvolts")
					.append_child_value("type", "EEG");
		}

		// Create stream outlet with information header
		lsl::stream_outlet outlet(streamInfo);

		// Data handle which holds the buffer
		DataHandle dataStream = IEE_DataCreate();
		IEE_DataSetBufferSizeInSec(bufferInSeconds);

		// Send information as long as no key has been hit
		while (!_kbhit())
		{
			// Fetch current Emotiv state
			state = IEE_EngineGetNextEvent(eEvent);

			// When state is ok, check whether ready to collect
			if (state == EDK_OK)
			{
				IEE_Event_t eventType = IEE_EmoEngineEventGetType(eEvent);
				IEE_EmoEngineEventGetUserId(eEvent, &userID);
				if (eventType == IEE_UserAdded)
				{
					std::cout << "User Successfully Added" << std::endl;
					IEE_DataAcquisitionEnable(userID, true);
					readyToCollect = true;
				}
			}

			// Since it is ready to collect, do it
			if (readyToCollect)
			{
				// Fetch samples and their count
				IEE_DataUpdateHandle(0, dataStream); // update data stream
				unsigned int sampleCount = 0;
				IEE_DataGetNumberOfSample(dataStream, &sampleCount);

				// Proceed when there are samples
				if (sampleCount != 0) {

					// Prepare local buffer for data
					unsigned int channelCount = sizeof(channelList) / sizeof(IEE_DataChannel_t); // extract channel count
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
						outlet.push_sample(values);
					}

					// Delete buffer
					for (int i = 0; i < (int)channelCount; i++)
					{
						delete buffer[i];
					}
					delete buffer;
				}

				// Sleep for a half second to collect further data
				std::this_thread::sleep_for(std::chrono::milliseconds(500));
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