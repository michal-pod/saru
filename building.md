# Building Instructions
This document provides instructions on how to build the SARU application from source code. Follow the steps below to set up your development environment and compile the project.

## Prerequisites
Before you begin, ensure you have the following software installed on your system:
- **Meson Build System**: Version 1.4 or higher
- **Conan Package Manager**: For managing dependencies
- **pkg-config**: For handling library configurations
- **Ninja Build System**: For building the project
- **Visual C++ Compiler**: Part of Visual Studio 2022 or later
- **Git**: For cloning the repository
- **WiX Toolset**: For creating Windows installers

Some tools for example Meson and Ninja are bundled together. Some need to be installed separately.

## Cloning the Repository
To get the source code, clone the SARU repository using Git:
```bash
git clone https://github.com/michal-pod/saru.git
cd saru
# Download submodules
git submodule update --init
```

## Configuring Conan
Set up Conan to use the required profiles for your build environment. You may need to create or modify a Conan profile to match your compiler and architecture settings. Cross compilation is supported; ensure you have the appropriate settings in your profile. Please refer to the [Conan documentation](https://docs.conan.io/en/latest/) for detailed instructions on setting up profiles.

## Install documentation
To build packages you need to have the documentation file `SARU-ssh-agent.pdf` in the `installer` folder. This documentation and build instruction is available in separate repository: [SARU Documentation](https://github.com/michal-pod/saru-documentation). Download the latest release and copy the PDF file to the `installer` folder.

## Building the Project
If you have all the prerequisites installed and conan configured correctly, you can proceed to build the project using only one simple command:

```bash
conan build . --build=missing
```

This command will configure the build environment, resolve dependencies, and compile the project.

Cross compilation is more complex, you need to specify the correct conan profile for your target architecture. For example, to build for arm64 architecture, you would use:

```bash
conan build . --profile:build=default --profile:host=your_arm64_profile --build=missing
```

## Creating a release package

The MSI and ZIP packages are intentionally not built as part of the default
build.  After configuring a build directory, create a signed release manually:

```bash
meson compile -C build package-release
```

The target requires `signtool`, `gpg` and `wix` to be available in `PATH`.
Use `CODESIGN_SHA` or `CODESIGN_CN` to select the Authenticode certificate;
`CODESIGN_ISSUER` optionally restricts the selection.  `GPG_SIGNING_KEY` is
required to create and verify detached signatures for the MSI and ZIP files.
