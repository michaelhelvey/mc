# mc

little minimal C programs that make building things I like easier.

## Getting Started

This project is built using [fab](https://github.com/michaelhelvey/fab). You can read more about
that build system at the provided link. In order to build this project as a static library, you can
run `fab build --mode release .`. This will produce `libmc.a` in `./target/release`. Header files
are in `./src/include`. You are welcome to include these headers and static library in your project
however you see fit.

## Programs

_Current_:

- JSON parser
- Common utilities (e.g. a string view type)
- Arena allocator

_Planned_:

- HTTP 1.1 client (with configurable OpenSSL support for HTTPS)
