# AGENTS Instructions

This file provides instructions for contributors and automated tools. It applies to the entire repository.

## Style
- Use `clang-format` version 16 or later with the included `.clang-format` for all C, C++, Objective-C, and Objective-C++ sources.
- Ensure each file ends with a trailing newline.

## Build and Test
After modifying code, run the following commands to make a best effort to build and test the project:

```sh
cmake --preset ubuntu-x86_64
cmake --build --preset ubuntu-x86_64
ctest --test-dir build_x86_64
```

If any command fails due to missing dependencies, attempt to install them and re-run the command.

## Commit Guidelines
- Write commit messages in the imperative mood and keep the summary line short.
- In Pull Request descriptions, summarize the changes and list the build and test commands you executed.
