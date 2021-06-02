///////////////////////////////////////////////////////
// Main function for the application
///////////////////////////////////////////////////////

// reporting and propagating exceptions
#include <iostream> 
#include <stdexcept>
#include <cstdlib> // EXIT_SUCCES and EXIT_FAILURE

// include the application
#include <app/Application.h>

int main() {
    Application app;
    try {
        app.run();
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}