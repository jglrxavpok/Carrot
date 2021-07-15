# Carrot

Game engine in C++, for *fun*, and to attempt a tiny clone of Pikmin.

Don't expect support.

## Some of its features
- Vulkan renderer
- Raytraced lighting & shadows
- Skeletal animation support
- Entity Component System

## Last screenshot
![Last screenshot](./screenshot.png)

# Building
VR requires building manually thirdparty/OpenXR-SDK-Source

Use `XR_API_LAYER_PATH=<path to Carrot>\thirdparty\OpenXR-SDK-Source\build\win64\src\api_layers` as an environnement variable to enable api layers. Change win64 by your platform if necessary.

# Credits
* Splash screen & logo by *Benjamin "beb" Er-Raach*