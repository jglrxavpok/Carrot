# Fertilizer

Fertilizer is the asset processing pipeline of Carrot.

It converts models and textures to a format Carrot can load quickly.
For instance, textures will be converted to KTX with a supercompression on top.

The output file will always take the timestamp of the input file, that way, already processed files are not re-processed.

## Usage
`fertilizer <file path> <output path> [arguments]`

The `[arguments]` are optional and will depend on the type of file.

### General options
- `-f`/`--force` Ignores whether the file was already processed and forces a reprocessing.

### Entire folders
- `-r`/`--recursive` Use this option to input a source folder and a destination folder. Fertilizer will apply its 
modifications to all compatible files inside the source folder (given via `<file path>`), and write the output to the destination (`<output path>`)

### Image files
Compresses the image to a fast to load and compressed format.

### .gltf files
Modifies the image uris inside the .gltf to point to compressed images. 
Does NOT perform the modification on these images, the images have to be converted by themselves.

Does copy the .bin file though.