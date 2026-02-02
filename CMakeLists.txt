cmake_minimum_required(VERSION 3.10)
project(MHGU_Mouse_Injector)
add_executable(main 
    main.cpp
    Core/Memory.cpp
    Core/MouseInput.cpp
    Core/Scanner.cpp
    Core/Unlocker.cpp
)