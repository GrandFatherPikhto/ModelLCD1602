cmake_minimum_required(VERSION 3.5.0)

project(Menu VERSION 0.1.0 LANGUAGES C)

set(SOURCES 
    main.c
    console.c
    menu.c
    )

include_directories("./include")

add_executable(${PROJECT_NAME} ${SOURCES})
