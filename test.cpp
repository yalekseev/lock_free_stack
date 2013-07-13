#include <iostream>

#include "stack.h"

int main() {
    lock_free::stack<int> stack;
    stack.push(3);
    stack.push(2);
    stack.push(1);
    stack.push(0);

    int val;
    while (stack.try_pop(val)) {
        std::cout << "val: " << val << std::endl;
    }

    return 0;
}
