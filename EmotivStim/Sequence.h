#include <stdio.h>
#include <iterator>

#include <iostream>     // std::cout
#include <algorithm>    // std::shuffle
#include <array>        // std::array
#include <random>       // std::default_random_engine
#include <chrono>       // std::chrono::system_clock

class Sequence {
	int length, markers, gap, filled;
	int seqArray[1000];
	const int T_LIMIT = 40; // max possible shuffling

public:

	Sequence(const int seqLength, int numTargets, int noRepeat){
		generate(seqLength, numTargets, noRepeat);
	}

	int generate(const int seqLength, int numTargets, int noRepeat){
		if (seqLength > 1000 || seqLength % numTargets != 0 || numTargets > seqLength){
			printf("\nERROR! Sequence: Wrong initializer values\n");
			return 0;
		}
		filled = ((seqLength / T_LIMIT) + 1)*T_LIMIT;
		markers = numTargets;
		gap = noRepeat;
		length = seqLength;

		int start = 0;
		int end = T_LIMIT;

		printf("\n--------------------------Generating Sequence-------------------------\n");
		printf("length : %d\t filled : %d\t\n", length, filled);

		int seq[1000];
		memcpy(seq, seqArray, 1000); //Copy the permanent seq array to temporary one (useless step)


		int prev = 0, prev2 = 0;
		int k = 0;
		while (k < (filled / T_LIMIT)){
			bool clean = false;
			start = k*T_LIMIT;
			if (k + 1 == (filled / T_LIMIT)){ end = seqLength; }
			else{ end = (k + 1)*T_LIMIT; } //different end for last one
			printf("k : %d\t start : %d\t end : %d\t > Shuffling...\n", k, start, end - 1);

			int crange = (end - start) / numTargets;

			int a = start;
			while (a < end){ // filling array 1* 2* 3* 4*
				seq[a] = ((a - start) / crange) + 1;
				a++;
			}

			unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
			shuffle(&seq[start], &seq[end], std::default_random_engine(seed));

			while (!clean){
				unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
				shuffle(&seq[start], &seq[end], std::default_random_engine(seed));

				clean = true;
				if (k == 0){ prev = 0; }
				else { prev = prev2; } //different prev for first one

				int i = start;
				while (i < end){
					int marker = seq[i];
					if (prev == marker){ clean = false; break; }
					prev = marker;
					i++;
				}
			}
			prev2 = seq[end - 1];
			printf("prev2 : %d\t\n", prev2);

			k++;
		}


		printf("------------------Sequence Generation Completed!----------------------\n");

		/*for (int i = 0; i < length; i++){
			printf("%d\t%d\n", i + 1, seq[i]);
		}

		printf("\n-------------------------------------------------------------\n");*/

		memcpy(seqArray, seq, 1000); //Copy the temporarily generated seq array to permanent one
		return 1;
	}

	int get(int i){
		if (i < length){
			return seqArray[i];
		}
		else{
			return -1;
		}		
	}
};