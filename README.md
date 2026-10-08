HotDylib
--------
Hot reload for dynamic library. Written in C99.

Features
--------
- Hot reloading DLL
- Structured Exception Handler (SEH)
- Simple file watcher
> Note: this library use C stdlib, setjmp, and malloc/free internally.

FAQs
----
- Why new library instead of use cr?
  - I want to create to understand mechanic of hot reload in C/C++. This project follow 2 principles of MaiCstyle: self-reliance and immediate feedback.

- Is it production ready?
  - Only for development: yes.
    For user production: No. Because many platforms not accept application load dynamic library in runtime. And there no showcase in final production yet.

- So how I can use code that write in hot dynamic library (guest library) in final product?
  - Just move your code to host project (application that load dynamic library at runtime).
    Or seperate configuration that have development load dynamic library at runtime, and release that linking the dynamic library.

- I found this project use premake5, how do use custom build system?
  - Just copy the source files (.h and .c) to your project. You can amalgamate source files into single header libray.

- I don't like the implementation? The library are too bloat?
  - That's okay! You can check the [/Docs](/Docs) folder, where I collecting good resources, tips and tricks to make libray and use hot reload in practice.

License
-------
Unlicense @ MaiHD 2019 - 2026
