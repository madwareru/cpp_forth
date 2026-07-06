#include <iostream>
#include <cstdint>
#include <string>

#define interpreter_impl
#include <interpreter.h>

const std::string EXAMPLE_PROGRAM =
    "[ :c :b :a b b * 4 a c * * - ] :D "
    "5 neg 4 neg 1 D . "
    "[ :n n 2 < [ 1 ] [ n n 1 - fact * ] ifelse ] :fact "
    "5 fact . "
    "[ :n n [ n dec odd ] [ 1 ] ifelse ] :even "
    "[ :n n [ n dec even ] [ 0 ] ifelse ] :odd "
    "42 dup even . odd . "
    "2 dup dup * + . 42 67 drop "
;

int main() {
    std::int32_t res = 0;

    if(!eval_program(EXAMPLE_PROGRAM, res)) {
        return 1;
    }
    std::cout << "Result of a program is " << res << std::endl;

    std::string next_command;
    while(next_command != "quit") {
        std::cout << "> ";
        std::getline(std::cin, next_command);
        if (next_command != "quit") {
            if(eval_program(next_command, res))
                std::cout << "Result of a program is " << res << std::endl;
        }
    }

    return 0;
}
