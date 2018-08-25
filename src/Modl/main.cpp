#include "ModlApp.h"

int main(int argc, char *argv[]) {
    return ModlApp().Run(std::vector<std::string>(argv, argv + argc));
}
