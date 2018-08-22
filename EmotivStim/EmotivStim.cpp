// EmotivStim.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <iostream>
#include <iomanip>
#include <time.h>
#include <string.h>
#include <ctime>
#include <chrono>
#include <process.h> // _beginthread(f,stacksize,*arglist)
#include "EmoStateDLL.h"
#include "edk.h"
#include "edkErrorCode.h"
#include "SerialPort.h"


#define MAX_DATA_LENGTH 2

#define TARGETS_PER_TRIAL 20
#define RANDOM_NUMS 4
#define INTERFLASH_INTERVAL 400

char* portName = "\\\\.\\COM3";
char incomingData[MAX_DATA_LENGTH];

char t[] = "1";
char nt[] = "2";
int loopCount = 0;
int lastTarget = 0;
int trialCount = 0;
bool trialDelay = false;
SerialPort *arduino;

int randomNum = 0;
int t_count = 0;
int nt_count = 0;

bool running = true; /* flag used to stop the program execution */
void StimulusGenerator(void *unused); /* Threaded function to generate stimulii and send them to Arduino MCU */
void EmotivDataCollector(void *unused); /* Threaded function to capture EEG and save in file */
int marker = 0; /* marker value coming from Stimulus Generator thread */


std::string GetTimeStr() { // Returns time in the format 2011.08.22-21.37.200 for file naming
	std::stringstream timeStr;
	auto t = std::time(nullptr);
	auto tm = *std::localtime(&t);	
	timeStr << std::put_time(&tm, "%d.%m.%Y-%H.%M.%S");
	return timeStr.str();
}

int main(int argc, char *argv[]) {
	//int _tmain(int argc, _TCHAR* argv[]) {
	std::cout << "EmotivStim v1.0 2018 - Emotiv EPOC EEG Data Logger for Light Flash Based P300 Experiments++" << std::endl;
	//std::cout << "Developed by Hiran Ekanayake (hiran.ekanayake@gmail.com)" << std::endl;
	std::cout << "This software has used external libraries such as edk.lib" << std::endl;
	std::cout << "*****************************************************************" << std::endl;
	std::cout << "Make sure to connect the EPOC headset and to connect the Arduino..." << std::endl;

	
	std::cout << "*****************************************************************" << std::endl;
	std::cout << "Press any key to start running (press again to stop)..." << std::endl;
	getchar();

	_beginthread(EmotivDataCollector, 0, NULL);
	_beginthread(StimulusGenerator, 0, NULL);
	getchar(); // or std::cin.get();

	running = false;
	std::cout << "*****************************************************************" << std::endl;
	std::cout << "Press any key to exit..." << std::endl;
	getchar();
	return 0;
}

void StimulusGenerator(void *unused)
{
	arduino = new SerialPort(portName);

	//Checking if arduino is connected or not
	if (arduino->isConnected()){
		std::cout << "Connection established at port " << portName << std::endl << std::endl;
	}
	while (running) {

		clock_t begin;
		clock_t end;

		//if (lastTarget + 2 < loopCount){ //generate a new random number if last 2 loops were not targets
		//	randomNum = rand() % RANDOM_NUMS;  //random number
		//}
		//else{
		//	randomNum = (rand() % (RANDOM_NUMS - 1)) + 1;
		//}

		randomNum = rand() % RANDOM_NUMS;  //random number

		if (randomNum == 0 || loopCount == 0){
			lastTarget = loopCount;

			if (t_count % (TARGETS_PER_TRIAL) == 0){
				if (!trialDelay){
					trialCount++;
					std::cout << std::endl << std::endl << "Beginning Trial: " << trialCount << std::endl << std::endl;
					arduino->writeSerialPort("c\0", MAX_DATA_LENGTH);
					Sleep(4000); //inter-trial delay
					trialDelay = true;
					randomNum = 1; //next flash cannot be a target
				}
				else{
					t_count++;
				}
			}
			else{
				trialDelay = false;
				t_count++;
			}
		}
		else{
			nt_count++;
		}
		begin = clock(); 

		if (randomNum == 0){
			//randomNum = 1; //next flash cannot be a target
			arduino->writeSerialPort("1\0", MAX_DATA_LENGTH);
			marker = 1;
			printf("\t\t\t Sent: %s -> ", "1\0");
		}
		else{
			arduino->writeSerialPort("2\0", MAX_DATA_LENGTH);
			marker = 2;
			printf("sent: %s -> ", nt);
		}
		int readResult;
		readResult = arduino->readBlockingSerialPort(incomingData, MAX_DATA_LENGTH);

		
		end = clock();
		double elapsed_secs = double(end - begin) / (CLOCKS_PER_SEC / 1000);
		
		printf("received: %s\n\t\t\t\t\t\t delay: %fms targets: %d non-targets %d\n", incomingData, elapsed_secs, t_count, nt_count);
		incomingData[0] = '\0';

		loopCount++;
		Sleep(INTERFLASH_INTERVAL);
	}
}

void EmotivDataCollector(void *unused) {
	try {
		EE_DataChannel_t targetChannelList[] = {
			ED_COUNTER,
			ED_AF3, ED_F7, ED_F3, ED_FC5, ED_T7,
			ED_P7, ED_O1, ED_O2, ED_P8, ED_T8,
			ED_FC6, ED_F4, ED_F8, ED_AF4, ED_GYROX, ED_GYROY, ED_TIMESTAMP,
			ED_FUNC_ID, ED_FUNC_VALUE, ED_MARKER, ED_SYNC_SIGNAL
		};

		const char header[] = "COUNTER,AF3,F7,F3,FC5,T7,P7,O1,O2,P8"
			",T8,FC6,F4,F8,AF4,GYROX,GYROY,TIMESTAMP,"
			"FUNC_ID,FUNC_VALUE,MARKER,SYNC_SIGNAL,STIM,";

		EmoEngineEventHandle eEvent = EE_EmoEngineEventCreate();
		EmoStateHandle eState = EE_EmoStateCreate();
		unsigned int userID = 0;
		const unsigned short composerPort = 1726;
		float secs = 1;
		unsigned int datarate = 0;
		bool collectEEG = false;
		int option = 0;
		int state = 0;

		if (EE_EngineConnect() != EDK_OK) {
			std::cout << "Emotiv Engine start up failed." << std::endl;
			running = false;
		}
		else {
			std::stringstream filename, filename1;

			filename << "eeglog-" << GetTimeStr() << ".csv"; // EEG log filename		
			std::ofstream ofs(filename.str(), std::ios::trunc);
			ofs << header << std::endl;


			DataHandle hData = EE_DataCreate();
			EE_DataSetBufferSizeInSec(secs);

			std::cout << "EEG buffer size in secs:" << secs << std::endl;

			while (running) {
				state = EE_EngineGetNextEvent(eEvent);

				if (state == EDK_OK) {
					EE_Event_t eventType = EE_EmoEngineEventGetType(eEvent);
					EE_EmoEngineEventGetUserId(eEvent, &userID);

					// Log the EmoState if it has been updated
					if (eventType == EE_UserAdded) {
						//std::cout << "User added";
						EE_DataAcquisitionEnable(userID, true);
						collectEEG = true;
					}

					// Extended...
					// Log the EmoState if it has been updated
					//if (eventType == EE_EmoStateUpdated) {
					//	EE_EmoEngineEventGetEmoState(eEvent, eState);
					//	//const float timestamp = ES_GetTimeFromStart(eState);
					//	//printf("%10.3fs : New EmoState from user %d ...\r", timestamp, userID);
					//	//logEmoState(ofs, userID, eState, writeHeader);
					//	//writeHeader = false;
					//}
				}

				if (collectEEG) {
					EE_DataUpdateHandle(0, hData);

					unsigned int nSamplesTaken = 0;
					EE_DataGetNumberOfSample(hData, &nSamplesTaken);

					if (nSamplesTaken != 0) {

						double* data = new double[nSamplesTaken];
						for (int sampleIdx = 0; sampleIdx<(int)nSamplesTaken; ++sampleIdx) {
							for (int i = 0; i<sizeof(targetChannelList) / sizeof(EE_DataChannel_t); i++) {

								EE_DataGet(hData, targetChannelList[i], data, nSamplesTaken);
								ofs << data[sampleIdx] << ",";
								//std::cout << data[sampleIdx] << ",";
							}
							ofs << marker << std::endl;
							marker = 0;
						}
						delete[] data;
					}
				}
				Sleep(1);
			}

			ofs.close();
			EE_DataFree(hData);
		}
		//	catch (const std::exception& e) {
		//		std::cerr << e.what() << std::endl;
		//		std::cout << "Press any key to exit..." << std::endl;
		//		getchar();
		//	}

		EE_EngineDisconnect();
		EE_EmoStateFree(eState);
		EE_EmoEngineEventFree(eEvent);
	}
	catch (...) {
		std::cout << "Exception occured in the EEG logger!" << std::endl;
		running = false;
	}
	std::cout << "Exiting from Emotiv connector..." << std::endl;
}