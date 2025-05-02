#include "Application.h"
#include <iostream>      // For std::cerr, std::endl, std::cout
#include <stdexcept>     // For std::exception
#include <string>

const unsigned int SCR_WIDTH = 1500;
const unsigned int SCR_HEIGHT = 1000;

int main() {
    try {
        Application app(SCR_WIDTH, SCR_HEIGHT, "Minecraft Clone (Structure)");

        if (!app.initialize()) {
            // English error message
            std::cerr << "Failed to initialize the application." << std::endl;
            return -1;
        }

        app.run();

    }
    catch (const std::exception& e) {
        // English error message
        std::cerr << "Runtime Error: " << e.what() << std::endl;
        return -1;
    }
    catch (...) {
        // English error message
        std::cerr << "An unknown critical error occurred." << std::endl;
        return -1;
    }

    // English status message
    std::cout << "Application finished execution." << std::endl;
    return 0;
}