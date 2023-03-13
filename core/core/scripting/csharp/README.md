## Why not reuse `MonoClass`, `MonoObject` and such directly?
The goal of this part of the core library is to avoid coupling engine code with Mono *too much*, 
AND support hot-reloading.