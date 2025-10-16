# Agent Rules Standard (AGENT.md):
# DistroAV Project Coding Rules

This document outlines the coding rules for the DistroAV project, combining guidelines from both DistroAV's own conventions and the OBS project's coding rules.

## Project structure and organization

The DistroAV project is a C++ OBS Studio plugin that utilizes CMake for its build system. The project is structured as follows:

*   **`src/`**: Contains the core source code for the plugin, including UI elements (`forms/`), OBS-specific support code (`obs-support/`), and the main plugin logic.
*   **`lib/`**: Contains third-party libraries, specifically the NDI SDK.
*   **`cmake/`**: Holds CMake modules and scripts for configuring the build process for different platforms (Linux, macOS, Windows).
*   **`tools/`**: A collection of shell scripts for common development tasks like building, cleaning, packaging, and running OBS with the plugin. Used only by developers t oquickly iterate locally.
*   **`CI/`**: Contains scripts for fetching and packaging the NDI library. This is used mostly for Linux.
*   **`.github/`**: Contains scripts that are the core of the build and packaging process leveraging github actions.
*   **`data/`**: Contains locale files for internationalization and additionnal configuration when required.

## Build, test, and development commands

The primary development and build commands are provided as scripts in the `tools/` directory. These scripts often wrap more complex logic contained within the `.github/scripts/` directory.

*   **Building the project:**
    *   `./tools/Build.sh`: This script is the main entry point for building the project. It invokes `.github/scripts/build-macos` to perform the actual build steps. The build process uses CMake and Xcode on macOS.

*   **Packaging the project:**
    *   `./tools/Package.sh`: This script packages the built plugin for distribution. It calls the `.github/scripts/package-macos` script.

*   **Running OBS with the plugin:**
    *   `./tools/RunOBS.sh`: This script launches OBS Studio and can be passed arguments to enable debugging flags for the plugin.

*   **Cleaning the build artifacts:**
    *   `./tools/Clean.sh`: This script removes the `build_macos` and `release` directories, cleaning up all build artifacts.

*   **CI and Dependency Management:**
    *   The scripts in the `CI/` directory are used to download and package the NDI SDK for Linux, which is a core dependency of the project. `CI/libndi-get.sh` downloads the SDK, and `CI/libndi-package.sh` creates a Debian package from it.

## Code style and conventions

*   **Language Standard:** C++17.
*   **Commit Messages:**
    *   Commit messages should have a title of 50 characters maximum, followed by an empty line, and a description wrapped to 72 columns.
    *   Commit titles must be in the present tense and prefixed with a scope (e.g., `CI`, `General`, `Source`).
*   **Formatting:**
    *   **Indentation:** Tabs are used for indentation and continuation, with a width of 8 columns.
    *   **Line Length:** The maximum line length is 120 characters.
    *   **Naming Conventions:**
        *   Use `snake_case` for function and variable names in C code.
        *   Use `CamelCase` for C++ names.
    *   **Braces:**
        *   Braces for functions are placed on the next line.
        *   Braces after control statements (if, else, while) are on the same line.
    *   **Pointers:** Pointer alignment is to the right (e.g., `int* a`).
    *   **Spaces:**
        *   Spaces are used before parentheses in control statements.
        *   No spaces after C-style casts.

## Architecture and design patterns

The DistroAV project is an OBS Studio plugin that integrates the NDI® (Network Device Interface) protocol. Its architecture is modular and event-driven, designed to seamlessly extend OBS Studio's functionality.

*   **Plugin Architecture**: The project follows the standard OBS Studio plugin architecture. The main entry point is in `src/plugin-main.cpp`, which defines the module loading (`obs_module_load`), unloading (`obs_module_unload`), and post-load (`obs_module_post_load`) functions. These functions handle the plugin's lifecycle, including initialization, resource management, and cleanup.

*   **Modular Design**: The plugin is composed of several distinct components, each responsible for a specific functionality:
    *   **NDI Source** (`ndi-source.cpp`): Implements an OBS source that can receive NDI streams. It manages the connection to NDI sources, handles video and audio data, and provides user-configurable properties.
    *   **NDI Output** (`ndi-output.cpp`): Implements an OBS output that can send NDI streams. It captures the main and preview outputs of OBS and transmits them over the network.
    *   **NDI Filter** (`ndi-filter.cpp`): Implements an OBS filter that can be applied to any source to send its output as an NDI stream.
    *   **Configuration Management** (`config.cpp`): Handles loading and saving of plugin settings from the OBS global configuration file. It uses a Singleton pattern to provide a single point of access to the configuration.

*   **Event-Driven Programming**: The plugin integrates with OBS Studio's event system to respond to various events, such as the frontend finishing loading, the application exiting, or profile changes. This is evident in the use of `obs_frontend_add_event_callback` in `src/plugin-main.cpp`.

*   **Multithreading**: The NDI source component uses a separate thread (`ndi_source_thread`) to handle the reception of NDI data. This prevents the main OBS thread from being blocked by network operations and ensures smooth video and audio playback.

*   **RAII (Resource Acquisition Is Initialization)**: The plugin uses RAII principles for managing resources. For example, the `ndi_source_create` and `ndi_source_destroy` functions in `ndi-source.cpp` handle the allocation and deallocation of resources for the NDI source.

*   **Singleton Pattern**: The `Config` class in `config.cpp` is implemented as a singleton, ensuring that there is only one instance of the configuration manager throughout the plugin's lifecycle.

*   **UI Integration**: The plugin uses Qt to create user interfaces for configuring the NDI output settings. The UI is defined in `.ui` files and loaded in the code, as seen in the `forms` directory.

## Testing guidelines

The DistroAV project currently lacks a formal, automated testing suite. Based on the analysis of the project structure and build scripts, the following guidelines are recommended to ensure the stability and reliability of the plugin:

*   **Manual Testing:** Since no automated testing framework is in place, manual testing is the primary method for validation. Testers should focus on the following areas:
    *   **NDI Source and Output:** Verify that NDI sources are correctly detected and that the output stream is stable and all features functional.
    *   **UI Interaction:** Test all UI elements, including settings for NDI sources and outputs, to ensure they function as expected.
    *   **Cross-Platform Functionality:** Test the plugin on all supported operating systems (Windows, macOS, and Linux) to identify any platform-specific issues.
*   **Integration Testing:** The plugin should be tested with various versions of OBS Studio to ensure compatibility and prevent regressions. Due to resource limitation this is limited to the latest available and the minimum required OBS version.
*   **Performance Testing:** Monitor computer ressource usage to ensure the plugin does not introduce significant performance overhead.

## Security considerations

The security analysis of the DistroAV project focused on potential vulnerabilities related to network communication, resource management, and interaction with the OBS host. The following considerations are important for maintaining the security of the plugin:

*   **Network Communication (NDI):** The plugin relies on the NDI SDK for network communication. While the NDI protocol itself is proprietary, it is essential to ensure that the plugin correctly handles NDI data to prevent potential buffer overflows or other vulnerabilities. All data received from the network should be treated as untrusted.
*   **Resource Management:** The plugin performs manual memory management. It is crucial to ensure that all allocated resources are properly freed to prevent memory leaks, which could lead to denial-of-service vulnerabilities.
*   **Interaction with OBS Host:** The plugin interacts with the OBS host through the OBS API. It is important to follow the API's guidelines and best practices to prevent crashes or other security issues within OBS Studio.
*   **Third-Party Libraries:** The plugin uses `libcurl` for network requests. It is important to keep this library updated to its latest version to mitigate any known vulnerabilities.