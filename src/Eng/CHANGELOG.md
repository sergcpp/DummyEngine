# Changelog

## [Unreleased]

### Added

### Fixed

### Changed

### Removed


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


[Unreleased]: https://github.com/sergcpp/Eng/compare/v0.1.0...master
[0.1.0]: https://github.com/sergcpp/Eng/releases/v0.1.0
