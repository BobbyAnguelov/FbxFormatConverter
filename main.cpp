#include <fbxsdk.h>
#include <windows.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <shellapi.h>
#include <assert.h>
#include <functional>
#include <algorithm>

#if _MSC_VER
#pragma warning(push, 0)
#pragma warning(disable: 4702)
#endif

// Note: this has been modified for this application
#include "cmdParser.h"

#if _MSC_VER
#pragma warning(pop)
#endif

//-------------------------------------------------------------------------

enum class FileFormat
{
    Unknown,
    Binary,
    Ascii
};

//-------------------------------------------------------------------------

namespace FileSystemHelpers
{
    static std::string GetFullPathString( char const* pPath )
    {
        assert( pPath != nullptr && pPath[0] != 0 );

        char fullpath[256] = { 0 };
        DWORD length = GetFullPathNameA( pPath, 256, fullpath, nullptr );
        assert( length != 0 && length != 255 );

        // We always append the trailing slash to simplify further operations
        DWORD const result = GetFileAttributesA( fullpath );
        if ( result != INVALID_FILE_ATTRIBUTES && ( result & FILE_ATTRIBUTE_DIRECTORY ) && fullpath[length - 1] != '\\' )
        {
            fullpath[length] = '\\';
            fullpath[length + 1] = 0;
        }

        return std::string( fullpath );
    }

    static std::string GetFullPathString( std::string const& path )
    {
        return GetFullPathString( path.c_str() );
    }

    static std::string GetParentDirectoryPath( std::string const& path )
    {
        std::string dirPath;
        size_t const lastSlashIdx = path.rfind( '\\' );
        if ( lastSlashIdx != std::string::npos )
        {
            dirPath = path.substr( 0, lastSlashIdx + 1 );
        }

        return dirPath;
    }

    static bool IsValidDirectoryPath( std::string const& directoryPath )
    {
        DWORD const result = GetFileAttributesA( directoryPath.c_str() );
        if ( result != INVALID_FILE_ATTRIBUTES && ( result & FILE_ATTRIBUTE_DIRECTORY ) )
        {
            return true;
        }

        return false;
    }

    static FileFormat GetFileFormat( std::string const& filePath )
    {
        FileFormat fileFormat = FileFormat::Unknown;

        FILE* fp = nullptr;
        int errcode = fopen_s( &fp, filePath.c_str(), "r" );
        if ( errcode != 0 )
        {
            return fileFormat;
        }

        //-------------------------------------------------------------------------

        fseek( fp, 0, SEEK_END );
        size_t filesize = (size_t) ftell( fp );
        fseek( fp, 0, SEEK_SET );

        void* pFileData = malloc( filesize );
        size_t readLength = fread( pFileData, 1, filesize, fp );
        fclose( fp );

        //-------------------------------------------------------------------------

        // Ascii files cannot contain the null character
        if ( memchr( pFileData, '\0', readLength ) != NULL )
        {
            fileFormat = FileFormat::Binary;
        }
        else
        {
            fileFormat = FileFormat::Ascii;
        }

        return fileFormat;
    }

    static void GetDirectoryContents( std::string const& directoryPath, std::vector<std::string>& directoryContents )
    {
        if ( !IsValidDirectoryPath( directoryPath ) )
        {
            printf( "Error! %s is not a valid directory!", directoryPath.c_str() );
            return;
        }

        //-------------------------------------------------------------------------

        std::string const directorySearchPath = directoryPath + "*";

        //-------------------------------------------------------------------------

        WIN32_FIND_DATAA findData;
        HANDLE foundFileHandle = FindFirstFileA( directorySearchPath.c_str(), &findData );
        assert( foundFileHandle != INVALID_HANDLE_VALUE );

        //-------------------------------------------------------------------------

        char stringBuffer[1024] = { 0 };

        do
        {
            if ( strcmp( findData.cFileName, "." ) == 0 || strcmp( findData.cFileName, ".." ) == 0 )
            {
                continue;
            }

            if ( findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
            {
                sprintf_s( stringBuffer, 1024, "%s%s\\", directoryPath.c_str(), findData.cFileName );
                GetDirectoryContents( stringBuffer, directoryContents );
            }
            else
            {
                sprintf_s( stringBuffer, 1024, "%s%s", directoryPath.c_str(), findData.cFileName );
                directoryContents.emplace_back( GetFullPathString( stringBuffer ) );
            }
        } while ( FindNextFileA( foundFileHandle, &findData ) != 0 );
    }

    static bool MakeDir( char const* pDirectoryPath )
    {
        assert( pDirectoryPath != nullptr );
        return SUCCEEDED( SHCreateDirectoryExA( nullptr, pDirectoryPath, nullptr ) );
    }
}

//-------------------------------------------------------------------------

class FbxConverter
{
public:

    FbxConverter()
        : m_pManager( FbxManager::Create() )
    {
        assert( m_pManager != nullptr );
        auto pIOPluginRegistry = m_pManager->GetIOPluginRegistry();

        // Find the IDs for the ascii and binary writers
        int const numWriters = pIOPluginRegistry->GetWriterFormatCount();
        for ( int i = 0; i < numWriters; i++ )
        {
            if ( pIOPluginRegistry->WriterIsFBX( i ) )
            {
                char const* pDescription = pIOPluginRegistry->GetWriterFormatDescription( i );
                if ( strcmp( pDescription, "FBX binary (*.fbx)" ) == 0 )
                {
                    const_cast<int&>( m_binaryWriteID ) = i;
                }
                else if ( strcmp( pDescription, "FBX ascii (*.fbx)" ) == 0 )
                {
                    const_cast<int&>( m_asciiWriterID ) = i;
                }
            }
        }

        //-------------------------------------------------------------------------

        // This should never occur but I'm leaving it here in case someone updates the plugin with a new SDK and names change
        assert( m_binaryWriteID != -1 && m_asciiWriterID != -1 );
    }

    ~FbxConverter()
    {
        m_pManager->Destroy();
        m_pManager = nullptr;
    }

    int ConvertFbxFile( std::string const& inputFilepath, std::string const& outputFilepath, FileFormat outputFormat )
    {
        // Import
        //-------------------------------------------------------------------------

        FbxImporter* pImporter = FbxImporter::Create( m_pManager, "FBX Importer" );
        if ( !pImporter->Initialize( inputFilepath.c_str(), -1, m_pManager->GetIOSettings() ) )
        {
            printf( "Error! Failed to load specified FBX file ( %s ): %s\n\n", inputFilepath.c_str(), pImporter->GetStatus().GetErrorString() );
            return 1;
        }

        auto pScene = FbxScene::Create( m_pManager, "ImportScene" );
        if ( !pImporter->Import( pScene ) )
        {
            printf( "Error! Failed to import scene from file ( %s ): %s\n\n", inputFilepath.c_str(), pImporter->GetStatus().GetErrorString() );
            pImporter->Destroy();
            return 1;
        }
        pImporter->Destroy();

        // Set output format
        //-------------------------------------------------------------------------

        int const fileFormatIDToUse = ( outputFormat == FileFormat::Binary ) ? m_binaryWriteID : m_asciiWriterID;

        // Export
        //-------------------------------------------------------------------------

        std::string const parentDirPath = FileSystemHelpers::GetParentDirectoryPath( outputFilepath );
        if ( !FileSystemHelpers::MakeDir( parentDirPath.c_str() ) )
        {
            printf( "Error! Failed to create output directory (%s)!\n\n", outputFilepath.c_str() );
        }

        //-------------------------------------------------------------------------

        FbxExporter* pExporter = FbxExporter::Create( m_pManager, "FBX Exporter" );
        if ( !pExporter->Initialize( outputFilepath.c_str(), fileFormatIDToUse, m_pManager->GetIOSettings() ) )
        {
            printf( "Error! Failed to initialize exporter: %s\n\n", pExporter->GetStatus().GetErrorString() );
            return 1;
        }

        if ( pExporter->Export( pScene ) )
        {
            printf( "Success!\nIn: %s \nOut (%s): %s\n\n", inputFilepath.c_str(), outputFormat == FileFormat::Binary ? "binary" : "ascii", outputFilepath.c_str() );
        }
        else
        {
            printf( "Error! File export failed: - %s\n\n", pExporter->GetStatus().GetErrorString() );
        }

        pExporter->Destroy();

        //-------------------------------------------------------------------------

        return 0;
    }

    bool IsFbxFile( std::string const& inputFilepath )
    {
        assert( !inputFilepath.empty() );
        int readerID = -1;
        auto pIOPluginRegistry = m_pManager->GetIOPluginRegistry();
        pIOPluginRegistry->DetectReaderFileFormat( inputFilepath.c_str(), readerID );
        return pIOPluginRegistry->ReaderIsFBX( readerID );
    }

private:

    FbxConverter( FbxConverter const& ) = delete;
    FbxConverter& operator=( FbxConverter const& ) = delete;

private:

    FbxManager*             m_pManager = nullptr;
    int const               m_binaryWriteID = -1;
    int const               m_asciiWriterID = -1;
};

//-------------------------------------------------------------------------

static void PrintErrorAndHelp( char const* pErrorMessage = nullptr )
{
    printf( "================================================\n" );
    printf( "FBX File Format Converter\n" );
    printf( "================================================\n" );
    printf( "2020 - Bobby Anguelov - MIT License\n\n" );

    if ( pErrorMessage != nullptr )
    {
        printf( "Error! %s\n\n", pErrorMessage );
    }

    printf( "Convert: -c <path> [-o <output path>] {-binary|-ascii}\n" );
    printf( "Query: -q <path>\n" );
}

static void PrintFileFormat( std::string const& filePath )
{
    FileFormat const fileFormat = FileSystemHelpers::GetFileFormat( filePath.c_str() );

    if ( fileFormat == FileFormat::Binary )
    {
        printf( "%s - binary\n", filePath.c_str() );
    }
    else if ( fileFormat == FileFormat::Ascii )
    {
        printf( "%s - ascii\n", filePath.c_str() );
    }
    else
    {
        printf( "%s doesnt exist or is not an FBX file!\n", filePath.c_str() );
    }
}

//-------------------------------------------------------------------------

int main( int argc, char* argv[] )
{
    cli::Parser cmdParser( argc, argv );
    cmdParser.disable_help();
    cmdParser.set_optional<std::string>( "c", "convert", "" );
    cmdParser.set_optional<std::string>( "o", "output", "" );
    cmdParser.set_optional<std::string>( "q", "query", "" );
    cmdParser.set_optional<bool>( "binary", "", false, ""  );
    cmdParser.set_optional<bool>( "ascii", "", false, "" );

    if ( cmdParser.run() )
    {
        FbxConverter fbxConverter;

        //-------------------------------------------------------------------------

        auto inputConvertPath = cmdParser.get<std::string>( "c" );
        if ( !inputConvertPath.empty() )
        {
            bool const outputAsBinary = cmdParser.get<bool>( "binary" );
            bool const outputAsAscii = cmdParser.get<bool>( "ascii" );

            if ( outputAsAscii && outputAsBinary )
            {
                PrintErrorAndHelp( "Having both -ascii and -binary arguments is not allowed." );
            }
            else if ( !outputAsAscii && !outputAsBinary )
            {
                PrintErrorAndHelp( "Either -ascii or -binary required!" );
            }
            else
            {
                FileFormat const outputFormat = outputAsBinary ? FileFormat::Binary : FileFormat::Ascii;

                inputConvertPath = FileSystemHelpers::GetFullPathString( inputConvertPath );
                if ( FileSystemHelpers::IsValidDirectoryPath( inputConvertPath ) )
                {
                    std::vector<std::string> directoryContents;
                    FileSystemHelpers::GetDirectoryContents( inputConvertPath, directoryContents );

                    auto outputPath = cmdParser.get<std::string>( "o" );
                    if ( outputPath.empty() )
                    {
                        for ( auto& filePath : directoryContents )
                        {
                            if ( !fbxConverter.IsFbxFile( filePath.c_str() ) )
                            {
                                continue;
                            }

                            fbxConverter.ConvertFbxFile( filePath.c_str(), filePath.c_str(), outputFormat );
                        }

                        return 0;
                    }
                    else // Convert and copy
                    {
                        outputPath = FileSystemHelpers::GetFullPathString( outputPath );

                        for ( auto& filePath : directoryContents )
                        {
                            if ( !fbxConverter.IsFbxFile( filePath ) )
                            {
                                continue;
                            }

                            std::string newOutputPath = filePath;
                            newOutputPath.replace( 0, inputConvertPath.length() - 1, outputPath.c_str() );
                            fbxConverter.ConvertFbxFile( filePath, newOutputPath, outputFormat );
                        }

                        return 0;
                    }
                }
                else
                {
                    auto outputPath = cmdParser.get<std::string>( "o" );
                    if ( outputPath.empty() )
                    {
                        return fbxConverter.ConvertFbxFile( inputConvertPath, inputConvertPath, outputFormat );
                    }
                    else
                    {
                        outputPath = FileSystemHelpers::GetFullPathString( outputPath );
                        return fbxConverter.ConvertFbxFile( inputConvertPath, outputPath, outputFormat );
                    }
                }
            }
        }
        else // check for query cmd line arg
        {
            auto inputQueryPath = cmdParser.get<std::string>( "q" );
            if ( !inputQueryPath.empty() )
            {
                inputQueryPath = FileSystemHelpers::GetFullPathString( inputQueryPath );
                if ( FileSystemHelpers::IsValidDirectoryPath( inputQueryPath ) )
                {
                    std::vector<std::string> directoryContents;
                    FileSystemHelpers::GetDirectoryContents( inputQueryPath, directoryContents );

                    for ( auto& filePath : directoryContents )
                    {
                        if ( !fbxConverter.IsFbxFile( filePath ) )
                        {
                            continue;
                        }

                        PrintFileFormat( filePath );
                    }
                }
                else 
                {
                    PrintFileFormat( inputQueryPath );
                }
            }
            else
            {
                PrintErrorAndHelp( "Invalid Arguments!" );
            }
        }

        return 0;
    }
    else
    {
        PrintErrorAndHelp();
    }

    return 1;
}