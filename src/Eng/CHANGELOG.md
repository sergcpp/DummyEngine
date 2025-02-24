# Changelog

## [Unreleased]

### Added

### Fixed

### Changed

### Removed

## [0.2.1] - 2025-02-24

### Added

- Approximate transparency in RT shaders
- Multi-threaded pipeline initialization

### Fixed

- Inactive probes near transparent objects
- Noisy recursive reflections
- GI cache flickering
- SSAO artifacts
- Black splotches in GI
- Occasional texture streaming crash

### Changed

- Improved denoising
- Improved screen-space shadow (two samplers approach)
- GTAO is done in half-res on medium/high

## [0.2.0] - 2024-11-02

### Added

- Emissives support
- Probe-based cache for multibounce GI
- Multibounce specular GI
- Order-independent transparency
- Procedural sky
- Jittered shadows for screenshots
- Autoexposure
- GTAO
- Bloom
- Chromatic aberraction
- Motion blur
- Diffuse/specular light influence flags
- Ability to skip multi_compile shader combinations
- Approximate LTC portal lights
- Spotlight support
- HDRI light (without NEE for now)
- Gaussian and Blackman-Harris image filters
- Visibility flags
- Backside material support
- Gamma parameter for tonemapping
- GLTF meshes conversion
- Small-scale screen space shadow

### Fixed

- Shadow BVH flickering
- TAA flicker at high exposure
- Missing subgroup extensions support check
- Non-bindless OpenGL mode
- Incorrect GPU timestamps
- Specular shimmering
- AMD/Arc issues loading sponza scene
- Cascaded shadow bugs
- Artifacts on mirror surfaces
- SSR black contours
- Material override only working on unique meshes
- Imprecise default normalmap
- Incorrect specular denoising reprojection (OGL only)
- Incorrect normal transform in RT shaders
- Incorrect streaming of non-mipmapped textures

### Changed

- Depth buffer uses reversed Z
- Improved blur-based denoising
- Better VNDF sampling is used for GGX
- Improved PCSS shadow filtering
- Catmull-Rom filter is used for TAA
- Velocity buffer is 2.5D now (3 channels)
- Simpler shading is used in RT shaders
- Improved memory-level aliasing is used for FG resources
- Faster SWRT
- Separate debug version of shaders is used in debug mode
- Alpha test uses separate texture masks
- Texture LOD bias is used for TAA compensation

### Removed

- SPIRV OpenGL support
- Smooth texture LOD transition

## [0.1.0] - 2024-02-29

### Added

- LTC area lights
- Static TAA mode
- Lighting support in RT shaders
- Software raytracing fallback
- BC4/BC5 texture compression
- Gbuffer normals dithering
- Hacky ambient term for GI
- LUT tonemapping (must be provided externally)
- Freelist allocator
- Graphical tests

### Fixed

- Color banding artifacts
- SSR artifacts on Intel GPUs
- Incorrect SSR ray length

### Changed

- Improved reflections denoising
- OGL shader permutations preprocessing
- Assets preprocessing is multithreaded
- Renderer frontend is partially parallelized

### Removed

- LinearAlloc


[Unreleased]: https://github.com/sergcpp/Eng/compare/v0.2.1...master
[0.2.1]: https://github.com/sergcpp/Eng/releases/v0.2.1
[0.2.0]: https://github.com/sergcpp/Eng/releases/v0.2.0
[0.1.0]: https://github.com/sergcpp/Eng/releases/v0.1.0
