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

## Assimp
Assimp broke FBX animation timing "recently", so the Assimp dependency is my fork, with the <b8bf1eac041f0bbb406019a28f310509dad51b86> commit reverted.

# Credits
* Splash screen & logo by *Benjamin "beb" Er-Raach*