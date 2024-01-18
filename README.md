# CHIP-8 Interpreter
Basic CHIP-8 Interpreter, made using C with SDL2.

CHIP-8 is an interpreted programming language that was designed for early personal computers. The emulator aims to replicate the behaviour of a CHIP-8 machine, allowing users to run and interact with CHIP-8 programs.

## Implemented Features
  - **SDL Integration:** This project utilises the SDL libraries for **window creation, rendering, and the handling of user input**.
  - **Emulator Configuration:** The emulator can be configured using a `config_t` struct, allowing users to set parameters such as window size, colours, and scale factor.
  - **Emulation Logic:** Implements the main CHIP-8 instruction set for executing ROMs.
  - **Graphics Rendering:** Handles graphics output, including pixel states and screen updates.
  - **Input Processing:** Manages user input to interact with CHIP-8 programs.
  - **Debugging Support:** Includes optional debugging functionality to aid in development (enabled via `DEBUG` flag).

## Getting Started
Follow these steps to get started with the interpreter:

  1. **Install Dependencies:**
     - **GCC:** Ensure you have the GNU Compiler Collection installed.
     - **Make:** Make sure you have the Make build tool installed.
     - **For Linux:**
       - Install `libsdl2` and `libsdl2-dev` packages.
     - **For Windows:**
       - Download and unzip `SDL2-devel-2.XX.X-mingw.zip` from [SDL Releases](https://github.com/libsdl-org/SDL/releases).

  2. **Build the Project:**
     - Open a terminal and navigate to the project directory.
     - Run the command `make` to build the project.

  3. **Run the Interpreter:**
     - On Linux: `./chip8 <path/to/rom/file>`
     - On Windows: `chip8 <path/to/rom/file>`
  
## Dependencies:
  - gcc
  - make
  - **For Linux:**
    - libsdl2 and libsdl2-dev packages.
  - **For Windows:**
    - SDL2-devel-2.XX.X-mingw.zip. (Download from [SDL Releases](https://github.com/libsdl-org/SDL/releases))


Feel free to provide feedback to enhance the functionality and performance of the emulator. 
