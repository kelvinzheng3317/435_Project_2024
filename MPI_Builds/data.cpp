#include <string>

using namespace std;

void generateArray(int* data, string arrType, int arrSize) {
  if (arrType == "sorted") {
    for (int i=0; i<arrSize; ++i) {
      data[i] = i;
    }
  } else if (arrType == "perturbed") {
    // generated sorted array
    for (int i=0; i<arrSize; ++i) {
      data[i] = i;
    }
    // perturbed 1% of the array
    for (int i=0; i<arrSize / 100; ++i) {
      data[rand() % arrSize] = rand();
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