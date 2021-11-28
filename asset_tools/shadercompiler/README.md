# Shader compiler
Shader compiler for Carrot.

Basically a fancy wrapper around glslang.

Supports includes from `resources/shaders/` folder, both locally (#include "a") for sibling files 
and system-wide (#include &lt;a&gt;) to search from a `resources/shaders` root.