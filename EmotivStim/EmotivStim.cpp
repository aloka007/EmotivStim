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
#include <string>
#include <cstdlib>
#include <algorithm>
#include <ctime>
#include <chrono>
#include <process.h> // _beginthread(f,stacksize,*arglist)
#include "EmoStateDLL.h"
#include "edk.h"
#include "edkErrorCode.h"
#include "SerialPort.h"


#define DATA_LENGTH 1

#define TRIALS_PER_SERIES 120
#define RANDOM_NUMS 4
#define INTERFLASH_INTERVAL 300

//Classifier Parameters - Integration
//#define WINDOW_OFFSET 155
//#define WINDOW_SIZE 8

//Classifier Parameters - Peak Comparison
#define WINDOW_OFFSET 140
#define WINDOW_SIZE 38

char* portName = "\\\\.\\COM6";
char incomingData[DATA_LENGTH];

char t[] = "1";
char nt[] = "2";
int loopCount = 0;
int trialCount = 0;
int seriesCount = 0;
SerialPort *arduino;

int randomNum = 0;

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

bool copyFile(const char *SRC, const char* DEST) //Copies file from program folder to a common folder
{
	std::ifstream src(SRC, std::ios::binary);
	std::ofstream dest(DEST, std::ios::binary);
	dest << src.rdbuf();
	return src && dest;
}



void StimulusGenerator(void *unused)
{
	arduino = new SerialPort(portName);

	//Checking if arduino is connected or not
	if (arduino->isConnected()){
		std::cout << "Connection established at port " << portName << std::endl << std::endl;
		//arduino->writeSerialPort("0", DATA_LENGTH);
		int readResult = arduino->readSerialPort(incomingData, DATA_LENGTH);
		readResult = arduino->readSerialPort(incomingData, DATA_LENGTH); //Clear incoming buffer
		readResult = arduino->readSerialPort(incomingData, DATA_LENGTH);
		readResult = arduino->readSerialPort(incomingData, DATA_LENGTH);
	}
	while (running) {

		clock_t begin;
		clock_t end;

		char sendbuf[] = "  ";

		if (trialCount % TRIALS_PER_SERIES == 0){ // Inter-Trial code *******************************************

			if (seriesCount == 2){
				running = false;
				break;
			}

			seriesCount++;
			std::cout << std::endl << std::endl << "Beginning Trial Series: " << seriesCount << std::endl << std::endl;
			arduino->writeSerialPort("c\0", DATA_LENGTH);
			arduino->readBlockingSerialPort(incomingData, DATA_LENGTH);
			Sleep(3000); //inter-trial delay
			arduino->writeSerialPort("d\0", DATA_LENGTH);
			arduino->readBlockingSerialPort(incomingData, DATA_LENGTH);
			Sleep(500);
		}

		randomNum =  (rand() % RANDOM_NUMS) + 1;  //random number
		trialCount++;

		strcpy(sendbuf, std::to_string(randomNum).c_str());
		begin = clock();
		arduino->writeSerialPort(sendbuf, DATA_LENGTH);  //Send random number to Arduino
		marker = randomNum;
		//THE WAIT!!!
		int readResult = arduino->readBlockingSerialPort(incomingData, DATA_LENGTH); //Receive reply from Arduino
		end = clock();

		double elapsed_secs = double(end - begin) / (CLOCKS_PER_SEC / 1000);
		printf("\t\t\t\t\tSent-Received: %s-%c\t delay: %fms Trial Count: %d Targets (Est): %d\n", sendbuf, incomingData[0], elapsed_secs, trialCount, trialCount / RANDOM_NUMS);

		if (sendbuf[0] != incomingData[0]){ //Compare the sent and received markers and stop if they are not equal
			printf("SERIAL COM OUT OF SYNC!\nRESYNCING...\n\n");

			readResult = arduino->readSerialPort(incomingData, DATA_LENGTH); //Clear incoming buffer
			readResult = arduino->readSerialPort(incomingData, DATA_LENGTH);
			readResult = arduino->readSerialPort(incomingData, DATA_LENGTH);

			//running = false;
		}

		incomingData[0] = '\0';

		loopCount++;
		Sleep(INTERFLASH_INTERVAL);
	}
	arduino->writeSerialPort("a\0", DATA_LENGTH);
	arduino->readBlockingSerialPort(incomingData, DATA_LENGTH);
	arduino->writeSerialPort("c\0", DATA_LENGTH);
	arduino->readBlockingSerialPort(incomingData, DATA_LENGTH);
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
			std::stringstream filename, filename2, source, destination, srcPath, destPath, destName;
			//Copying paths
			source << "C:/Users/Tharinda/Documents/Visual Studio 2013/Projects/EmotivStim/Debug/";
			
			destination << "C:/Users/Tharinda/Documents/MATLAB/eeglab14_1_1b/eeg-common/";
			destName << "templog.csv"; 
			

			filename << "eeglog-" << GetTimeStr() << ".csv"; // EEG log filename		
			std::ofstream ofs(filename.str(), std::ios::trunc);
			ofs << header << std::endl;

			//filename2 << "preprocessed-log-" << GetTimeStr() << ".csv"; // EEG log filename		
			filename2 << "preprocessed-log.csv"; // COMMON EEG log filename	
			std::ofstream ofs2(filename2.str(), std::ios::trunc);

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
								int channel = targetChannelList[i];
								EE_DataGet(hData, targetChannelList[i], data, nSamplesTaken);
								ofs << data[sampleIdx] << ",";
								if (channel == ED_P7 || channel == ED_O1 || channel == ED_O2 || channel == ED_AF4){
									ofs2 << data[sampleIdx] << ",";
								}
							}
							ofs << marker << std::endl;
							ofs2 << marker << std::endl;
							marker = 0;
						}
						delete[] data;
					}
				}
				Sleep(1);
			}

			ofs.close();
			ofs2.close();
			EE_DataFree(hData);
			/*char* sourceDir = "C:\\Users\\Tharinda\\Documents\\Visual Studio 2013\\Projects\\EmotivStim\\Debug\\";
			strcat(sourceDir, "preprocessed-log.csv");
			char* destDir = "C:\\Users\\Tharinda\\Documents\\MATLAB\\eeglab14_1_1b\\eeg-common\\";
			strcat(destDir, "preprocessed-log.csv");
			copyFile(sourceDir, destDir);*/

			
			/*srcPath << source.str() << filename2.str();
			LPCWSTR src = (LPCWSTR)srcPath.str().c_str();
			destPath << destination.str() << destName.str();
			LPCWSTR dest = (LPCWSTR)destPath.str().c_str();
			CopyFile(src, dest, TRUE);*/

			
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
	std::cout << std::endl << std::endl << "Data collection ended" << std::endl;
	std::cout << "Press any key to continue" << std::endl;
}

void P300Classifier_Integrate() {
	std::ifstream infile1("C:\\Users\\Tharinda\\Documents\\MATLAB\\eeglab14_1_1b\\eeg-common\\mat-to-cpp\\epochs-1-ar.avg");
	std::ifstream infile2("C:\\Users\\Tharinda\\Documents\\MATLAB\\eeglab14_1_1b\\eeg-common\\mat-to-cpp\\epochs-2-ar.avg");
	std::ifstream infile3("C:\\Users\\Tharinda\\Documents\\MATLAB\\eeglab14_1_1b\\eeg-common\\mat-to-cpp\\epochs-3-ar.avg");
	std::ifstream infile4("C:\\Users\\Tharinda\\Documents\\MATLAB\\eeglab14_1_1b\\eeg-common\\mat-to-cpp\\epochs-4-ar.avg");

	int count = 0;

	std::string header;
	double a, b, c, d, e, avg;

	printf("\nScanning File epochs-1 \n");
	double total1 = 0;
	std::getline(infile1, header);
	while (infile1 >> a >> b >> c >> d >> e)	{
		if (count >= WINDOW_OFFSET && count <= WINDOW_OFFSET + WINDOW_SIZE){
			avg = (c + d + e) / 3;
			total1 += avg;
			printf("time: %f avg: %f\n", a, avg);
		}
		count++;
	}
	count = 0;

	printf("\nScanning File epochs-2 \n");
	double total2 = 0;
	std::getline(infile2, header);
	while (infile2 >> a >> b >> c >> d >> e)	{
		if (count >= WINDOW_OFFSET && count <= WINDOW_OFFSET + WINDOW_SIZE){
			avg = (c + d + e) / 3;
			total2 += avg;
			printf("time: %f avg: %f\n", a, avg);
		}
		count++;
	}
	count = 0;

	printf("\nScanning File epochs-3 \n");
	double total3 = 0;
	std::getline(infile3, header);
	while (infile3 >> a >> b >> c >> d >> e)	{
		if (count >= WINDOW_OFFSET && count <= WINDOW_OFFSET + WINDOW_SIZE){
			avg = (c + d + e) / 3;
			total3 += avg;
			printf("time: %f avg: %f\n", a, avg);
		}
		count++;
	}
	count = 0;

	printf("\nScanning File epochs-4 \n");
	double total4 = 0;
	std::getline(infile4, header);
	while (infile4 >> a >> b >> c >> d >> e)	{
		if (count >= WINDOW_OFFSET && count <= WINDOW_OFFSET + WINDOW_SIZE){
			avg = (b + c + d) / 3;
			//avg = b;
			total4 += avg;
			printf("time: %f avg: %f\n", a, avg);
		}
		count++;
	}
	count = 0;

	infile1.close();
	infile2.close();
	infile3.close();
	infile4.close();

	printf("\n\nScan Ended\nTotal 1: %f\nTotal 2: %f\nTotal 3: %f\nTotal 4: %f\n ", total1, total2, total3, total4);
	double totals[4] = { total1, total2, total3, total4 };
	std::sort(totals, totals + 4);

	int choice = 0;
	double total = totals[0];
	if (total == total1){
		choice = 1;
		printf("\n MARKER 1 IS THE CHOICE \n");
	}
	else if (total == total2){
		choice = 2;
		printf("\n MARKER 2 IS THE CHOICE \n");
	}
	else if (total == total3){
		choice = 3;
		printf("\n MARKER 3 IS THE CHOICE \n");
	}
	else if (total == total4){
		choice = 4;
		printf("\n MARKER 4 IS THE CHOICE \n");
	}


	getchar();
}

void P300Classifier_PeakDiff() {
	std::ifstream infile1("C:\\Users\\Tharinda\\Documents\\MATLAB\\eeglab14_1_1b\\eeg-common\\mat-to-cpp\\epochs-1-ar.avg");
	std::ifstream infile2("C:\\Users\\Tharinda\\Documents\\MATLAB\\eeglab14_1_1b\\eeg-common\\mat-to-cpp\\epochs-2-ar.avg");
	std::ifstream infile3("C:\\Users\\Tharinda\\Documents\\MATLAB\\eeglab14_1_1b\\eeg-common\\mat-to-cpp\\epochs-3-ar.avg");
	std::ifstream infile4("C:\\Users\\Tharinda\\Documents\\MATLAB\\eeglab14_1_1b\\eeg-common\\mat-to-cpp\\epochs-4-ar.avg");

	int count = 0;

	std::string header;
	double a, b, c, d, e, avg;

	printf("\nScanning File epochs-1 \n");
	double diff1 = 0;
	std::getline(infile1, header);
	while (infile1 >> a >> b >> c >> d >> e)	{
		if (count >= WINDOW_OFFSET && count <= WINDOW_OFFSET + WINDOW_SIZE){
			avg = (c + d + e) / 3;
			if (avg < diff1){
				diff1 = avg;
			}
			printf("time: %f avg: %f\n", a, avg);
		}
		count++;
	}
	count = 0;

	printf("\nScanning File epochs-2 \n");
	double diff2 = 0;
	std::getline(infile2, header);
	while (infile2 >> a >> b >> c >> d >> e)	{
		if (count >= WINDOW_OFFSET && count <= WINDOW_OFFSET + WINDOW_SIZE){
			avg = (c + d + e) / 3;
			if (avg < diff2){
				diff2 = avg;
			}
			printf("time: %f avg: %f\n", a, avg);
		}
		count++;
	}
	count = 0;

	printf("\nScanning File epochs-3 \n");
	double diff3 = 0;
	std::getline(infile3, header);
	while (infile3 >> a >> b >> c >> d >> e)	{
		if (count >= WINDOW_OFFSET && count <= WINDOW_OFFSET + WINDOW_SIZE){
			avg = (c + d + e) / 3;
			if (avg < diff3){
				diff3 = avg;
			}
			printf("time: %f avg: %f\n", a, avg);
		}
		count++;
	}
	count = 0;

	printf("\nScanning File epochs-4 \n");
	double diff4 = 0;
	std::getline(infile4, header);
	while (infile4 >> a >> b >> c >> d >> e)	{
		if (count >= WINDOW_OFFSET && count <= WINDOW_OFFSET + WINDOW_SIZE){
			avg = (b + c + d) / 3;
			//avg = b;
			if (avg < diff4){
				diff4 = avg;
			}
			printf("time: %f avg: %f\n", a, avg);
		}
		count++;
	}
	count = 0;

	infile1.close();
	infile2.close();
	infile3.close();
	infile4.close();

	printf("\n\nScan Ended\ndiff 1: %f\ndiff 2: %f\ndiff 3: %f\ndiff 4: %f\n ", diff1, diff2, diff3, diff4);
	double diffs[4] = { diff1, diff2, diff3, diff4 };
	std::sort(diffs, diffs + 4);

	int choice = 0;
	double diff = diffs[0];
	if (diff == diff1){
		choice = 1;
		printf("\n MARKER 1 IS THE CHOICE \n");
	}
	else if (diff == diff2){
		choice = 2;
		printf("\n MARKER 2 IS THE CHOICE \n");
	}
	else if (diff == diff3){
		choice = 3;
		printf("\n MARKER 3 IS THE CHOICE \n");
	}
	else if (diff == diff4){
		choice = 4;
		printf("\n MARKER 4 IS THE CHOICE \n");
	}


	getchar();
}



int main(int argc, char *argv[]) {
	//int _tmain(int argc, _TCHAR* argv[]) {
	std::cout << "EmotivStim v1.0 2018 - Emotiv EPOC EEG Data Logger for Light Flash Based P300 Experiments++" << std::endl;
	std::cout << "This software has used external libraries such as edk.lib" << std::endl;
	std::cout << "*****************************************************************" << std::endl;
	std::cout << "Make sure to connect the EPOC headset and to connect the Arduino..." << std::endl;
	std::cout << "*****************************************************************" << std::endl;
	std::cout << "Press any key to start running (press again to stop)..." << std::endl;
	getchar();

	HANDLE DCThread = (HANDLE)_beginthread(EmotivDataCollector, 0, NULL);
	HANDLE SGThread = (HANDLE)_beginthread(StimulusGenerator, 0, NULL);
	
	WaitForSingleObject(DCThread, INFINITE);
	WaitForSingleObject(SGThread, INFINITE);
	//getchar(); // or std::cin.get();

	running = false;
	
	Sleep(100);
	std::cout << "Sending data to MATLAB..." << std::endl;
	//File copying code
	LPCWSTR dest = L"C:/Users/Tharinda/Documents/MATLAB/eeglab14_1_1b/eeg-common/cpp-to-mat/templog.csv";
	LPCWSTR src = L"C:/Users/Tharinda/Documents/Visual Studio 2013/Projects/EmotivStim/Debug/preprocessed-log.csv";
	CopyFile(src, dest, TRUE);
	std::cout << "Waiting for response from MATLAB" << std::endl;
	//getchar();

	LPCWSTR lookupFile = L"C:\\Users\\Tharinda\\Documents\\MATLAB\\eeglab14_1_1b\\eeg-common\\mat-to-cpp\\epochs-4-ar.avg";
	while (true)
	{
		GetFileAttributes(lookupFile); // from winbase.h
		if (INVALID_FILE_ATTRIBUTES == GetFileAttributes(lookupFile) && GetLastError() == ERROR_FILE_NOT_FOUND)
		{
			Sleep(100);
		}
		else{
			std::cout << "Got response from MATLAB" << std::endl;
			Sleep(1000);
			break;
		}
	}

	P300Classifier_PeakDiff();

	std::cout << "*****************************************************************" << std::endl;
	std::cout << "Press any key to exit..." << std::endl;
	getchar();
	return 0;
}