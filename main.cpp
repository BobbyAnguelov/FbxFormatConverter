#include <fbxsdk.h>
#include <windows.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <shellapi.h>

#if _MSC_VER
#pragma warning(push, 0)
#pragma warning(disable: 4702)
#endif

#include "cmdParser.h"
#include <assert.h>

#if _MSC_VER
#pragma warning(pop)
#endif

//-------------------------------------------------------------------------

enum class OutputFormat
{
    Unknown,
    Binary,
    Ascii
};

static std::string GetFullPathString( char const* pPath )
{
    assert( pPath != nullptr && pPath[0] != 0 );

    char fullpath[256] = { 0 };
    DWORD length = GetFullPathNameA( pPath, 256, fullpath, nullptr );
    assert( length != 0 && length != 255 );

    // Ensure directory paths have the final slash appended
    DWORD const result = GetFileAttributesA( fullpath );
    if ( result != INVALID_FILE_ATTRIBUTES && ( result & FILE_ATTRIBUTE_DIRECTORY ) && fullpath[length - 1] != '\\' )
    {
        fullpath[length] = '\\';
        fullpath[length + 1] = 0;
    }

    return std::string( fullpath );
}

//-------------------------------------------------------------------------

int ConvertFbxFile( char const* pInputFilepath, char const* pOutputFilepath, OutputFormat format )
{
    FbxManager* pManager = FbxManager::Create();

    // Import
    //-------------------------------------------------------------------------

    FbxImporter* pImporter = FbxImporter::Create( pManager, "FBX Importer" );
    if ( !pImporter->Initialize( pInputFilepath, -1, pManager->GetIOSettings() ) )
    {
        printf( " >> Error! Failed to load specified FBX file ( %s ): %s\n", pInputFilepath, pImporter->GetStatus().GetErrorString() );
        return 1;
    }

    printf( " >> File opened successfully! - %s\n", pInputFilepath );

    auto pScene = FbxScene::Create( pManager, "ImportScene" );
    if ( !pImporter->Import( pScene ) )
    {
        printf( " >> Error! Failed to load specified FBX file ( %s ): %s\n", pInputFilepath, pImporter->GetStatus().GetErrorString() );
        pImporter->Destroy();
        return 1;
    }
    pImporter->Destroy();

    printf( " >> File imported successfully!\n" );

    // Set output format
    //-------------------------------------------------------------------------

    auto pIOPluginRegistry = pManager->GetIOPluginRegistry();
    
    int binaryFileFormatID = -1;
    int asciiFileFormatID = -1;

    // Find the IDs for the ascii and binary writers
    int const numWriters = pIOPluginRegistry->GetWriterFormatCount();
    for ( int i = 0; i < numWriters; i++ )
    {
        if ( pIOPluginRegistry->WriterIsFBX( i ) )
        {
            char const* pDescription = pIOPluginRegistry->GetWriterFormatDescription( i );
            if ( strstr( pDescription, "FBX binary" ) != nullptr )
            {
                binaryFileFormatID = i;
            }
            else if ( strstr( pDescription, "FBX ascii" ) != nullptr )
            {
                asciiFileFormatID = i;
            }
        }
    }

    // This should never occur but I'm leaving it here in case someone updates the plugin with a new SDK and names change
    if ( binaryFileFormatID == -1 || asciiFileFormatID == -1 )
    {
        printf( " >> Error! Failed to determine FBX ascii/binary format IDs\n" );
        return 1;
    }

    int const fileFormatIDToUse = ( format == OutputFormat::Binary ) ? binaryFileFormatID : asciiFileFormatID;

    // Export
    //-------------------------------------------------------------------------

    FbxExporter* pExporter = FbxExporter::Create( pManager, "FBX Exporter" );
    if ( !pExporter->Initialize( pOutputFilepath, fileFormatIDToUse, pManager->GetIOSettings() ) )
    {
        printf( " >> Error! Failed to initialize exporter: %s\n", pExporter->GetStatus().GetErrorString() );
        return 1;
    }

    if ( pExporter->Export( pScene ) )
    {
        printf( " >> File exported Successfully! - %s\n\n", pOutputFilepath );
    }
    else
    {
        printf( " >> File export failed: - %s\n\n", pExporter->GetStatus().GetErrorString() );
    }

    pExporter->Destroy();

    // Cleanup
    //-------------------------------------------------------------------------

    pManager->Destroy();

    return 0;
}

//-------------------------------------------------------------------------

int main( int argc, char* argv[] )
{
    printf( " ================================================\n" );
    printf( "  FBX File Format Converter\n" );
    printf( " ================================================\n" );
    printf( "  Usage: FbxFormatConverter.exe -i \"inputfile.fbx\" -o \"outputfile.fbx\" -f <binary|ascii>\n" );
    printf( " ------------------------------------------------\n\n" );

    //-------------------------------------------------------------------------

    cli::Parser cmdParser( argc, argv );
    cmdParser.disable_help();
    cmdParser.set_required<std::string>( "i", "infile", "" );
    cmdParser.set_required<std::string>( "o", "outfile", "" );
    cmdParser.set_required<std::string>( "f", "format", "" );

    if ( cmdParser.run() )
    {
        auto inputFile = cmdParser.get<std::string>( "i" );
        inputFile = GetFullPathString( inputFile.c_str() );

        //-------------------------------------------------------------------------

        auto outputFile = cmdParser.get<std::string>( "o" );
        outputFile = GetFullPathString( outputFile.c_str() );

        //-------------------------------------------------------------------------

        if ( inputFile == outputFile )
        {
            printf( " >> Error! Input file (%s) cannot be the same as the output file (%s)!\n\n", inputFile.c_str(), outputFile.c_str() );
            return 1;
        }

        //-------------------------------------------------------------------------

        OutputFormat outputFormat = OutputFormat::Unknown;

        auto const format = cmdParser.get<std::string>( "f" );
        if ( format == "binary" )
        {
            outputFormat = OutputFormat::Binary;
        }
        else if ( format == "ascii" )
        {
            outputFormat = OutputFormat::Ascii;
        }
        else
        {
            printf( " >> Error! Invalid parameter for -f, either 'binary' or 'ascii' expected!\n\n" );
            return 1;
        }

        //-------------------------------------------------------------------------

        return ConvertFbxFile( inputFile.c_str(), outputFile.c_str(), outputFormat );
    }

    return 1;
}