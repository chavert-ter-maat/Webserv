#include <iostream>

double bmi(double weight, double height) { return weight / (height * height); }

int main(int argc, char **argv) {
  double weight;
  double height;

  std::cout << "Content-Type: " << "text/html\r\n\r\n";
  std::cout << "<!DOCTYPE html><html lang=\"en\"><head><title>BMI "
               "Calculator</title></head><body>"
            << std::endl;

  if (argc != 3) {
    std::cout << "Program takes two input params: 'weight (in kg)' and 'height "
                 "(in m)'"
              << std::endl;
    std::cout << "</body></html>";
    return 0;
  }

  try {
    weight = std::stod(argv[1]);
    height = std::stod(argv[2]);
  } catch (std::exception &e) {
    std::cout << "Invalid input" << std::endl;
    std::cout << "</body></html>";
    return 0;
  }
  double result = bmi(weight, height);
  std::cout << "Weight = " << weight << " kg" << std::endl;
  std::cout << "Height = " << height << " m" << std::endl;
  std::cout << "BMI = " << result << std::endl;
  std::cout << "</body></html>";
}
