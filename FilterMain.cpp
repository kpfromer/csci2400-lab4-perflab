#include "Filter.h"
#include "cs1300bmp.h"
#include <fstream>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>

using namespace std;

#include "rdtsc.h"

//
// Forward declare the functions
//
Filter *readFilter(string filename);
double applyFilter(Filter *filter, cs1300bmp *input, cs1300bmp *output);

int main(int argc, char **argv) {

  if (argc < 2) {
    fprintf(stderr, "Usage: %s filter inputfile1 inputfile2 .... \n", argv[0]);
  }

  //
  // Convert to C++ strings to simplify manipulation
  //
  string filtername = argv[1];

  //
  // remove any ".filter" in the filtername
  //
  string filterOutputName = filtername;
  string::size_type loc = filterOutputName.find(".filter");
  if (loc != string::npos) {
    //
    // Remove the ".filter" name, which should occur on all the provided filters
    //
    filterOutputName = filtername.substr(0, loc);
  }

  Filter *filter = readFilter(filtername);

  double sum = 0.0;
  int samples = 0;

  for (int inNum = 2; inNum < argc; inNum++) {
    string inputFilename = argv[inNum];
    string outputFilename =
        "filtered-" + filterOutputName + "-" + inputFilename;
    struct cs1300bmp *input = new struct cs1300bmp;
    struct cs1300bmp *output = new struct cs1300bmp;
    int ok = cs1300bmp_readfile((char *)inputFilename.c_str(), input);

    if (ok) {
      double sample = applyFilter(filter, input, output);
      sum += sample;
      samples++;
      cs1300bmp_writefile((char *)outputFilename.c_str(), output);
    }
    delete input;
    delete output;
  }
  fprintf(stdout, "Average cycles per sample is %f\n", sum / samples);
}

class Filter *readFilter(string filename) {
  ifstream input(filename.c_str());

  if (!input.bad()) {
    int size = 0;
    input >> size;
    Filter *filter = new Filter(size);
    int div;
    input >> div;
    filter->setDivisor(div);
    for (int i = 0; i < size; i++) {
      for (int j = 0; j < size; j++) {
        int value;
        input >> value;
        filter->set(i, j, value);
      }
    }
    return filter;
  } else {
    cerr << "Bad input in readFilter:" << filename << endl;
    exit(-1);
  }
}

inline void applyPixel(class Filter *filter, cs1300bmp *input, const int &i,
                       const int &j, const int &row, const int &col,
                       short *valueR, short *valueG, short *valueB) {
  int filterValue = filter->get(i, j);
  *valueR = *valueR +
            (input->color[row + i - 1][col + j - 1][COLOR_RED] * filterValue);
  *valueG = *valueG +
            (input->color[row + i - 1][col + j - 1][COLOR_GREEN] * filterValue);
  *valueB = *valueB +
            (input->color[row + i - 1][col + j - 1][COLOR_BLUE] * filterValue);
}

double applyFilter(class Filter *filter, cs1300bmp *input, cs1300bmp *output) {

  long long cycStart, cycStop;

  cycStart = rdtscll();

  output->width = input->width;
  output->height = input->height;

  int cols = (input->width) - 1;
  int rows = (input->height) - 1;
  int filterDivisor = filter->getDivisor();

#pragma omp parallel for collapse(2)
  for (int row = 1; row < rows; ++row) {
    for (int col = 1; col < cols; ++col) {
      short *valueR = &output->color[row][col][COLOR_RED];
      short *valueG = &output->color[row][col][COLOR_GREEN];
      short *valueB = &output->color[row][col][COLOR_BLUE];

      applyPixel(filter, input, 0, 0, row, col, valueR, valueG, valueB);
      applyPixel(filter, input, 0, 1, row, col, valueR, valueG, valueB);
      applyPixel(filter, input, 0, 2, row, col, valueR, valueG, valueB);
      applyPixel(filter, input, 1, 0, row, col, valueR, valueG, valueB);
      applyPixel(filter, input, 1, 1, row, col, valueR, valueG, valueB);
      applyPixel(filter, input, 1, 2, row, col, valueR, valueG, valueB);
      applyPixel(filter, input, 2, 0, row, col, valueR, valueG, valueB);
      applyPixel(filter, input, 2, 1, row, col, valueR, valueG, valueB);
      applyPixel(filter, input, 2, 2, row, col, valueR, valueG, valueB);

      *valueR = *valueR / filterDivisor;
      *valueG = *valueG / filterDivisor;
      *valueB = *valueB / filterDivisor;

      if (*valueR < 0) {
        *valueR = 0;
      }
      if (*valueR > 255) {
        *valueR = 255;
      }

      if (*valueG < 0) {
        *valueG = 0;
      }
      if (*valueG > 255) {
        *valueG = 255;
      }

      if (*valueB < 0) {
        *valueB = 0;
      }
      if (*valueB > 255) {
        *valueB = 255;
      }
    }
  }

  cycStop = rdtscll();
  double diff = cycStop - cycStart;
  double diffPerPixel = diff / (output->width * output->height);
  fprintf(stderr, "Took %f cycles to process, or %f cycles per pixel\n", diff,
          diff / (output->width * output->height));
  return diffPerPixel;
}