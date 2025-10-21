#include <iostream>
#include <sstream>

int main() {
    std::ostringstream name;
    
    // Assuming you have some code that modifies the content of the 'name' stream.
    // For example:
    name << "LOAD";

    // Check if the content of 'name' is equal to "LOAD"
    if (name.str() == "LOAD") {
        // Perform the operation if the content is equal
        std::cout << "Name is LOAD. Performing operation..." << std::endl;

        // Your operation code goes here.
    } else {
        // Perform another operation if the content is not equal
        std::cout << "Name is not LOAD. Performing a different operation..." << std::endl;

        // Your other operation code goes here.
    }

    return 0;
}





