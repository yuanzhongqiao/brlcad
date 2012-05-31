/*
* NIST STEP Core Class Library
* cleditor/STEPfile.inline.cc
* April 1997
* Peter Carr
* K. C. Morris
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

#include <STEPfile.h>
#include <SdaiHeaderSchema.h>
#include <STEPaggregate.h>
#include <cmath>

#include <cstring>

extern void HeaderSchemaInit( Registry & reg );

//To Be inline functions

//constructor & destructor

STEPfile::STEPfile( Registry & r, InstMgr & i, const char * filename )
    :
    _reg( r ), _instances( i ),
      _headerId( 0 ), _maxErrorCount( 5000 ),
      _fileName( 0 ), _entsNotCreated( 0 ), _entsInvalid( 0 ),
      _entsIncomplete( 0 ), _entsWarning( 0 ),
      _errorCount( 0 ), _warningCount( 0 ),
      _fileIdIncr( 0 )

{
    SetFileType( VERSION_CURRENT );
    SetFileIdIncrement();
    _currentDir = new DirObj( "" );
    _headerRegistry = new Registry( HeaderSchemaInit );
    _headerInstances = new InstMgr;
    if( filename ) {
        ReadExchangeFile( filename );
    }
}

STEPfile::~STEPfile() {
    delete _currentDir;

    // remove everything from the Registry before deleting it
    _headerRegistry->DeleteContents();
    delete _headerRegistry;

    _headerInstances->DeleteInstances();
    delete _headerInstances;
}

int STEPfile::SetFileType( FileTypeCode ft ) {
    FileType( ft );

    switch( _fileType ) {
        case( VERSION_OLD ):
            ENTITY_NAME_DELIM = '@';
            FILE_DELIM = "STEP;";
            END_FILE_DELIM = "ENDSTEP;";
            break;
        case( VERSION_UNKNOWN ):
        case( VERSION_CURRENT ):
            ENTITY_NAME_DELIM = '#';
            FILE_DELIM = "ISO-10303-21;";
            END_FILE_DELIM = "END-ISO-10303-21;";
            break;
        case( WORKING_SESSION ):
            ENTITY_NAME_DELIM = '#';
            FILE_DELIM = "STEP_WORKING_SESSION;";
            END_FILE_DELIM = "END-STEP_WORKING_SESSION;";
            break;

        default:
            // some kind of error
            cerr << "Internal error:  " << __FILE__ <<  __LINE__
                 << "\n" << _POC_ "\n";
            return 0;
    }
    return 1;
}


/******************************************************/
const char * STEPfile::TruncFileName( const char * filename ) const {
    const char * tmp = strrchr( filename, '/' );
    if( tmp ) {
        return tmp++;
    } else {
        return filename;
    }

}


/******************************************************/
Severity STEPfile::ReadExchangeFile( const char * filename, int useTechCor ) {
    _error.ClearErrorMsg();
    _errorCount = 0;
    istream * in = OpenInputFile( filename );
    if( _error.severity() < SEVERITY_WARNING ) {
        CloseInputFile( in );
        return _error.severity();
    }

    instances().ClearInstances();
    if( _headerInstances ) {
        _headerInstances->ClearInstances();
    }
    _headerId = 5;
    Severity rval = AppendFile( in, useTechCor );
    CloseInputFile( in );
    return rval;
}

Severity STEPfile::AppendExchangeFile( const char * filename, int useTechCor ) {
    _error.ClearErrorMsg();
    _errorCount = 0;
    istream * in = OpenInputFile( filename );
    if( _error.severity() < SEVERITY_WARNING ) {
        CloseInputFile( in );
        return _error.severity();
    }
    Severity rval = AppendFile( in, useTechCor );
    CloseInputFile( in );
    return rval;
}

Severity STEPfile::ReadWorkingFile( const char * filename, int useTechCor ) {
    _error.ClearErrorMsg();
    _errorCount = 0;
    istream * in = OpenInputFile( filename );
    if( _error.severity() < SEVERITY_WARNING ) {
        CloseInputFile( in );
        return _error.severity();
    }

    instances().ClearInstances();
    _headerInstances->ClearInstances();
    SetFileType( WORKING_SESSION );

    Severity rval = AppendFile( in, useTechCor );
    SetFileType();
    CloseInputFile( in );
    return rval;
}


Severity STEPfile::AppendWorkingFile( const char * filename, int useTechCor ) {
    _error.ClearErrorMsg();
    _errorCount = 0;
    istream * in = OpenInputFile( filename );
    if( _error.severity() < SEVERITY_WARNING ) {
        CloseInputFile( in );
        return _error.severity();
    }
    SetFileType( WORKING_SESSION );
    Severity rval = AppendFile( in, useTechCor );
    SetFileType();
    CloseInputFile( in );
    return rval;
}

istream * STEPfile::OpenInputFile( const char * filename ) {
    //  if there's no filename to use, fail
    if( !( strcmp( filename, "" ) || strcmp( FileName(), "" ) ) ) {
        _error.AppendToUserMsg( "Unable to open file for input. No current file name.\n" );
        _error.GreaterSeverity( SEVERITY_INPUT_ERROR );
        return ( 0 );
    } else {
        if( !SetFileName( filename ) && ( strcmp( filename, "-" ) != 0 ) ) {
            char msg[BUFSIZ];
            sprintf( msg, "Unable to find file for input: \'%s\'. File not read.\n", filename );
            _error.AppendToUserMsg( msg );
            _error.GreaterSeverity( SEVERITY_INPUT_ERROR );
            return ( 0 );
        }
    }

    std::istream * in;

    if( strcmp( filename, "-" ) == 0 ) {
        in = &std::cin;
    } else {
        in = new ifstream( FileName() );
    }

    if( !in || !( in -> good() ) ) {
        char msg[BUFSIZ];
        sprintf( msg, "Unable to open file for input: \'%s\'. File not read.\n", filename );
        _error.AppendToUserMsg( msg );
        _error.GreaterSeverity( SEVERITY_INPUT_ERROR );
        return ( 0 );
    }

    return in;
}

void STEPfile::CloseInputFile( istream * in ) {
    if( in && *in != std::cin ) {
        delete in;
    }
}

ofstream * STEPfile::OpenOutputFile( const char * filename ) {
    if( !filename ) {
        if( !FileName() ) {
            _error.AppendToUserMsg( "No current file name.\n" );
            _error.GreaterSeverity( SEVERITY_INPUT_ERROR );
        }
    } else {
        if( !SetFileName( filename ) ) {
            char msg[BUFSIZ];
            sprintf( msg, "can't find file: %s\nFile not written.\n", filename );
            _error.AppendToUserMsg( msg );
            _error.GreaterSeverity( SEVERITY_INPUT_ERROR );
        }
    }

    if( _currentDir->FileExists( TruncFileName( FileName() ) ) ) {
        MakeBackupFile();
    }

    ofstream * out  = new ofstream( FileName() );
    // default for ostream is writeonly and protections are set to 644
    if( !out ) {
        _error.AppendToUserMsg( "unable to open file for output\n" );
        _error.GreaterSeverity( SEVERITY_INPUT_ERROR );
    }
    return out;
}

void STEPfile::CloseOutputFile( ostream * out ) {
    delete out;
}

int STEPfile::IncrementFileId( int fileid ) {
    return ( fileid + FileIdIncr() );
}


void STEPfile::SetFileIdIncrement() {
    if( instances().MaxFileId() < 0 ) {
        _fileIdIncr = 0;
    } else {
        _fileIdIncr = ( int )( ( ceil( ( instances().MaxFileId() + 99.0 ) / 1000.0 ) + 1.0 ) * 1000.0 );
    }
}

/**
 * Returns the schema name from the file schema header section (or the 1st
 * one if more than one exists).  Copies this value into schName.  If there
 * is no header section or no value for file schema, NULL is returned and
 * schName is unset.
 */
std::string STEPfile::schemaName() {
    SdaiFile_schema * fs;
    std::string schName;
    STEPnode * n;

    if( _headerInstances == NULL ) {
        return schName;
    }
    fs = ( SdaiFile_schema * )_headerInstances->GetApplication_instance( "File_Schema" );
    if( fs == ENTITY_NULL ) {
        return schName;
    }

    n = ( STEPnode * )fs->schema_identifiers_()->GetHead();
    // (take the first one)
    if( n == NULL ) {
        return schName;
    }
    n->STEPwrite( schName );
    if( schName.empty() || schName[0] == '$' ) {
        schName.clear();
        return schName;
    } else if( schName[0] == '\0' ) {
        //probably never - it seems that putting null in std::string takes effort
        _error.AppendToUserMsg( "In STEPfile::schemaName: schName contains \\0 - it should be empty." );
        _error.GreaterSeverity( SEVERITY_WARNING );
        schName.clear();
        return schName;
    }
    if( schName[ schName.length() - 1 ] == '\'' ) {
        schName = schName.substr( 1, schName.length() - 2 );
    } else {
        _error.AppendToUserMsg( "In STEPfile::schemaName: schName was truncated." );
        _error.GreaterSeverity( SEVERITY_WARNING );

        schName = schName.substr( 1, schName.length() - 1 );
    }
    return schName;
}