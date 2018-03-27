#include <iostream>
#include <string>
#include <sstream>
#include <random>
#include <algorithm>
#include <ctime>
#include <cstdlib>
/* I know this is already implemented in tide-index, but the first test I'll write will be a simple shuffle program */

int main(){
  std::string tempLine;
  std::srand(unsigned (std::time(0)));
  while(std::getline(std::cin, tempLine)){
    std::random_shuffle(tempLine.begin(), tempLine.end());
    std::cout << tempLine << std::endl;
  }
  return 0;
}
