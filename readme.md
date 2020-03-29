# Fbx Format Converter

This project allows you to convert binary fbx files to asciis and vice versa. This is especially useful when trying to import fbx files into blender since blender cannot read ascii FBX files.

## Features

* Single file conversion between binary and ascii
* Batch folder conversion
* Single file/folder query

## To build:

* You need to have the FBX SDK installed (https://www.autodesk.com/developer-network/platform-technologies/fbx-sdk-2020-0)

* Open the FbxFormatConverter.props file and change the FBX_SDK_DIR macro to point to the FBXSDK install directory.

* Open the sln file using visual studio and hit build.

## Conversion:

If you want to convert an ascii file into a binary one or vice versa.

`FbxFormatConverter.exe -c <filepath|folderpath> [-o <filepath|folderpath>] {-ascii|-binary}`

* -c : convert the file/folder specified
* -o : (optional) the outputpath for the converted files, if not supplied then the source file will be overwritten
* -binary/-ascii : the required output file format. Only one is allowed.

## Query:

If you want to find out if an FBX file is an ascii or a binary file.

`FbxFormatConverter.exe -q <filepath|folderpath>`

* -q : query the file/folder specified

## Examples

If you want to covert file "anim_temp_final_0_v2.fbx" to binary.

`FbxFormatConverter.exe -c "c:\anim_temp_final_0_v2.fbx" -binary`

If you want to convert all the files in folder a to ascii and store the converted files in folder b:

`FbxFormatConverter.exe -c "c:\a" -o "c:\b" -ascii`

If you want to know if file "dancingbaby.fbx" is a binary file.

`FbxFormatConverter.exe -q "c:\dancingbaby.fbx"`

## Notes:

This project uses CmdParser ( https://github.com/FlorianRappl/CmdParser )