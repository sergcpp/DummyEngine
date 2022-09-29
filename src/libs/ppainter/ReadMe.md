# Pivot Painter for Blender
Pivot Painter is a Blender addon to create a 3d model, for use with Pivot Painter Tool's shaders in Unreal Engine 4.

The addon tries to mimic the basic function of the 3dsmaxscript that is based on, while missing many tools that would speed up the creation of a final mesh. As such the resulting mesh and texture, should be fully compatible with the build in material functions of the Pivot Painter Tool 2 shader. Therefore should refer to unreal engine documentation for [Pivot Painter Tool 2.0](https://docs.unrealengine.com/en-us/Engine/Content/Tools/PivotPainter)

Despite the name the addon does not make use of vertex paint that was used in Pivot Painter Tool 1, but is possible to recreate most of the examples from the pivot point 1 map in the ContentExamples.

The basic functionality of the pivot painter comes from the textures where the info gets stored. One UV layer is set to point each object to the specific pixel with their info.

This [video](https://youtu.be/63wU-zAbpNM) shows the motion that can be created.

## Getting Started
* Study the Content Examples.
* Install the add-on.
* See the example blend file.
* Read the tooltips of the addon for info.
* Find what textures the example you want to recreate needs. The names in the add-on are the same with the documentation in UE4(filenames differ). The Content Example uses different names.
* Set the blender scene units on centimeters.
* Create the mesh in separate parts.
* Depending on the texture set origin point, parent, selection order.
* Select all the objects and create the textures.
* Apply scale in the meshes if needed and export the meshes and the textures.
* In UE4 when you import the mesh enable Combine Meshes. See [UE4 documentation](https://docs.unrealengine.com/en-US/Engine/Content/Tools/PivotPainter/PivotPainter2#importingassets) for guide with images, ignore the optional step.
* For the textures set filter Nearest. If the texture is png also set compression VectorDisplacementMap and disable sRGB. (See UE4 documentation link above)
* Combine textures, materials and mesh.

For more detail instructions and examples see [here](https://drive.google.com/drive/folders/1OQ_DwbMsR2QZnrKk-4144nJPH4av3LB-?usp=sharing).

## Considerations
The tooltips of the addon contain important information than can help avoid problems. It is easy to miss them, keep an eye on them.

The addon to work, needs all the parts of the mesh to be separate objects, and necessary settings configured depending of the texture selection.
E.g. to create a tree you need to set prior the parent of each object and the origin point,  while to recreate the house(from the content examples) you need to set the selection order.

Most likely you will need X axis and X extent textures. These textures need proper objects rotations relative to the global axis. In case of a automatic creation(with no rotation) or applied rotation, you can try to use the experimental option to calculate X axis from the bounbbox.

The Save Textures to folder option will always overwrite if an other texture file has the same name.

Because the tool requires separate objects to work, in complex meshes the number of meshes can heavily impact the performance of the Blender UI.

It is not possible to recreate exactly the same, some of the examples from pivot painter 1. Pivot painter tool 1 provide custom alpha data, which this addon does not create. 
 
## Changelog

version 1.1.2
- fix Ydepth and Zheight functions.
- correct license message.
- remove warning from inside blender.

version 1.1.1
- removed warnings (Crashes with undo/redo of 2.80 rc2 have been fixed in the final release of 2.80)
- updated the version in the addon to run without modification.

version 1.1
- add support for 2.80. Had random crashes.
- removed unnecessary options (merge, keep original, merge levels)
- correct license to GPL (was the effective one based on the blender license)

version 1.0
- bumped version number, with the creation of the guide, this add-on is now complete.

version 0.9.2
- fixed a bug that prevented the automatic save of the textures to folder, expect a subfolder from the location of the blend file.
- fixed a bug that prevented the selection of the separate levels when calculating the X axis from boundbox.
- improved tooltips for better visibility.
- add little more info in the readme.

version 0.9.1
- add examples using pivot painter tool 1 examples.
- add info in the Gui showing when the operation is complete(tiny bar on top for 2 sec)

version 0.9
- add selection order
- add option to save textures to files
- add merging of levels
- add number of textures selection, up to 4
- add more checks to minimize errors 
- update the .blend file with more examples
- add UE4 project with the examples results
- fixed X Axis calculation from bound box
- fixed texture creation, when there are more than 256 objects
- improved UI
- corrected typo in license version

version 0.7
- initial release

### Links
 Repository https://github.com/Gvgeo/Pivot-Painter-for-Blender
 
 Forum https://forums.unrealengine.com/community/community-content-tools-and-tutorials/1472015-pivot-painter-for-blender

## Author

 *George Vogiatzis (Gvgeo)*

## License

This project is Licensed under the GPL v3.0, see the [LICENSE.txt](LICENSE.txt) file for details.

## Acknowledgment

* This addon is based on the 3dsMaxScript Pivot Painter version 2.0 written by Jonathan Lindquist at Epic Games.
