# BizWiz - Not the future of business

This folder contains random files used during the development of the Thesis project. This ReadMe may or may not explain everything. Basically all code is contained in the exciting main.cpp file. Almost 5000 lines of exciting, clean code... Please do not use this as an example of anything good at all, it is only throwaway-prototype code meant to test things. It most certainly contains bugs too. Probably an infestation at this point.

## Dependencies

This program uses the library raylib for rendering. You will have to download and install it manually here (then set the makefile paths correctly). https://github.com/raysan5/raylib/

This program uses emscripten for web compilation. If you wish to compile to the web you will need to download it. https://emscripten.org/

This program uses tinyxml2, but since it's included you do not have to do anything.

## Compile & Run
All instructions assume windows (for now). 

To compile for desktop, run `make`. This generates `desktop.exe`.

To compile for web, run `make web`, This generates the a bunch of index files. 
To launch the program run the command `emrun --no_browser --port 8080 .` 
Then open `http://localhost:8080/index.html` in your browser.

## Example
A running proram may be found at https://people.cs.umu.se/oi17okf/

It is unknown how long it will be available.

The program behaves oddly in a browser due to mouse capture. The program has two main states. Mouse cursor active/not active. When the mouse cursor is active the user may inspect the environment by right clicking with the mouse. To exit this state select the option 'hide mouse'. Then one may walk around using WASD.