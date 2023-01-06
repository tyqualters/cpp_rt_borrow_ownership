/**
 * This is a C++ example of implementing Rust Lifetimes for memory safety.
 * Note: This is more relevant to Runtime assurance, not so similar to Rust's compile time assurance.
 */
#include <iostream>
#include "lifetime.hpp"

auto add(Lifetime<int> b) /* Lifetime<int>& would be the same instance */ {
    b.set(b.get() + 5);
    std::cout << "Value of 'b' is: " << b.get() << std::endl;
}

auto main() -> int {

    auto a = Lifetime<int>::from(15); // Create a new Lifetime (int) with value: 15

    // add(a); cannot be duplicated (will subsequently error - CT - deleted functions)
    add(a.clone()); // Clone the Lifetime
    // add(a.borrow_mutable()); // Borrow as a mutable reference
    // add(a.move()); // Move ownership (will subsequently error - RT - owner deleted)

    if(a.is_owner()) // verify ownership
        a.set(15);

    std::cout << "Value of 'a' is: " << a.get() << std::endl;

    return 0;
}
