# Airport Desk Helper

This little console app helps you check airport desk lines (time, flight, and computer ID) so they look neat and correct.

## What is inside
- `Airport-v1.cpp` – the program code.
- `data.txt` – a practice list with some good and bad lines.
- `data.valid.csv` – the fixed version of that list with only the good lines.
- `output/latestValues.txt` – a sample of saved good answers (the app also saves to `latestValues.txt` next to the program).

## What you need
- A Windows PC.
- A C++17 compiler (free: install **modern MinGW-w64** or use **VS Code + C++ tools**).

### modern MinGW-w64 installation

```
    MSYS2 (best choice):

    1. Download: https://www.msys2.org

    2. Install

    3. Open MSYS2 UCRT64

        Run:

        pacman -S mingw-w64-ucrt-x86_64-gcc

    4. Add to PATH:

        C:\msys64\ucrt64\bin


    This gives you:

        - Modern GCC

        - Full C++17 / C++20 support

        - No hacks needed
```

## How to run it (Windows)
1) Open **Command Prompt** in this folder.
2) Build the program:
   ```bash
   g++ -std=c++17 Airport-v1.cpp -o airport.exe
   ```
3) Start it:
   ```bash
   .\airport.exe
   ```

## Quick rules the app checks
- Time: looks like `09:25:30` or `092530` (24-hour clock).
- Flight ID: starts with a letter, up to 10 letters or numbers.
- Computer ID: exactly 3 letters/numbers; avoid the letters `I` or `O` in the last two spots.

## What you will see
A menu with options:
- [1] Quick check: type one line and see if it is valid.
- [2] Guided mode: answer small questions (hour, minute, second, flight, computer) to build a correct line.
- [3] Show saved lines: read what you already saved.
- [4] Read a file: peek at `data.txt` or another file.
- [5] Analyze a file: find which lines are good or bad and optionally save the good ones to a `.valid.csv` file.
- [6] Help: a short reminder of the rules.
- [7] Exit: close the app.

## Fast try
- Pick option 2 (Guided mode) and follow the prompts.
- Or copy a line from `data.txt` and test it with option 1.

## If something goes wrong
- If the window shows strange color codes, that is just decoration; the app still works.
- If it says a file is missing, make sure the file name is typed correctly (for example `data.txt`).
- If `g++` is not found, install modern MinGW-w64 and add it to your PATH, then try again.
