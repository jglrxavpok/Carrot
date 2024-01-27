# Building the engine
## Requirements
1. You need to install the Vulkan SDK. I currently use the 1.3.268.0 LunarG SDK.
2. You need to install a Lua distribution. LuaJIT or Vanilla Lua are both supported by sol3 and should work out-of-the-box.
3. You need a Mono installation. Path is expected to be inside MONO_SDK_PATH environment variable. (https://www.mono-project.com/download/stable/)
   * For instance, I have to use "C:\Program Files\Mono" after running an installer from the Mono download page.
4. To build OpenXR-Hpp (required dependency), you will need a Python 3.9+ install.

## Steps

1. Install the requirements
2. Git clone this repository, with the option `--recurse-submodules` active.
3. Build Peeler (which is the editor) to ensure the entire engine & libraries compile correctly.