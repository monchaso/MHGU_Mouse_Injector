cmake_minimum_required(VERSION 3.10)
project(MHGU_Mouse_Injector)

# Global Definitions
add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX)
add_executable(main 
    main.cpp
    Core/Memory.cpp
    Core/MouseInput.cpp
    Core/Scanner.cpp
    Core/Unlocker.cpp
)