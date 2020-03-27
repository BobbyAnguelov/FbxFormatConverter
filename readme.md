# Fbx Format Converter

This project allows you to convert binary fbx files to asciis and vice versa. This is especially useful when trying to import fbx files into blender since blender cannot read ascii FBX files.

## To build:

* You need to have the FBX SDK installed (https://www.autodesk.com/developer-network/platform-technologies/fbx-sdk-2020-0)

* Open the FbxFormatConverter.props file and change the FBX_SDK_DIR macro to point to the FBXSDK install directory.

* Open the sln file using visual studio and hit build.

## To use:

FbxFormatConverter.exe -i \"inputfile.fbx\" -o \"outputfile.fbx\" -f <binary|ascii>

    e.g. FbxFormatConverter.exe -i "Character.fbx" -o "CharacterAscii.fbx" -f ascii

## Notes:

This project uses CmdParser ( https://github.com/FlorianRappl/CmdParser )