Small more or less self-contained library to read and write an intrinsic scene format. This library depends on [Epsilon-Intersection](https://github.com/Jojendersie/Epsilon-Intersection) and the Cpp standard library.

The managed scene format consists of to files: a binary model (.bim) and a JSON file for materials and light sources. Moreover, it supports chunkt parallel loading processes for out-of-core scenarios.

## How to use? ##
To include the library into a project add it (and ε) as submodules and simply compile the cpp files along with the other files from your project. (This is still the most compatible way to include a cpp library - crossplatform).

To load a bim do the following:

    #include "bim.hpp"

	...
	bim::BinaryModel model;
	model.load("test.bim", "test.json");
	model.makeChunkResident(ei::IVec3(0));

The load() function only loads meta information for the scene (e.g. the number of chunks). The last line is necessary to get the actual data. Each chunk must be made resident for itself. This allows to handle scene files larger than the current RAM (as long as at least the requested number of chunks fits into the memory). Usually smaller files only have a single chunk.

## File Structure ##
The binary file is stored in a classical chunk pattern (not to confuse with the scene chunks). A file-chunk starts with a header (4 byte type, 8 byte size value) followed by its data of the length given in the header's size value. A loaded may ignore entire chunks by simply skipping them.

A bim file always begins with the META-chunk which stores information like the number of chunks and a bounding box. Then the CHUNK\_SECTION or other information follow. Inside the CHUNK\_SECTION the scene-chunks are stored as lists of property chunks. The number of scene-chunks is equal to that in the META section.

	META
	MATERIAL_REF
	CHUNK_SECTION
		CHUNK
			POSITIONS
			NORMALS
			TRIANGLES
			...
		CHUNK
			...
		...

An example for a JSON file looks as follows:

	{
		"materials": {
			"diffusegrey": {
				"albedo": [0.5, 0.5, 0.5, 0.0],
				"albedo": "grey.png"
			}
		}
		"lights": {
			<NOT SPECIFIED YET>
		}
	}

Thereby, materials can define any list of properties. A property must always be a list of up to four floats or a string for texture names. It is possible to use the same name twice, once for a texture and once for a value list.


----------

# Tools #

Currently there is one tool which uses the Assimp import library to convert almost any scene into a bim and a json file. It is simply called *tobim* and is a command line tool with the following options. Each option must be free of white spaces or set into "".

    -i<input file>      The input 3d model - should be loadable with Assimp
    -o<output file>     A name for the output without the suffix (.bim). If not
                        given the name of the input scene will be used.
    -g<X>,<Y>,<Z>       The resolution of the chunk grid. The scene is divided
                        into chunks by the uniform grid. The default is (1,1,1).
    -bAAB               Build BVH with axis aligned boxes. It is possible to
                        set multiple -b options.
    -bOB                Build BVH with oriented boxes. It is possible to set
                        multiple -b options.
    -mSAH               Use BVH build method with surface area heuristic.
    -mKD                Use BVH build method with axis aligned kd-tree.
    -cSGGX              Compute SGGX normal distributions for the nodes in the
                        hierarchy.
