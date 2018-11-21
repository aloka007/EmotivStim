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
#include <conio.h>
#include <iomanip>
#include <time.h>
#include <string.h>
#include <string>
#include <cstdlib>
#include <algorithm>
#include <math.h>
#include <ctime>
#include <process.h> // _beginthread(f,stacksize,*arglist)
#include "EmoStateDLL.h"
#include "edk.h"
#include "edkErrorCode.h"
#include "SerialPort.h"
#include "enceph.h"
#include "SendKeys.h"
#include "Sequence.h"

#define DATA_LENGTH 1

//#define TRIALS_PER_SERIES 120
//#define RANDOM_NUMS 4
//#define INTERFLASH_INTERVAL 300

//Classifier Parameters - Integration
//#define WINDOW_OFFSET 155
//#define WINDOW_SIZE 8

//Classifier Parameters - Peak Comparison
//#define WINDOW_OFFSET 140
//#define WINDOW_SIZE 38

int TRIALS_PER_SERIES = 100;
int RANDOM_NUMS = 4;
int INTERFLASH_INTERVAL = 500;

//Classifier Parameters - Peak Comparison
int WINDOW_OFFSET = 140;
int WINDOW_SIZE = 38;

char* portName = "\\\\.\\COM6";  //Port of Arduino Mega - stimulus generator
char* controllerPort = "\\\\.\\COM15"; //Port of  Arduino Uno - appliance controller

char incomingData[DATA_LENGTH];

char t[] = "1";
char nt[] = "2";
int loopCount = 0;
int trialCount = 0;
int seriesCount = 0;

SerialPort *arduino;
SerialPort *controller;
//Sequence *sequence;

int randomNum = 0;

bool running = true; /* flag used to stop the program execution */
bool aborted = true; //flag used to detect if the trial was aborted
bool standby = true; //flag used to wait for a trial start

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

void ApplianceController(SerialPort *port, int command){
	char sendbuffer[] = " ";
	
	strcpy(sendbuffer, std::to_string(command).c_str());

	int readResult = port->readSerialPort(incomingData, DATA_LENGTH);
	readResult = port->readSerialPort(incomingData, DATA_LENGTH); //Clear incoming buffer
	readResult = port->readSerialPort(incomingData, DATA_LENGTH);

	port->writeSerialPort(sendbuffer, 2);  //Send command to arduino
	std::cout << "Command # " << command << " sent to controller" << std::endl << std::endl;
	
}

void StimulusGenerator(void *unused)
{
	int seriesCount = 0;
	int loopCount = 0;
	int trialCount = 0;
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

			if (seriesCount == 1){
				aborted = false;
				running = false;
				break;
			}

			seriesCount++;
			std::cout << std::endl << std::endl << "\t\t\t\t\t\tBeginning Trial Series: " << seriesCount << std::endl << std::endl;
			arduino->writeSerialPort("c\0", DATA_LENGTH);
			arduino->readBlockingSerialPort(incomingData, DATA_LENGTH);
			Sleep(3000); //inter-series delay
			arduino->writeSerialPort("d\0", DATA_LENGTH);
			arduino->readBlockingSerialPort(incomingData, DATA_LENGTH);
			Sleep(500);
		}

		randomNum =  (rand() % RANDOM_NUMS) + 1;  //random number
		//randomNum = (int) sequence->get(trialCount);                 // GET RECORDED RANDOM NUM
		

		strcpy(sendbuf, std::to_string(randomNum).c_str());
		begin = clock();
		arduino->writeSerialPort(sendbuf, DATA_LENGTH);  //Send random number to Arduino
		marker = randomNum;
		//THE WAIT!!!
		int readResult = arduino->readBlockingSerialPort(incomingData, DATA_LENGTH); //Receive reply from Arduino
		end = clock();

		double elapsed_secs = double(end - begin) / (CLOCKS_PER_SEC / 1000);
		printf("\t\t\t\t\t\tSent-Received: %s-%c\t delay: %fms Trial Count: %d Targets (Est): %d\n", sendbuf, incomingData[0], elapsed_secs, trialCount, trialCount / RANDOM_NUMS);

		if (sendbuf[0] != incomingData[0]){ //Compare the sent and received markers and stop if they are not equal
			printf("SERIAL COM OUT OF SYNC!\nRESYNCING...\n\n");

			readResult = arduino->readSerialPort(incomingData, DATA_LENGTH); //Clear incoming buffer
			readResult = arduino->readSerialPort(incomingData, DATA_LENGTH);
			readResult = arduino->readSerialPort(incomingData, DATA_LENGTH);

			//running = false;
		}

		incomingData[0] = '\0';

		trialCount++;
		loopCount++;
		Sleep(INTERFLASH_INTERVAL);
	}
	arduino->writeSerialPort("a\0", DATA_LENGTH);
	arduino->readBlockingSerialPort(incomingData, DATA_LENGTH);
	arduino->writeSerialPort("c\0", DATA_LENGTH);
	arduino->readBlockingSerialPort(incomingData, DATA_LENGTH);
	delete arduino;
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

			//std::cout.setstate(std::ios_base::failbit);
			//std::streambuf* cout_sbuf = std::cout.rdbuf(); // save original sbuf
			//std::ofstream   fout("/dev/null");
			//std::cout.rdbuf(fout.rdbuf()); // redirect 'cout' to a 'fout'

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

			//std::cout.rdbuf(cout_sbuf); // restore the original stream buffer
			//std::cout.clear();

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
	if (!aborted){
		// ENTER key down ******************************************************SIMULATED KEYPRESS
		keybd_event(VK_RETURN, 0x9C, 0, 0);
		// ENTER key up
		keybd_event(VK_RETURN, 0x9C, 0, 0);
	}
}

void BlinkTrigger(){
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
			DataHandle hData = EE_DataCreate();
			EE_DataSetBufferSizeInSec(secs);

			std::cout << "EEG buffer size in secs:" << secs << std::endl;
			int samplecount = 0;
			double sampleTotal = 0;
			int smoothing = 5;
			while (standby) {

				
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

				}

				if (collectEEG) {
					EE_DataUpdateHandle(0, hData);

					unsigned int nSamplesTaken = 0;
					EE_DataGetNumberOfSample(hData, &nSamplesTaken);

					if (nSamplesTaken != 0) {

						double* data = new double[nSamplesTaken];
						double plotval = 0;
						for (int sampleIdx = 0; sampleIdx<(int)nSamplesTaken; ++sampleIdx) {
							for (int i = 0; i<sizeof(targetChannelList) / sizeof(EE_DataChannel_t); i++) {
								int channel = targetChannelList[i];
								EE_DataGet(hData, targetChannelList[i], data, nSamplesTaken);

								char graph[120] = "                                                                                                                ";

								if ( channel == ED_P7){
									plotval = data[sampleIdx];
									sampleTotal += plotval / smoothing;

									if (samplecount % smoothing == 0){
										int lineindex = 25 + ((int)sampleTotal - 4000) / 5;
										if (lineindex > 0 && lineindex < 100){
											graph[lineindex] = 'X';
										}
										
										std::cout << ">" << graph << ">" << sampleTotal << std::endl;
										sampleTotal = 0;
									}
									
								}
							}
							samplecount++;
						}
						delete[] data;
					}
				}
				
				Sleep(1);
			}
			EE_DataFree(hData);
		}

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
			avg = (b + c + d) / 3;
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
			avg = (b + c + d) / 3;
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
			avg = (b + c + d) / 3;
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

	printf("\n\nScan Ended\nTotal 1: %f\nTotal 2: %f\nTotal 3: %f\nTotal 4: %f\n", total1, total2, total3, total4);
	double totals[4] = { total1, total2, total3, total4 };
	std::sort(totals, totals + 4);

	int choice = 0;
	double total = totals[0];
	std::cout << "Decision of Classifier: -------- ";
	if (total == total1){
		choice = 1;
		printf("MARKER 1\n");
	}
	else if (total == total2){
		choice = 2;
		printf("MARKER 2\n");
	}
	else if (total == total3){
		choice = 3;
		printf("MARKER 3\n");
	}
	else if (total == total4){
		choice = 4;
		printf("MARKER 4\n");
	}

	std::cout << "Confidence: " << ((total - ((totals[1] < 0) ? totals[1] : 0)) / total) * 100 << "% " << std::endl;
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

void P300Classifier_WeightGauss(){
	std::ifstream infile1("C:\\Users\\Tharinda\\Documents\\MATLAB\\eeglab14_1_1b\\eeg-common\\mat-to-cpp\\epochs-1-ar.avg");
	std::ifstream infile2("C:\\Users\\Tharinda\\Documents\\MATLAB\\eeglab14_1_1b\\eeg-common\\mat-to-cpp\\epochs-2-ar.avg");
	std::ifstream infile3("C:\\Users\\Tharinda\\Documents\\MATLAB\\eeglab14_1_1b\\eeg-common\\mat-to-cpp\\epochs-3-ar.avg");
	std::ifstream infile4("C:\\Users\\Tharinda\\Documents\\MATLAB\\eeglab14_1_1b\\eeg-common\\mat-to-cpp\\epochs-4-ar.avg");

	int count = 0;

	std::string header;
	double a, b, c, d, e;

	double buffer1[128][8], buffer2[128][8], buffer3[128][8], buffer4[128][8];



	printf("\nScanning File epochs-1 \n");
	std::getline(infile1, header);
	while (infile1 >> a >> b >> c >> d >> e)	{
		if (count >= 128 && count < 256){
			buffer1[count - 128][0] = a; 
			buffer1[count - 128][1] = b;
			buffer1[count - 128][2] = c;
			buffer1[count - 128][3] = d;
			buffer1[count - 128][4] = e;
			
			printf("time: %f AF4: %f P7: %f O1: %f O2: %f \n", a, b, c, d, e);
		}
		count++;
	}
	count = 0;

	printf("\nScanning File epochs-1 \n");
	std::getline(infile2, header);
	while (infile2 >> a >> b >> c >> d >> e)	{
		if (count >= 128 && count < 256){
			buffer2[count - 128][0] = a;
			buffer2[count - 128][1] = b;
			buffer2[count - 128][2] = c;
			buffer2[count - 128][3] = d;
			buffer2[count - 128][4] = e;

			printf("time: %f AF4: %f P7: %f O1: %f O2: %f \n", a, b, c, d, e);
		}
		count++;
	}
	count = 0;

	printf("\nScanning File epochs-1 \n");
	std::getline(infile3, header);
	while (infile3 >> a >> b >> c >> d >> e)	{
		if (count >= 128 && count < 256){
			buffer3[count - 128][0] = a;
			buffer3[count - 128][1] = b;
			buffer3[count - 128][2] = c;
			buffer3[count - 128][3] = d;
			buffer3[count - 128][4] = e;

			printf("time: %f AF4: %f P7: %f O1: %f O2: %f \n", a, b, c, d, e);
		}
		count++;
	}
	count = 0;

	printf("\nScanning File epochs-1 \n");
	std::getline(infile4, header);
	while (infile4 >> a >> b >> c >> d >> e)	{
		if (count >= 128 && count < 256){
			buffer4[count - 128][0] = a;
			buffer4[count - 128][1] = b;
			buffer4[count - 128][2] = c;
			buffer4[count - 128][3] = d;
			buffer4[count - 128][4] = e;

			printf("time: %f AF4: %f P7: %f O1: %f O2: %f \n", a, b, c, d, e);
		}
		count++;
	}
	count = 0;

	infile1.close();
	infile2.close();
	infile3.close();
	infile4.close();

	static double peaks[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

	double total1 = 0, total2 = 0, total3 = 0, total4 = 0;

	double peakPos = 250.0;

	for (int i = 0; i < 128; i++){
		double v = gaussian(buffer1[i][0], peakPos) * weighting(buffer1[i][1], buffer1[i][2], buffer1[i][3], buffer1[i][4]);
		buffer1[i][5] = v;
		total1 += (-v);
	}
	for (int i = 0; i < 128; i++){
		double v = gaussian(buffer2[i][0], peakPos) * weighting(buffer2[i][1], buffer2[i][2], buffer2[i][3], buffer2[i][4]);
		buffer2[i][5] = v;
		total2 += (-v);
	}
	for (int i = 0; i < 128; i++){
		double v = gaussian(buffer3[i][0], peakPos) * weighting(buffer3[i][1], buffer3[i][2], buffer3[i][3], buffer3[i][4]);
		buffer3[i][5] = v;
		total3 += (-v);
	}
	for (int i = 0; i < 128; i++){
		double v = gaussian(buffer4[i][0], peakPos) * weighting(buffer4[i][1], buffer4[i][2], buffer4[i][3], buffer4[i][4]);
		buffer4[i][5] = v;
		total4 += (-v);
	}


	printf("\n\nScan Ended\nTotal 1: %f\nTotal 2: %f\nTotal 3: %f\nTotal 4: %f\n", total1, total2, total3, total4);
	double totals[4] = { total1, total2, total3, total4 };
	std::sort(totals, totals + 4);

	int choice = 0;
	double total = totals[0];
	std::cout << "Decision of Classifier: -------- ";
	if (total == total1){
		choice = 1;
		printf("MARKER 1\n");
	}
	else if (total == total2){
		choice = 2;
		printf("MARKER 2\n");
	}
	else if (total == total3){
		choice = 3;
		printf("MARKER 3\n");
	}
	else if (total == total4){
		choice = 4;
		printf("MARKER 4\n");
	}

	std::cout << "Confidence: " << ((total - ((totals[1] < 0) ? totals[1] : 0)) / total) * 100 << "% " << std::endl;
}

template<size_t R, size_t C>
int P300Classifier_AdaptWeightGauss(double center, double radius,int experiment, char mode, int target, double (&history)[R][C]){
	std::cout << "Model based - Adaptive Hybrid Classifier With Gaussian Filtering and Weighted Channel Ensembling" << std::endl;

	int TARGETS = 4; //hardcoded

	double SEARCH_START, SEARCH_END, SEARCH_RADIUS = radius, SEARCH_CENTER = center; // start from around 300
	SEARCH_START = SEARCH_CENTER - SEARCH_RADIUS;
	SEARCH_END = SEARCH_CENTER + SEARCH_RADIUS;
	std::cout << "\t > Search Start > " << SEARCH_START << "\t < Search End > " << SEARCH_END << std::endl;

	//Getting file streams
	std::ifstream infile1("C:\\Users\\Tharinda\\Documents\\MATLAB\\eeglab14_1_1b\\eeg-common\\mat-to-cpp\\epochs-1-ar.avg");
	std::ifstream infile2("C:\\Users\\Tharinda\\Documents\\MATLAB\\eeglab14_1_1b\\eeg-common\\mat-to-cpp\\epochs-2-ar.avg");
	std::ifstream infile3("C:\\Users\\Tharinda\\Documents\\MATLAB\\eeglab14_1_1b\\eeg-common\\mat-to-cpp\\epochs-3-ar.avg");
	std::ifstream infile4("C:\\Users\\Tharinda\\Documents\\MATLAB\\eeglab14_1_1b\\eeg-common\\mat-to-cpp\\epochs-4-ar.avg");

	int count = 0;

	std::string header;
	double a, b, c, d, e;

	double buffer1[128][8], buffer2[128][8], buffer3[128][8], buffer4[128][8];

	//Scanning files and loading into buffers
	printf("\nScanning File epochs-1 \n");
	std::getline(infile1, header);
	while (infile1 >> a >> b >> c >> d >> e)	{
		if (count >= 128 && count < 256){
			buffer1[count - 128][0] = a;
			buffer1[count - 128][1] = b;
			buffer1[count - 128][2] = c;
			buffer1[count - 128][3] = d;
			buffer1[count - 128][4] = e;

			//printf("time: %f AF4: %f P7: %f O1: %f O2: %f \n", a, b, c, d, e);
		}
		count++;
	}
	count = 0;

	printf("\nScanning File epochs-2 \n");
	std::getline(infile2, header);
	while (infile2 >> a >> b >> c >> d >> e)	{
		if (count >= 128 && count < 256){
			buffer2[count - 128][0] = a;
			buffer2[count - 128][1] = b;
			buffer2[count - 128][2] = c;
			buffer2[count - 128][3] = d;
			buffer2[count - 128][4] = e;

			//printf("time: %f AF4: %f P7: %f O1: %f O2: %f \n", a, b, c, d, e);
		}
		count++;
	}
	count = 0;

	printf("\nScanning File epochs-3 \n");
	std::getline(infile3, header);
	while (infile3 >> a >> b >> c >> d >> e)	{
		if (count >= 128 && count < 256){
			buffer3[count - 128][0] = a;
			buffer3[count - 128][1] = b;
			buffer3[count - 128][2] = c;
			buffer3[count - 128][3] = d;
			buffer3[count - 128][4] = e;

			//printf("time: %f AF4: %f P7: %f O1: %f O2: %f \n", a, b, c, d, e);
		}
		count++;
	}
	count = 0;

	printf("\nScanning File epochs-4 \n");
	std::getline(infile4, header);
	while (infile4 >> a >> b >> c >> d >> e)	{
		if (count >= 128 && count < 256){
			buffer4[count - 128][0] = a;
			buffer4[count - 128][1] = b;
			buffer4[count - 128][2] = c;
			buffer4[count - 128][3] = d;
			buffer4[count - 128][4] = e;

			//printf("time: %f AF4: %f P7: %f O1: %f O2: %f \n", a, b, c, d, e);
		}
		count++;
	}
	count = 0;

	//Closing file streams
	infile1.close();
	infile2.close();
	infile3.close();
	infile4.close();

	double total1 = 0, total2 = 0, total3 = 0, total4 = 0;

	static double peaks[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

	double peakPos1 = 250.0, peakPos2 = 250.0, peakPos3 = 250.0, peakPos4 = 250.0 ;

	double peak1 = 0, peak2 = 0, peak3 = 0, peak4 = 0;

	//Peak picking: This gives the negative peak value and its position in P7 channel
	//only the peaks between search start and search end are considered
	for (int i = 0; i < 128; i++){
		double v = buffer1[i][2];
		if (buffer1[i][0] >= SEARCH_START && buffer1[i][0] <= SEARCH_END && v < peak1){
			peak1 = v;
			peakPos1 = buffer1[i][0];
		}
		
	}

	for (int i = 0; i < 128; i++){
		double v = buffer2[i][2];
		if (buffer1[i][0] >= SEARCH_START && buffer1[i][0] <= SEARCH_END && v < peak2){
			peak2 = v;
			peakPos2 = buffer2[i][0];
		}
	}

	for (int i = 0; i < 128; i++){
		double v = buffer3[i][2];
		if (buffer1[i][0] >= SEARCH_START && buffer1[i][0] <= SEARCH_END && v < peak3){
			peak3 = v;
			peakPos3 = buffer3[i][0];
		}
	}

	for (int i = 0; i < 128; i++){
		double v = buffer4[i][2];
		if (buffer1[i][0] >= SEARCH_START && buffer1[i][0] <= SEARCH_END && v < peak4){
			peak4 = v;
			peakPos4 = buffer4[i][0];
		}
	}

	if (mode == 'A' && experiment < 10){
		switch (target){
		case 1 : SEARCH_CENTER = peakPos1; break;
		case 2 : SEARCH_CENTER = peakPos2; break;
		case 3 : SEARCH_CENTER = peakPos3; break;
		case 4 : SEARCH_CENTER = peakPos4; break;
		default: break;
		}
		SEARCH_RADIUS -= 20;
		
	}

	// Wave analysis
	for (int i = 0; i < 128; i++){
		double v = gaussian(buffer1[i][0], peakPos1) * weighting(buffer1[i][1], buffer1[i][2], buffer1[i][3], buffer1[i][4]);
		buffer1[i][5] = v;
		total1 += (-v);
	}
	for (int i = 0; i < 128; i++){
		double v = gaussian(buffer2[i][0], peakPos2) * weighting(buffer2[i][1], buffer2[i][2], buffer2[i][3], buffer2[i][4]);
		buffer2[i][5] = v;
		total2 += (-v);
	}
	for (int i = 0; i < 128; i++){
		double v = gaussian(buffer3[i][0], peakPos3) * weighting(buffer3[i][1], buffer3[i][2], buffer3[i][3], buffer3[i][4]);
		buffer3[i][5] = v;
		total3 += (-v);
	}
	for (int i = 0; i < 128; i++){
		double v = gaussian(buffer4[i][0], peakPos4) * weighting(buffer4[i][1], buffer4[i][2], buffer4[i][3], buffer4[i][4]);
		buffer4[i][5] = v;
		total4 += (-v);
	}


	printf("\n\nScan Ended\nTotal 1: %f\nTotal 2: %f\nTotal 3: %f\nTotal 4: %f\n", total1, total2, total3, total4);
	double totals[4] = { total1, total2, total3, total4 };
	std::sort(totals, totals + 4);

	int choice = 0;
	double total = totals[0];
	std::cout << "Decision of Classifier: -------- ";
	if (total == total1){
		choice = 1;
		history[experiment][4] = peakPos1;
		printf("MARKER 1\n");
	}
	else if (total == total2){
		choice = 2;
		history[experiment][4] = peakPos2;
		printf("MARKER 2\n");
	}
	else if (total == total3){
		choice = 3;
		history[experiment][4] = peakPos3;
		printf("MARKER 3\n");
	}
	else if (total == total4){
		choice = 4;
		history[experiment][4] = peakPos4;
		printf("MARKER 4\n");
	}

	double confidence = ((total - ((totals[1] < 0) ? totals[1] : 0)) / total) * 100;

	history[experiment][3] = confidence;

	std::cout << "Confidence: " << confidence  << "% " << std::endl;
	return choice;
}


int main(int argc, char *argv[]) {
	//int _tmain(int argc, _TCHAR* argv[]) {
	const int total_experiments = 100;             //increase this!
	int experiment = 1;
	bool testing = true;
	char mode = 'B';
	int target = 0;

	int correct = 0;
	int wrong = 0;

	double search_center = 250.0; //parameters for the classifier
	double search_radius = 10.0;

	double history[total_experiments][10]; //Experiment history = {correct/wrong - 1/0 , target , result , confidence , peakPos}

	std::cout << "EncephLink v3.0 2018 - Emotiv EPOC EEG Data Logger | Stimulus generator | Appliance controller" << std::endl;
	std::cout << "This software has used external libraries such as edk.lib" << std::endl;
	std::cout << "*****************************************************************" << std::endl;
	std::cout << "Make sure to connect the EPOC headset and to connect the Arduino" << std::endl << std::endl;

	controller = new SerialPort(controllerPort);

	if (controller->isConnected()){
		std::cout << "Connected to controller at port " << controllerPort << std::endl << std::endl;
	}

	//sequence = new Sequence(TRIALS_PER_SERIES, RANDOM_NUMS, 1); // Sequence generator

	while (testing && experiment < total_experiments){
		std::cout << std::endl << std::endl;

		std::cout << "~~~~~~----------------------~~HISTORY~~-----------------------~~~~~~" << std::endl;
		std::cout << "#\tC/W/A \tTarget \tResult \tConf \tPeak \tFlash \tISI \tTime" << std::endl;
		
		
		for (int i = 1; i < experiment; i++){
			std::fixed;
			std::cout << "#" << i << "\t" << history[i][0] << "\t" << history[i][1] << "\t" << history[i][2]
				<< "\t" << std::setprecision(4) << history[i][3] << "%\t" << std::setprecision(5) << history[i][4] << "\t" << history[i][5] << "\t"
				<< history[i][6] << "\t" << history[i][7] << "\t"
				<< std::endl;
		}

		std::cout << std::endl << std::endl;

		std::cout << "******************************************************************" << std::endl;
		std::cout << "                          EXPERIMENT #" << experiment << std::endl;
		std::cout << "******************************************************************" << std::endl << std::endl;

		std::cout << "Current experiment parameters are as follows..." << std::endl << std::endl;

		std::cout << "\tRunning Mode:--------------------- " << mode << std::endl;
		std::cout << "\tTrials Per Series:---------------- " << TRIALS_PER_SERIES << std::endl;
		std::cout << "\tRandom Numbers:------------------- " << RANDOM_NUMS << std::endl;
		std::cout << "\tISI:------------------------------ " << INTERFLASH_INTERVAL << "ms" <<  std::endl;
		std::cout << "\tPeak Search Center:--------------- " << search_center << std::endl;
		std::cout << "\tPeak Search Radius:--------------- " << search_radius << std::endl << std::endl;
		/*std::cout << "\tWindow Offset (From -1000ms):----- " << WINDOW_OFFSET << " frames" << std::endl;
		std::cout << "\tWindow Size:---------------------- " << WINDOW_SIZE << " frames" << std::endl << std::endl;*/

		std::cout << "Press SPACE to continue, Press E to change values" << std::endl << std::endl;
		char key;
		key = _getch();
		if (key == 'E' || key == 'e'){
			//getchar();

			std::string input;
			std::cout << "\tRunning Mode:--------------------- ";
			std::getline(std::cin, input);
			if (!input.empty()) {
				std::istringstream stream(input);
				stream >> mode;
			}
			std::cout << std::endl;

			std::cout << "\tTrials Per Series (x of 4):------- ";
			std::getline(std::cin, input);
			if (!input.empty()) {
				std::istringstream stream(input);
				stream >> TRIALS_PER_SERIES;
				//sequence->generate(TRIALS_PER_SERIES, RANDOM_NUMS, 1); // generate a new sequence on change
			}
			std::cout << std::endl;

			std::cout << "\tISI:------------------------------ ";
			std::getline(std::cin, input);
			if (!input.empty()) {
				std::istringstream stream(input);
				stream >> INTERFLASH_INTERVAL;
			}
			std::cout << std::endl;

			std::cout << "\tPeak Search Center:--------------- ";
			std::getline(std::cin, input);
			if (!input.empty()) {
				std::istringstream stream(input);
				stream >> search_center;
			}
			std::cout << std::endl;

			std::cout << "\tPeak Search Radius:---------------- ";
			std::getline(std::cin, input);
			if (!input.empty()) {
				std::istringstream stream(input);
				stream >> search_radius;
			}
			std::cout << std::endl;

			std::cout << "Updated experiment parameters are as follows..." << std::endl << std::endl;

			std::cout << "\tRunning Mode:--------------------- " << mode << std::endl;
			std::cout << "\tTrials Per Series:---------------- " << TRIALS_PER_SERIES << std::endl;
			std::cout << "\tRandom Numbers:------------------- " << RANDOM_NUMS << std::endl;
			std::cout << "\tISI:------------------------------ " << INTERFLASH_INTERVAL << "ms" << std::endl;
			std::cout << "\tPeak Search Center:--------------- " << search_center << std::endl;
			std::cout << "\tPeak Search Radius:--------------- " << search_radius << std::endl << std::endl;
		}


		// Get expected result to compare
		if (mode == 'A' || mode == 'B'){
			std::string input;
			std::cout << "\t Target Command:--------------- ";
			std::getline(std::cin, input);
			if (!input.empty()) {
				std::istringstream stream(input);
				stream >> target;
			}
			std::cout  << std::endl;
			std::cout << "\t Expecting Target Command:----- " << target << std::endl << std::endl << std::endl;
		}

		else {
			target = 0;
		}


		int result = 0; // int to store result

		std::cout << "Press any key to start trial. Press again to abort" << std::endl;

		//BlinkTrigger(); // The blink to start function still in development

		_getch();

		HANDLE DCThread = (HANDLE)_beginthread(EmotivDataCollector, 0, NULL);
		HANDLE SGThread = (HANDLE)_beginthread(StimulusGenerator, 0, NULL);

		getchar(); // or std::cin.get(); - THIS ABORTS THE TRIAL!
		running = false;

		WaitForSingleObject(DCThread, INFINITE);
		WaitForSingleObject(SGThread, INFINITE);

		if (aborted){ //if the trial was aborted

			history[experiment][0] = 0; history[experiment][1] = 0; history[experiment][2] = 0; history[experiment][3] = 0;
			history[experiment][4] = 0; history[experiment][5] = 0; history[experiment][6] = 0; history[experiment][7] = 0;

		}

		else{ // if the trial completed normally

			history[experiment][5] = TRIALS_PER_SERIES;
			history[experiment][6] = INTERFLASH_INTERVAL;
			history[experiment][7] = TRIALS_PER_SERIES*INTERFLASH_INTERVAL/1000;

			Sleep(100);
			std::cout << "Sending data to MATLAB..." << std::endl;
			//File copying code >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
			LPCWSTR dest = L"C:/Users/Tharinda/Documents/MATLAB/eeglab14_1_1b/eeg-common/cpp-to-mat/templog.csv";
			LPCWSTR src = L"C:/Users/Tharinda/Documents/Visual Studio 2013/Projects/EmotivStim/Debug/preprocessed-log.csv";
			CopyFile(src, dest, TRUE);
			std::cout << "Waiting for response from MATLAB" << std::endl;
			//getchar(); >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

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

			
			result = P300Classifier_AdaptWeightGauss(search_center,search_radius, experiment, mode, target, history);
			//P300Classifier_Integrate();

			if (mode == 'B' | mode == 'C'){
				ApplianceController(controller, result);
			}


			if (mode == 'A' || mode == 'B'){
				std::cout << std::endl << "\t Target Command:----- " << target << std::endl;
				std::cout << "\t Result Command:----- " << result << std::endl << std::endl;
				if (target == result){
					std::cout << "\t +++ CORRECT CLASSIFICATION! +++" << std::endl;
					correct++;
					history[experiment][0] = 1;
				}
				else{
					std::cout << "\t  XXX WRONG CLASSIFICATION! XXX" << std::endl << std::endl;
					wrong++;
					history[experiment][0] = -1;
				}
				std::cout << std::endl << "\t Correct:------- " << correct << std::endl;
				std::cout << "\t Wrong:--------- " << wrong << std::endl;
				std::cout << "\t Accuracy:------ " << ((float)correct / (float)(correct + wrong)) * 100.0 << "%" << std::endl;

				history[experiment][1] = target;
				history[experiment][2] = result;

			}
		}


		
		experiment++;

		std::cout << std::endl << "Press SPACE to continue, press X to stop experiments" << std::endl;
		key = _getch();
		if (key == 'X' || key == 'x'){
			break;
		}

		aborted = true;
		running = true;
	}

	std::cout << "*****************************************************************" << std::endl;
	std::cout << "Press any key to exit..." << std::endl;
	getchar();
	return 0;
}