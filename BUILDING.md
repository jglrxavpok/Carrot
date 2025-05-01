# Building the engine

## Requirements
1. You need to install the Vulkan SDK. I currently use the 1.3.268.0 LunarG SDK.
2. You need to install a Lua distribution. LuaJIT or Vanilla Lua are both supported by sol3 and should work out-of-the-box. On Linux, `liblua5.1-dev` should work.
3. (Linux) You may need to install `xorg-dev` for GLFW (as per its documentation).
4. (Linux) You may need to install `libgtk-3-dev` for nativefiledialog.
5. (Linux) You may need to install `libxcb-glx0-dev` for OpenXR.
6. (Linux) You may need to install `libpulse-dev` for OpenAL.
7. You need a Mono installation. Path is expected to be inside MONO_SDK_PATH environment variable. (https://www.mono-project.com/download/stable/)
   * For instance, I have to use "C:\Program Files\Mono" after running an installer from the Mono download page.
8. To build OpenXR-Hpp (required dependency), you will need a Python 3.9+ install.
Go into `thirdparty/OpenXR-Hpp` and run `generate-openxr-hpp.sh` (or .ps1). If you have the error ` cannot import name 'soft_unicode' from 'markupsafe'`, you need to remove markupsafe & jinja2 from your installed packages on Linux (see https://github.com/KhronosGroup/OpenXR-SDK-Source/issues/416).
9. Then you can open the CMake project with your IDE, or generate the project via the command line.

TODO: automate and remove dependencies where possible

## Steps

1. Install the requirements
2. Git clone this repository, with the option `--recurse-submodules` active.
3. Build Peeler (which is the editor) to ensure the entire engine & libraries compile correctly.
4. If you use GDB as your debugger, you will need to run the GDB command `handle SIGXCPU SIG33 SIG35 SIG36 SIG37 SIG38 SIGPWR nostop noprint` to disable GDB panicking with the signals used internally by Mono (you can add it to your .gdbinit file)