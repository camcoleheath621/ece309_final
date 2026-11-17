# Local setup

Working copy: `Final_starter` in the ECE 309 ChatGPT workspace.
Remote: https://github.com/camcoleheath621/ece309_final
Branch: main

## VS Code

Open this folder in VS Code. Press Ctrl+Shift+B to configure and build using
Ubuntu WSL2. Use Terminal > Run Task > Test (WSL) for all tests.
The installed WSL, C/C++, and CMake extensions are available.
For Linux IntelliSense and debugging, use the command palette action
WSL: Reopen Folder in WSL.

## Linux terminal commands

```bash
cmake -S . -B build -DMH_WERROR=ON
cmake --build build -j 4
ctest --test-dir build --output-on-failure
```

## Initial baseline (November 16, 2026)

- Copied exactly the eight required P2/P3 files from the existing P3_starter.
- Reference switches are OFF; the project uses those copied files.
- Ubuntu build passes with -Werror, ASan, and UBSan enabled.
- P2: 14/14 tests pass. P3 tool loop: 9/9 pass.
- End-to-end: 7/11 pass; the four new final-project demos fail.
- Final examples: 0/8 pass because the graded implementations are still stubs.
- These final-project failures are the expected starting baseline.

## Git workflow

```bash
git status
git add <files-you-changed>
git commit -m "Describe the change"
git push
```

Start with MinHeap/top_k and LruCache, then memory/context, policies, and skills.
Use Final_spec.md for exact requirements and keep the design log updated.
Build files and local memory/audit output are excluded from commits.
