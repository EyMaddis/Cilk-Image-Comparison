extern "C" {
  #include "puzzle_common.h"
  #include "puzzle.h"
}

#include "pgetopt.hpp"
#include <vector>
#include <iostream>
#include "listdir.h"
#include <fstream>
#include <cilk/cilk.h>
#include "cilktime.h"

// for qsort
#include <algorithm>
//#include <iterator>
//#include <functional>

using namespace std;

const double IDENTITY_THRESHOLD = 0.12;

typedef struct Opts_ {
	const char *refImage;
    const char *dir;
    int fix_for_texts;
} Opts;

typedef struct ImageDistancePair_ {
	string fileName;
	double distance;
} ImageDistancePair;

string outputFile = "";
ofstream outputStream;

// for sorting
bool operator<(ImageDistancePair p, ImageDistancePair other){ return p.distance < other.distance; };

void usage(void)
{
    puts("\nUsage: puzzle-diff [-o <outputFile>] referenceImage directory\n");
    exit(EXIT_SUCCESS);
}

void writeOutputLine(string out){
	cout << out << endl;
	if (outputFile.length() > 0)
		outputStream << out << endl;
	
}

int parse_opts(Opts * const opts, PuzzleContext * context,
               int argc, char **argv) {
    int opt;
    extern char *poptarg;
    extern int poptind;

    opts->fix_for_texts = 1;
    while ((opt = pgetopt(argc, argv, "o:")) != -1) {
        switch (opt) {
        case 'o':
            // set output text atof(poptarg);
			outputFile = poptarg;
			outputStream.open(outputFile);
			cout << "Using output file!" << endl;
			if (!outputStream.is_open())
			{
				cout << "Cannot open output file!";
				outputFile = "";
			}
			break;
        default:
            usage();      
        }
    }
    argc -= poptind;
    argv += poptind;
    if (argc != 2) {
        usage();
    }
    opts->refImage = *argv++;
    opts->dir = *argv;
    
    return 0;
}

/* Does not sort correctly
void struct_qsort(ImageDistancePair * begin, ImageDistancePair * end)
{
	if (begin != end) {
		--end;  // Exclude last element (pivot) from partition
		ImageDistancePair * middle = partition(begin, end, bind2nd(less<ImageDistancePair>(), *end));
		using swap;
		swap(*end, *middle);    // move pivot to middle
		cilk_spawn struct_qsort(begin, middle);
		struct_qsort(++middle, ++end); // Exclude pivot and restore end
		cilk_sync;
	}
} */

void sortImages(vector<ImageDistancePair>& imagelist, vector<ImageDistancePair>& toplist){

	unsigned int vecSize = imagelist.size();
	unsigned int topSize = toplist.size();

	for (int j = 0; j < topSize; j++){
			toplist[j].distance = 2.0;			
			toplist[j].fileName = "";
		}		
		for (int i = 0; i < vecSize; i++){
			for (int j = 0; j < topSize; j++){
				if (toplist[j].distance >= imagelist[i].distance){
					if (toplist[j].distance == imagelist[i].distance){						
						break;						
					}
					swap(toplist[j], imagelist[i]);					
				}
			}
	}	
}

void sortImagesDublicate(vector<ImageDistancePair>& imagelist, vector<ImageDistancePair>& toplist){

	unsigned int vecSize = imagelist.size();
	unsigned int topSize = toplist.size();

	for (int j = 0; j < topSize; j++){
		toplist[j].distance = 2.0;
		toplist[j].fileName = "";
	}
	for (int i = 0; i < vecSize; i++){
		for (int j = 0; j < topSize; j++){
			if (toplist[j].distance > imagelist[i].distance){				
				swap(toplist[j], imagelist[i]);
			}
		}
	}
}

int main(int argc, char *argv[])
{

	long long executionStart = cilk_getticks();
    Opts opts;
    PuzzleContext context;
	PuzzleCvec cvec1;
	unsigned long long start_ticks = cilk_getticks();

    puzzle_init_context(&context);    
    parse_opts(&opts, &context, argc, argv);
	if (outputFile.length() > 0){
		cout << "Output set to " << outputFile << endl;
	}

	puzzle_init_cvec(&context, &cvec1);

	// reference file
	if (puzzle_fill_cvec_from_file(&context, &cvec1, opts.refImage) != 0) {
		fprintf(stderr, "Unable to read reference image: [%s]\n", opts.refImage);
        return 1;
    }


	std::cout << "Reference image loaded in " << (cilk_getticks() - start_ticks) << " milliseconds." << std::endl;
	vector<string> fileNamesVector;
	
	listDir(opts.dir, fileNamesVector);
	cout << "Number of file names found in search directory: " << fileNamesVector.size() << "\n\n";
	
	
	// parallel cvec distance calculation
	unsigned int files = fileNamesVector.size();
	vector<ImageDistancePair> distances(files);
	start_ticks = cilk_getticks();

	cilk_for(unsigned int i = 0; i < files; i++){
		PuzzleCvec puzzleCvec;
		double d;
		const char* fileName = fileNamesVector[i].c_str();
		ImageDistancePair pair;
		puzzle_init_cvec(&context, &puzzleCvec);
		if (puzzle_fill_cvec_from_file(&context, &puzzleCvec, fileName) == 0){
			pair.distance = puzzle_vector_normalized_distance(&context, &cvec1, &puzzleCvec, opts.fix_for_texts);
			pair.fileName = fileName;
		}
		else {
			pair.distance = 100.0; // will be filtered out
			pair.fileName = ""; // invalid
			fprintf(stderr, "Unable to read image [%s]\n", fileName); // skip this iteration
		}
		distances[i] = pair;
		puzzle_free_cvec(&context, &puzzleCvec);
	}
	std::cout << "all images loaded in " << (cilk_getticks() - start_ticks) << " milliseconds." << std::endl;


	// filter by thresholds
	vector<ImageDistancePair> similarImages;
	vector<ImageDistancePair> identicalImages;
	vector<ImageDistancePair> toplist(10);
	vector<ImageDistancePair> simmilarToplist(10);

	start_ticks = cilk_getticks();

	ImageDistancePair pair;
	for(unsigned int i = 0; i < files; i++){
		pair = distances[i];

		// dismiss images that are too different
		if (pair.distance <= IDENTITY_THRESHOLD){ // is identical 
			identicalImages.push_back(pair);
		}
		else { // just similiar
			similarImages.push_back(pair);
		}
	}
	std::cout << "all images loaded in " << (cilk_getticks() - start_ticks) << " milliseconds." << std::endl;

	start_ticks = cilk_getticks();

	
	cilk_spawn	sortImages(similarImages, simmilarToplist);
	
	sortImagesDublicate(identicalImages, toplist);
	cilk_sync;
	

	std::cout << "sorted in " << (cilk_getticks() - start_ticks) << " milliseconds." << std::endl;


	//struct_qsort(similarImages.begin(),similarImages.end());
	

	// print results

	// top 10 list or less
	unsigned int size = simmilarToplist.size();
	writeOutputLine("*** Pictures found to be similar to " + string(opts.refImage) + " ***\n ");
	for (int i = 0; i < size; i++){
		pair = simmilarToplist[i]; //topList[i];
		if (pair.fileName == "") continue;
		writeOutputLine(to_string((long double)pair.distance) + " " + pair.fileName);
	}

	// print identical
	writeOutputLine("\n*** Pictures found to be identical/close resemblance to " + string(opts.refImage) + " ***\n");
	size = toplist.size();
	for (int i = 0; i < size; i++) {
		pair = toplist[i];
		if (pair.fileName == "") continue;
		writeOutputLine(to_string((long double)pair.distance) + " " + pair.fileName);
	}


	
	// free reference image & context
    puzzle_free_cvec(&context, &cvec1);
    puzzle_free_context(&context);
	cout << "Overall execution time: " << cilk_getticks() - executionStart << endl;
    return 0;
}

