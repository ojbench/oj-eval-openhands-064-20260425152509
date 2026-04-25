#include "printf.hpp"
#include <iostream>
#include <vector>
#include <string>

int main() {
    // Test basic string formatting
    sjtu::printf("Hello %s!\n", "World");
    
    // Test integer formatting
    sjtu::printf("Number: %d\n", 42);
    sjtu::printf("Unsigned: %u\n", 100u);
    
    // Test escape
    sjtu::printf("Percent: %%\n");
    
    // Test default formatting
    sjtu::printf("Default int: %_\n", 123);
    sjtu::printf("Default unsigned: %_\n", 456u);
    sjtu::printf("Default string: %_\n", "test");
    
    // Test vector formatting
    std::vector<int> vec = {1, 2, 3, 4, 5};
    sjtu::printf("Vector: %_\n", vec);
    
    return 0;
}
