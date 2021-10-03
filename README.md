# DummyEngine [![Build status](https://gitlab.com/sergcpp/DummyEngine/badges/master/pipeline.svg)](https://gitlab.com/sergcpp/DummyEngine)
Small cross platform Vulkan/OpenGL 3d engine for personal experimentation.

- (Old) Video: <https://youtu.be/ujMV9O58uqc>

<div>
<div float="center">
  <img src="/img9.jpg" width="95.5%" />
</div>
<div float="center">
  <img src="/img7.jpg" width="47.5%" />
  <img src="/img8.jpg" width="47.5%" />
</div>
<div float="center">
  <img src="/img2.jpg" width="50%" />
  <img src="/anim1.gif" width="45%" />
</div>
</div>

Links to used free 3d models: [Bistro](<https://developer.nvidia.com/orca/amazon-lumberyard-bistro>), [People](<https://renderpeople.com/free-3d-people/>), [Living Room](<https://gumroad.com/davzeppelin#DSFfy>), [Wolf](<https://free3d.com/3d-model/wolf-rigged-and-game-ready-42808.html>)

## Features
  - Frame graph with semi-automatic barrier placement (inspired by https://www.slideshare.net/DICEStudio/framegraph-extensible-rendering-architecture-in-frostbite)
  - Dynamic BVH as main scene structure (http://box2d.org/files/GDC2019/ErinCatto_DynamicBVH_GDC2019.pdf)
  - Clustered forward renderer (inspired by http://www.humus.name/Articles/PracticalClusteredShading.pdf)
  - Shadow map atlas with caching
  - Screen space reflections and reflections from probes
  - Occlusion culling based on software rasterization (implementation of https://software.intel.com/content/dam/develop/external/us/en/documents/masked-software-occlusion-culling.pdf)
  - Pathtracer for global illumination baking and preview
  - DXT and ASTC texture formats, compact binary mesh format
  - Skinning using compute shader
  - Works on Windows (GL, VK), Linux (GL, VK), MacOS (VK through MoltenVK), Android (GL)
