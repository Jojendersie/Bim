Small more or less self-contained library to read and write an intrinsic scene format. This library depends on [Epsilon-Intersection](https://github.com/Jojendersie/Epsilon-Intersection) and the Cpp standard library.

The managed scene format consists of to files: a binary model (.bim) and a JSON file for materials and light sources. Moreover, it supports chunked parallel loading processes for out-of-core scenarios.

## How to use? ##
To include the library into a project add it (and ε) as submodules and simply compile the cpp files along with the other files from your project. (This is still the most compatible way to include a cpp library - cross platform).

To load a bim do the following:

    #include "bim.hpp"

    ...
    bim::BinaryModel model;
    model.load("test.json");
    model.makeChunkResident(ei::IVec3(0));

The load() function only loads meta information for the scene (e.g. the number of chunks). The last line is necessary to get the actual data. Each chunk must be made resident for itself. This allows to handle scene files larger than the current RAM (as long as at least the requested number of chunks fits into the memory). Usually smaller files only have a single chunk.

## Json File Structure

A json file is used to define a scene environment. It references exactly one \*.bim binary file.
An example for a JSON file looks as follows:

    {
        "scene": "blabla.bim",
        "accelerator": "aabox",
        "cameras": {
            "controlVelocity": 2.5 // Allows to change the movement speed in the scene for interaction
            <camname>: {
				"type": "perspective",
                "scenario": ["stresstest"],
                "position": [1.0, 2.0, 3.0],
                "fov": 67.0,
                "lookAt": [0.0, 0.0, 0.0],
				"velocity": 2.5, // Factor for interactive movements
            },
            <camname>: {
				"type": "orthographic",
                "scenario": ["baking"],
                "direction": [0.0, 0.0, 1.0],
                "position": [1.0, 2.0, 3.0],
                "left": -4.0,
                "right": 4.0,
                "top": 4.0,
                "bottom": -4.0,
                "near": 0.0,
                "far": 10.0,
            },
            <camname>: {
				"type": "focus",
                "scenario": ["day"],
                "position": [1.0, 2.0, 3.0],
                "lookAt": [0.0, 0.0, 0.0],
                "focalLength": 20.0,
                "focusDistance": 1.0,
                "sensorSize": 24.0,
                "aperture": 1.4
            }
        },
        "sensor": {
            "resolution": [1024, 512],
            "filter": {"type": "none"},
            "toneMapper": {"type": "none"}
        },
        "materials": {
            <matname>: {
                "type": "physical",
                "albedo": [0.5, 0.5, 0.5],
                "roughness": "rough.png"
            }
        },
        "lights": {
            <lightname>: {
				"type": "point",
                "position": [1, 2, 3],
                "intensity": [10, 50, 10],
                "scenario": ["day", "stresstest", ...]
            }
        }
    }

### "scene" ###
Required property which references the binary scene file.
Currently only a single binary file is supported.
The given file name is interpreted as relative to the json-file.

### "accelerator" ###
The ray tracing structure which should be used. The binary file must contain
the precomputed structure to be used. Then, valid choices are `aabox` (BVH) and `obox` (BVH).
The default is `aabox`.

### "cameras" ###
A camera defines an (initial) view to the scene. Most of the cameras can be moved around interactively. The `velocity` [units/s] attribute can always be given and should be chosen dependent on the scene size. A value of 0 disables interaction (including the rotation).

Like lights a camera can be used in one or multiple `scenario`s. On the other side, each scenario must have exactly one camera.

The different camera `type`s are:

`perspective`:

    position      The position.                              {0, 0, 0}
    lookAt        A position which is centered in the image. {0, 0, 1}
    direction     Alternative to lookAt: a direction.        {0, 0, 1}
                  Does not need to be normalized.
    up            Up orientation of the camera (not y axis)	 {0, 1, 0}
    fov           Vertical field of view in degree.          {90}

`orthographic`:

    position      The position.                              {0, 0, 0}
    lookAt        A position which is centered in the image. {0, 0, 1}
    direction     Alternative to lookAt: a direction.        {0, 0, 1}
                  Does not need to be normalized.
    up            Up orientation of the camera (not y axis)	 {0, 1, 0}
    left          Left clipping plane in view space.         {-1}
	right         Right clipping plane in view space.        {1}
	bottom        Bottom clipping plane in view space.       {-1}
	top           Top clipping plane in view space.          {1}
	near          Near clipping plane in view space.         {0}
	far           Far clipping plane in view space.          {1e30}

`focus`: A perspective projection wich simulates DOF of a thin lens.

	position      The position.                              {0, 0, 0}
	lookAt        A position which is centered in the image. {0, 0, 1}
	direction     Alternative to lookAt: a direction.        {0, 0, 1}
                  Does not need to be normalized.
    up            Up orientation of the camera (not y axis)	 {0, 1, 0}
	focalLength   Focal length of the lens in [mm].          {20}
	focusDistance Distance to the sharp plane in [m]         {1}
	sensorSize    Vertical size of the sensor in [mm]        {24}
	aperture      Aperture in f-stops (1.0, 1.4, ...)        {1.0}

### "sensor" ###
Defines the image resolution and post processing.

`filter` `type` can be:

* `none`  
* `bilateral`: bilateral noise reduction. Has additional parameter `radius`.

`toneMapper` `type` can be:

* `none`  
* `gamma`: gamma curve correction (will be made on Luma channel in YCoCg). Has additional parameter `exponent`.

### "materials" ###

Materials are defined as a list of certain properties. Their name is used as reference in the binary file and cannot be changed therefore.

A property must always be a list of floats or a string for texture names. It is possible to use the same name twice, once for a texture and once for a value list in which case both are multiplied.
Spectral parameters are currently not supported.

There are several types of materials:

`physical`: most used material which can model a lot of 'solid' objects including water, milk...

    albedo          Diffuse color (also used in scattering). (RGB)                  {0.5, 0.5, 0.5}
    refractionIdxN  First part of complex valued refraction index. (S, RGB)         {1.3}
    refractionIdxK  Second part of complex valued refraction index. (S, RGB)        {0}
    emissivity      Exitant radiant energy (RGB) [cd/m^2]                           {0, 0, 0}
    roughness       Surface roughness in [0,1], can be anisotropic (S, S, angle)    {0}
    reflectivity    A scale of the Fresnel term. (S) [0,1]                          {1}
    displacement    Displacement map, must be a texture (S)                         {NONE}
    absorption      Physical absorption coefficient σ_a. (S) [/m]                   {0.5}
                    Gets multiplied with the albedo!
    scattering      Scattering coefficient σ_s. (S, RGB) [/m]                       {1e30}
                    Use very large numbers for opaque materials. 10^30 disables any
                    subsurface scattering (counts a s opaque).
    density         Scale of the two extinction coefficients. [0,1] (S)             {1.0}
                    Useful for non-homogeneous participating media.
    phase           Backward or forward scattering (Henyey-Greenstein) [-1,1] (S)   {0.0}

`general`: a more Disney-like approach with easy to setup parameters. This material is mapped to a physical one internally.

    color           Base color used to derive diffuse/spec color (RGB)              {0.5, 0.5, 0.5}
    metalness       Plastic like or metal like reflections [0,1] (S)                {0}
    roughness       Surface roughness in [0,1], can be anisotropic (S, S, angle)    {0}
    reflectivity    A scale of the Fresnel term. (S) [0,1]                          {1}
    emissivity      Exitant radiant energy (RGB) [cd/m^2]                           {0, 0, 0}
    transmissivity  Percentage of transmitted light to the back face (RGB)          {0}
                    This simulates transparent and two sided materials (e.g. leafs).
    subscattering   Scattering of transmitted light (kind of back-face roughness).  {0}
                    [0,1] (S)
    refractionIdxN  First part of complex valued refraction index. (S, RGB)         {1.3}
    displacement    Displacement map, must be a texture (S)                         {NONE}

`volumetric`: a pure volumetric material without a surface (e.g. fog)

    absorption      Physical absorption coefficient σ_a. (RGB) [%/m]                {10.0, 20.0, 40.0}
    scattering      Scattering coefficient σ_s. (RGB) [%/m]                         {100.0, 100.0, 100.0}
    density         Scale of the two coefficients. [0,1] (S)                        {1.0}
                    Useful for non-homogeneous media.
    phase           Backward or forward scattering (Henyey-Greenstein) [-1,1] (S)   {0.0}

`legacy`: Non-realistic model without energy preservation

    albedo          Lambertian diffuse color (RGB)                                  {0.5, 0.5, 0.5, 1.0}
                    May contain an opacity (alpha) channel.
    specularColor   A color for specular highlights. (RGB)                          {1.0, 1.0, 1.0}
    reflectivity    Isotropic amount of reflected light (offset term in Fresnel     {0.05}
                    approximations) [0,1] (S)
                    To avoid any reflections scale down the specularColor.
    roughness       Surface roughness in [0,1], can be anisotropic (S, S, angle)    {0.5, 0.5, 0}
	emissivity      Exitant radiant energy (RGB) [cd/m^2]                           {0, 0, 0}
	
`transparent`: Material for glass, water, wine, ...

	specularColor   A color for specular highlights. (RGB)                          {1.0, 1.0, 1.0}
	roughness       Surface roughness in [0,1], can be anisotropic (S, S, angle)    {0.5, 0.5, 0}
	reflectivity    Fresnel offset term F0.                                         {0.05}
	optDensity		Absorption coefficient (RGB) and real refraction index N.       {0, 0, 0, 1.3}

`thinLayer`: a material for leaves and foils. If transmitted the render expects to be in free space again instead of inside a model.

	backscatterColor   Procentual amount of back scattered light (if not reflected  {0.5, 0.5, 0.5}
                       specular before).
    transmittance      Procentual amount of transmitted light.                      {0.5, 0.5, 0.5}
                       backscatterColor + transmittance <= 1!
    roughnessUp        Surface roughness in normal direction. (S)                   {1.0}
    roughnessDown      Surface roughness opposed to the normal. (S)                 {1.0}
    scatteringStrength "Roughness" for transmitted light. (S)                       {1.0}

where `(S)` is a scalar, `(RGB)` is an RGB color in [0,1]^3, `[]` note value intervals or units and `{x}` the default values.
Every property is optional (its default is used if not given).

### "lights" ###
Each light has a set of mandatory defaults dependent on its type. There are no defaults.
Additionally, there is a list of flags in which configurations the light should be used. This allows to setup different lighting situations (day/night) for the same scene.

Light types are:

`point`:

    position
    intensity      [cd = lm / sr]

`lambert`:

    position
    intensity      [cd = lm / sr]
    normal

`directional`:

    direction	   Direction towards the light
    irradiance     [lm / m^2]

`spot`:

    position
    direction
    peakIntensity or intensity [cd = lm / sr]
    falloff
    halfAngle

`sky`:

    sunDirection   Direction of the sun (y is up)
    turbidity      A value greater 1 (usually in [2,10]). The larger the value the more
                   scattering is introduced.
    aerialPerspective true/false Enable depth dependent fog (atmospherical scattering).

`goniometric`:

    intensityMap    A single .dds or .ktx texture containing a cube map (HDR: [cd = lm/sr]).
	intensityScale	Unitless scaling factor which is multiplied with the value of the map.
    position

`environment`

    radianceMap         A single .dds or .ktx texture containing a cube map (HDR: [cd/m^2]).

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
