because i WILL forget

Follow the [Google style guide](https://google.github.io/styleguide/cppguide.html) unless otherwise called out.

- naming:
 macros are SCREAMING_SNAKE 
 types (including enums, classes, concepts, and typedefs) and methods are PascalCase 
 namespaces, free functions, variables, and members are camelCase 
 prefix constants with k, non-constant statics with s, private members with m, and template parameters with T 
 Base classes should start with the prefix "Base". 
 file names are camelCase. 

- source files are .cpp, headers are .h, leave template implementations in the header that declared them.

- use `#pragma once` guards.

- prefer forward declaration to nested header inclusion

- C++ exceptions, virtual inheritance, RTTI, and dynamic_cast<> are all valid to use (performance, schmerformance). Avoid multiple inheritance if you can.

- every member variable should be initialized. 
```
struct MyStruct {
    MyStruct(): a(), b() { e = 0; }

    // fine, they are initialized in constructor list
    int a;
    int b;

    // fine, they are initialized inline
    int c{};
    int d = 0;

    // BAD! even though it's manually intialized in constructor
    int e;
}
```
if you don't know how to initialize something, empty braces should work, and if they don't that's proof you need to something extra in the constructor