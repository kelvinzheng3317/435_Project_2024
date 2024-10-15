#include <string>

using namespace std;

void generateArray(int* data, string arrType, int arrSize) {
  if (arrType == "sorted") {
    for (int i=0; i<arrSize; ++i) {
      data[i] = i;
    }
  } else if (arrType == "perturbed") {
    // should it be exactly 1% of the data or is approximately 1% good enough?
    for (int i=0; i<arrSize; ++i) {
      if (rand() % 100 == 1) {
        data[i] = rand();
      } else {
        data[i] = i;
      }
    }
  } else if (arrType == "random")
  {
    for (int i = 0; i < arrSize; i++) {
      data[i] = rand();
    }
  } else if (arrType == "reverse") {
    for (int i=0; i < arrSize; i++) {
      data[i] = arrSize - i;
    }
  }
}