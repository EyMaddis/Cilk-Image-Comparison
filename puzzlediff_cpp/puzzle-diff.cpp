extern "C" {
  #include "puzzle_common.h"
  #include "puzzle.h"
}

#include "pgetopt.hpp"
#include <vector>
#include <iostream>
#include "listdir.h"
#include <cilk/cilk.h>
using namespace std;

typedef struct Opts_ {
	const char *refImage;
    const char *dir;
    int fix_for_texts;
    int exit;
    double similarity_threshold;
} Opts;

void usage(void)
{
    puts("\nUsage: puzzle-diff [-o <outputFile>] referenceImage directory\n");
    exit(EXIT_SUCCESS);
}

int parse_opts(Opts * const opts, PuzzleContext * context,
               int argc, char **argv) {
    int opt;
    extern char *poptarg;
    extern int poptind;

    opts->fix_for_texts = 1;
    opts->exit = 0;
    opts->similarity_threshold = PUZZLE_CVEC_SIMILARITY_THRESHOLD;
    while ((opt = pgetopt(argc, argv, "o")) != -1) {
        switch (opt) {
        case 'o':
            // set output text atof(poptarg);

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

int main(int argc, char *argv[])
{
    Opts opts;
    PuzzleContext context;
	PuzzleCvec cvec1;
    
    puzzle_init_context(&context);    
    parse_opts(&opts, &context, argc, argv);


	puzzle_init_cvec(&context, &cvec1);

	// reference file
	if (puzzle_fill_cvec_from_file(&context, &cvec1, opts.refImage) != 0) {
		fprintf(stderr, "Unable to read reference image: [%s]\n", opts.refImage);
        return 1;
    }

	std::cout << "refImage " << opts.refImage << "\n";
	std::vector<std::string> fileNamesVector;
	
	listDir(opts.dir, fileNamesVector);	/* opts.file2 holds the directory name */
	std::cout << "Number of file names found in search directory: " << fileNamesVector.size() << "\n\n";

	//TODO: multiple
	unsigned int i = 0;
	cilk_for(i = 0; i < fileNamesVector.size(); i++){
		PuzzleCvec puzzleCvec;
		double d;
		const char* fileName = fileNamesVector[i].c_str();

		puzzle_init_cvec(&context, &puzzleCvec);
		if (puzzle_fill_cvec_from_file(&context, &puzzleCvec, fileName) == 0){

			d = puzzle_vector_normalized_distance(&context, &cvec1, &puzzleCvec, opts.fix_for_texts);

			if (d >= opts.similarity_threshold) {
				std::cout << "similiar image found: " << fileName << " distance: " << d << std::endl;
			}
			else {
				std::cout << "distance below threshold for " << fileName << " distance: " << d << endl;
			}
		}
		else {
			fprintf(stderr, "Unable to read image [%s]\n", fileName); // skip this iteration
		}
		puzzle_free_cvec(&context, &puzzleCvec);
	}

	
	// free reference image & context
    puzzle_free_cvec(&context, &cvec1);
    puzzle_free_context(&context);
    //if (opts.exit == 0) {
        //printf("%g\n", d);
    //    return 0;
    //}
    //if (d >= opts.similarity_threshold) {
    //    return 20;
    //}
    return 10;
}
