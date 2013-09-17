//
//  Copyright (c) 2009, Tweak Software
//  All rights reserved.
// 
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions
//  are met:
//
//     * Redistributions of source code must retain the above
//       copyright notice, this list of conditions and the following
//       disclaimer.
//
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials
//       provided with the distribution.
//
//     * Neither the name of the Tweak Software nor the names of its
//       contributors may be used to endorse or promote products
//       derived from this software without specific prior written
//       permission.
// 
//  THIS SOFTWARE IS PROVIDED BY Tweak Software ''AS IS'' AND ANY EXPRESS
//  OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
//  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
//  ARE DISCLAIMED. IN NO EVENT SHALL Tweak Software BE LIABLE FOR
//  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
//  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
//  OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
//  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
//  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
//  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
//  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
//  DAMAGE.
//

#include "gtoReader.h"
#include "gtoHeader.h"
#include <Python.h>
#include <vector>

namespace PyGto {

static char ReaderDocString[] = 
"\n"
"The basic theory of operation is similar to how the C++ Gto::Reader class\n"
"works.  In your own code, you would create a class that is derived from\n"
"the gto.Reader class defined in this module.  You should implement the\n"
"'object', 'component', 'property', and 'dataRead' methods.\n"
"\n"
"There are a couple of major differences from the C++ implementation to\n"
"be aware of:\n"
"\n"
"1. The return value for the object, component, and property methods should\n"
"   evaluate to True if you want to read that property, or false/None if\n"
"   you don't.  There is no way to return a pointer/object and have it \n"
"   passed back to you when the data function is called.  It doesn't really\n"
"   make sense to do this in Python anyway.\n"
"\n"
"2. Only the dataRead method is implemented, as the data method doesn't\n"
"   make sense in Python.  As each property is read, a python tuple is\n"
"   automatically created and passed to dataRead.  Note that the prototype \n"
"   is slightly different here than in the Gto::Reader class:  I added the \n"
"   'name' argument for convenience sake.\n"
"\n";

// *****************************************************************************
// Converts from gto's enum-based types to their string representation
static const char *typeAsString( int type )
{
    switch( type )
    {
    case Gto::Int:
        return "int";
    case Gto::Float:
        return "float";
    case Gto::Double:
        return "double";
    case Gto::Half:
        return "half";
    case Gto::String:
        return "string";
    case Gto::Boolean:
        return "bool";
    case Gto::Short:
        return "short";
    case Gto::Byte:
        return "byte";
    default:
        return "unknown";
    }
}

// *****************************************************************************
// The next several functions implement the methods on our derived C++ 
// Gto::Reader class.  They get called by Gto::Reader::open(), and their main 
// purpose is to convert their arguments into Python objects and then call 
// their equivalent methods on the Python _gto.Reader class.
// *****************************************************************************

// *****************************************************************************
Reader::Reader( PyObject *callingInstance, unsigned int mode )
  : Gto::Reader( mode )
  , m_callingInstance( callingInstance )
{
    // Nothing
}

// *****************************************************************************
Reader::~Reader()
{
}

// *****************************************************************************
Request Reader::object( const std::string &name,
                        const std::string &protocol,
                        unsigned int protocolVersion,
                        const Gto::Reader::ObjectInfo &info )
{
    assert( m_callingInstance != NULL );

    // Build the Python equivalent of the Gto::ObjectInfo struct
    PyObject *oi = newObjectInfo( this, info );
    
    PyObject *returnValue = NULL;
    returnValue = PyObject_CallMethod( m_callingInstance,
                                       (char*)"object",
                                       (char*)"ssiO",
                                       name.c_str(),                
                                       protocol.c_str(),             
                                       protocolVersion,              
                                       oi );         
    
    Py_DECREF(oi);
    
    if( returnValue == NULL )
    {
        // Invalid parameters, stop the reader and let Python do a stack trace
        fail();
        return Request( false );
    }
    
    bool rv = (PyObject_IsTrue(returnValue) == 1);
    
    Py_DECREF(returnValue);
    
    return Request(rv);
}

// *****************************************************************************
Request Reader::component( const std::string &name,
                           const std::string &interp,
                           const Gto::Reader::ComponentInfo &info)
{
    assert( m_callingInstance != NULL );

    // Build the Python equivalent of the Gto::ComponentInfo struct
    PyObject *ci = newComponentInfo( this, info );

    PyObject *returnValue = NULL;
    // Try calling the component method
    returnValue = PyObject_CallMethod( m_callingInstance, 
                                       (char*)"component",
                                       (char*)"ssO",
                                       name.c_str(),
                                       interp.c_str(),
                                       ci );

    Py_DECREF(ci);
    
    if( returnValue == NULL )
    {
        // Invalid parameters, stop the reader and let Python do a stack trace
        fail();
        return Request( false );
    }
    
    bool rv = (PyObject_IsTrue(returnValue) == 1);
    
    Py_DECREF(returnValue);
    
    return Request(rv);
}


// *****************************************************************************
Request Reader::property( const std::string &name,
                          const std::string &interp,
                          const Gto::Reader::PropertyInfo &info )
{
    assert( m_callingInstance != NULL );

    // Build the Python equivalent of the Gto::PropertyInfo struct
    PyObject *pi = newPropertyInfo( this, info );

    PyObject *returnValue = NULL;
    // Try calling the property method with the propInfo tuple
    returnValue = PyObject_CallMethod( m_callingInstance, 
                                       (char*)"property",
                                       (char*)"ssO",
                                       name.c_str(),
                                       interp.c_str(),
                                       pi );
    
    Py_DECREF(pi);
    
    if( returnValue == NULL )
    {
        // Invalid parameters, stop the reader and let Python do a stack trace
        fail();
        return Request( false );
    }
    
    bool rv = (PyObject_IsTrue(returnValue) == 1);
    
    Py_DECREF(returnValue);
    
    return Request(rv);
}

// *****************************************************************************
// Note that this method does not call any overloaded Python method, it
// is here simply to allocate space for Gto::Reader.
void *Reader::data( const PropertyInfo &pinfo, size_t )
{
    assert( m_callingInstance != NULL );
    
    switch( pinfo.type )
    {
    case Gto::Int:
    case Gto::String:
        m_tmpIntData.resize( pinfo.size * pinfo.width );
        return (void *)&m_tmpIntData.front();
    case Gto::Float:
        m_tmpFloatData.resize( pinfo.size * pinfo.width );
        return (void *)&m_tmpFloatData.front();
    case Gto::Double:
        m_tmpDoubleData.resize( pinfo.size * pinfo.width );
        return (void *)&m_tmpDoubleData.front();
    case Gto::Byte:
        m_tmpCharData.resize( pinfo.size * pinfo.width );
        return (void *)&m_tmpCharData.front();
    case Gto::Short:
        m_tmpShortData.resize( pinfo.size * pinfo.width );
        return (void *)&m_tmpShortData.front();
    default:
        PyErr_Format( gtoError(), "Unsupported data type: %s", typeAsString( pinfo.type ) );
        fail();
    }
    return NULL;
}

// *****************************************************************************
void Reader::dataRead( const PropertyInfo &pinfo )
{
    assert( m_callingInstance != NULL );

    // Build a tuple out of the data for this property
    PyObject *dataTuple = PyTuple_New( pinfo.size );
    
    if( pinfo.width > 1 )
    {
      switch( pinfo.type )
      {
      case Gto::String:
          for( size_t i = 0, di = 0; i < pinfo.size; ++i )
          {
              PyObject *subTuple = PyTuple_New( pinfo.width );
              for( size_t w = 0; w < pinfo.width; ++w, ++di )
              {
                  PyTuple_SetItem( subTuple, w, PyString_FromString( stringFromId( m_tmpIntData[di] ).c_str() ) );
              }
              PyTuple_SetItem( dataTuple, i, subTuple );
          }
          m_tmpIntData.clear();
          break;
      case Gto::Int:
          for( size_t i = 0, di = 0; i < pinfo.size; ++i )
          {
              PyObject *subTuple = PyTuple_New( pinfo.width );
              for( size_t w = 0; w < pinfo.width; ++w, ++di )
              {
                  PyTuple_SetItem( subTuple, w, PyInt_FromLong( m_tmpIntData[di] ) );
              }
              PyTuple_SetItem( dataTuple, i, subTuple );
          }
          m_tmpIntData.clear();
          break;
      case Gto::Float:
          for( size_t i = 0, di = 0; i < pinfo.size; ++i )
          {
              PyObject *subTuple = PyTuple_New( pinfo.width );
              for( size_t w = 0; w < pinfo.width; ++w, ++di )
              {
                  PyTuple_SetItem( subTuple, w, PyFloat_FromDouble( m_tmpFloatData[di] ) );
              }
              PyTuple_SetItem( dataTuple, i, subTuple ); 
          }
          m_tmpFloatData.clear();
          break;
      case Gto::Double:
          for( size_t i = 0, di = 0; i < pinfo.size; ++i )
          {
              PyObject *subTuple = PyTuple_New( pinfo.width );
              for( size_t w = 0; w < pinfo.width; ++w, ++di )
              {
                  PyTuple_SetItem( subTuple, w, PyFloat_FromDouble( m_tmpDoubleData[di] ) );
              }
              PyTuple_SetItem( dataTuple, i, subTuple ); 
          }
          m_tmpDoubleData.clear();
          break;
      case Gto::Byte:
          for( size_t i = 0, di = 0; i < pinfo.size; ++i )
          {
              PyObject *subTuple = PyTuple_New( pinfo.width );
              for( size_t w = 0; w < pinfo.width; ++w, ++di )
              {
                  PyTuple_SetItem( subTuple, w, PyInt_FromLong( m_tmpCharData[di] ) );
              }
              PyTuple_SetItem( dataTuple, i, subTuple ); 
          }
          m_tmpCharData.clear();
          break;
      case Gto::Short:
          for( size_t i = 0, di = 0; i < pinfo.size; ++i )
          {
              PyObject *subTuple = PyTuple_New( pinfo.width );
              for( size_t w = 0; w < pinfo.width; ++w, ++di )
              {
                  PyTuple_SetItem( subTuple, w, PyInt_FromLong( m_tmpShortData[di] ) );
              }
              PyTuple_SetItem( dataTuple, i, subTuple ); 
          }
          m_tmpShortData.clear();
      default:
          break;
      }
    }
    else
    {
      switch( pinfo.type )
      {
      case Gto::String:
          for( size_t i = 0; i < pinfo.size; ++i )
          {
              PyTuple_SetItem( dataTuple, i, PyString_FromString( stringFromId( m_tmpIntData[i] ).c_str() ) );
          }
          m_tmpIntData.clear();
          break;
      case Gto::Int:
          for( size_t i = 0; i < pinfo.size; ++i )
          {
              PyTuple_SetItem( dataTuple, i, PyInt_FromLong( m_tmpIntData[i] ) );
          }
          m_tmpIntData.clear();
          break;
      case Gto::Float:
          for( size_t i = 0; i < pinfo.size; ++i )
          {
              //std::cout << m_tmpFloatData[i] << " ";
              PyTuple_SetItem( dataTuple, i, PyFloat_FromDouble( m_tmpFloatData[i] ) ); 
          }
          m_tmpFloatData.clear();
          break;
      case Gto::Double:
          for( size_t i = 0; i < pinfo.size; ++i )
          {
              PyTuple_SetItem( dataTuple, i, PyFloat_FromDouble( m_tmpDoubleData[i] ) ); 
          }
          m_tmpDoubleData.clear();
          break;
      case Gto::Byte:
          for( size_t i = 0; i < pinfo.size; ++i )
          {
              PyTuple_SetItem( dataTuple, i, PyInt_FromLong( m_tmpCharData[i] ) ); 
          }
          m_tmpCharData.clear();
          break;
      case Gto::Short:
          for( size_t i = 0; i < pinfo.size; ++i )
          {
              PyTuple_SetItem( dataTuple, i, PyInt_FromLong( m_tmpShortData[i] ) ); 
          }
          m_tmpShortData.clear();
      default:
          break;
      }
    }
    
    assert( dataTuple != NULL );

    // Build the Python equivalent of the Gto::PropertyInfo struct
    PyObject *pi = newPropertyInfo( this, pinfo );

    PyObject *returnValue = NULL;
    returnValue = PyObject_CallMethod( m_callingInstance,
                                       (char*)"dataRead",
                                       (char*)"sOO", 
                                       stringFromId( pinfo.name ).c_str(),
                                       dataTuple,
                                       pi );
    
    Py_DECREF(pi);
    Py_DECREF(dataTuple);
    
    // The data method was not properly overridden...
    if( returnValue == NULL )
    {
        // Invalid parameters, stop the reader and let Python do a stack trace
        fail();
    }
    else
    {
        Py_DECREF(returnValue);
    }
}


static PyObject* Reader_new(PyTypeObject *type, PyObject *, PyObject *)
{
    PyReader *self = (PyReader*) type->tp_alloc(type, 0);

    if (self != NULL)
    {
        self->m_reader = 0;
        self->m_isOpen = false;
    }
    
    return (PyObject*) self;
}

static void Reader_dealloc( PyObject *self )
{
    PyReader *rdr = (PyReader *)self;
    
    if( rdr->m_reader )
    {
        delete rdr->m_reader;
    }
    
    Py_TYPE(self)->tp_free(self);
}

static int Reader_init( PyObject *self, PyObject *args, PyObject * )
{
    int mode = Gto::Reader::None;
    
    if( ! PyArg_ParseTuple( args, "|i:gto.Reader.__init__", &mode ) )
    {
        return 1;
    }
    
    Reader *creader = new Reader( (PyObject *)self, mode );
    if( ! creader )
    {
        PyErr_Format( gtoError(), "Unable to create instance of Gto::Reader. Bad parameters?" );
        return 1;
    }
    
    // Create a new Python object to hold the pointer to the C++ reader
    // instance and add it to this Python instance's dictionary
    PyReader *prdr = (PyReader*) self;
    prdr->m_reader = creader;
    prdr->m_isOpen = false;

    return 0;
}

static PyObject *Reader_open( PyObject *self, PyObject *args )
{
    char *filename;
    
    if( ! PyArg_ParseTuple( args, "s:_gto.Reader.open", &filename ) )
    {
        // Invalid parameters, let Python do a stack trace
        return NULL;
    }

    PyReader *reader = (PyReader *) self;

    // We set isOpen _before_ calling open, because it is the open method that
    // calls all our other crap.  If we don't do it now, we don't get another
    // chance until it's all over.
    reader->m_isOpen = true;
    if( ! reader->m_reader->open( filename ) )
    {
        // Something went wrong.  If the error was in the Python world, 
        // the error message should already be set.  If not, there was a
        // problem in the C++ world, so set the error message now.
        if( ! PyErr_Occurred() )
        {
            PyErr_Format( gtoError(), "Unable to open %s: %s", filename, reader->m_reader->why().c_str() );
        }
        reader->m_isOpen = false;
        return NULL;
    }
    
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *Reader_fail( PyObject *self, PyObject *args )
{
    char *why;

    if( ! PyArg_ParseTuple( args, "s:_gto.Reader.fail", &why ) )
    {
        // Invalid parameters, let Python do a stack trace
        return NULL;
    }

    PyReader *prdr = (PyReader*) self;

    if ( !prdr->m_reader || !prdr->m_isOpen )
    {
      return NULL;
    }

    Reader *reader = prdr->m_reader;
    
    reader->fail( why );

    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *Reader_why( PyObject *self, PyObject * )
{
    PyReader *prdr = (PyReader*) self;

    if ( !prdr->m_reader || !prdr->m_isOpen )
    {
      return NULL;
    }

    Reader *reader = prdr->m_reader;
    
    PyObject *errMsg = PyString_FromString( reader->why().c_str() );
    
    return errMsg;
}

static PyObject *Reader_close( PyObject *self, PyObject * )
{
    PyReader *reader = (PyReader *) self;
    
    if( reader->m_reader == NULL || reader->m_isOpen == false )
    {
        PyErr_SetString( gtoError(), "no file is open." );
        return NULL;
    }

    reader->m_reader->close();
    reader->m_isOpen = false;

    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *Reader_object( PyObject *, PyObject *args )
{
    char *name;
    char *protocol;
    int protocolVersion;
    PyObject *objInfo;

    if( ! PyArg_ParseTuple( args, "ssiO:_gto.Reader.object", &name, &protocol, &protocolVersion, &objInfo ) )
    {
        // Invalid parameters, let Python do a stack trace
        return NULL;
    }
    
    // Assume we want all the objects in the file
    return PyInt_FromLong( 1 );
}

static PyObject *Reader_component( PyObject *, PyObject *args )
{
    char *name;
    char *interp;
    PyObject *compInfo;

    if( ! PyArg_ParseTuple( args, "ssO:_gto.Reader.component", &name, &interp, &compInfo ) )
    {
        // Invalid parameters, let Python do a stack trace
        return NULL;
    }

    // Assume we want all the components in the file
    return PyInt_FromLong( 1 );
}

static PyObject *Reader_property( PyObject *, PyObject *args )
{
    char *name;
    char *interp;
    PyObject *propInfo;

    if( ! PyArg_ParseTuple( args, "ssO:_gto.Reader.property", &name, &interp, &propInfo ) )
    {
        // Invalid parameters, let Python do a stack trace
        return NULL;
    }
       
    // Assume we want all the properties in the file
    return PyInt_FromLong( 1 );
}

static PyObject *Reader_dataRead( PyObject *, PyObject *args )
{
    char *name;
    PyObject *dataTuple;
    PyObject *propInfo;
    
    if( ! PyArg_ParseTuple( args, "sOO:_gto.Reader.dataRead", &name, &dataTuple, &propInfo ) )
    {
        // Invalid parameters, let Python do a stack trace
        return NULL;
    }
    
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *Reader_stringFromId( PyObject *self, PyObject *args )
{
    int id;
    
    if( ! PyArg_ParseTuple( args, "i:_gto.Reader.stringFromId", &id ) )
    {
        // Invalid parameters, let Python do a stack trace
        return NULL;
    }

    PyReader *prdr = (PyReader*) self;

    if ( !prdr->m_reader || !prdr->m_isOpen )
    {
        return NULL;
    }
    
    Reader *reader = prdr->m_reader;
    
    PyObject *str = PyString_FromString( reader->stringFromId( id ).c_str() );
    
    return str;
}


static PyObject *Reader_stringTable( PyObject *self, PyObject * )
{
    PyReader *prdr = (PyReader*) self;

    if ( !prdr->m_reader || !prdr->m_isOpen )
    {
        return NULL;
    }
    
    Reader *reader = prdr->m_reader;
    
    Gto::Reader::StringTable stringTable = reader->stringTable();

    PyObject *stringTableTuple = PyTuple_New( stringTable.size() );
    
    for( size_t i = 0; i < stringTable.size(); ++i )
    {
        PyTuple_SetItem( stringTableTuple, i, PyString_FromString( stringTable[i].c_str() ) );
    }
    
    return stringTableTuple;    
}


static PyObject *Reader_isSwapped( PyObject *self, PyObject * )
{
    PyReader *prdr = (PyReader*) self;

    if ( !prdr->m_reader || !prdr->m_isOpen )
    {
        return NULL;
    }
    
    Reader *reader = prdr->m_reader;

    if( reader->isSwapped() )
    {
        Py_INCREF( Py_True );
        return Py_True;
    }
    
    Py_INCREF( Py_False );
    return Py_False;
}

static PyObject *Reader_objects( PyObject *self, PyObject * )
{
    PyReader *prdr = (PyReader*) self;

    if ( !prdr->m_reader || !prdr->m_isOpen )
    {
        return NULL;
    }
    
    Reader *reader = prdr->m_reader;

    if( reader->readMode() != Gto::Reader::RandomAccess )
    {
        PyErr_SetString( gtoError(), "file was not opened for random access." );
        return NULL;
    }
    
    Gto::Reader::Objects &objects = reader->objects();
    
    PyObject *objectsTuple = PyTuple_New( objects.size() );
    
    for( size_t i = 0; i < objects.size(); ++i )
    {
        PyTuple_SetItem( objectsTuple, i, newObjectInfo( reader, objects[i] ) );
    }
    
    return objectsTuple;    
}

static PyObject *Reader_accessObject( PyObject *self, PyObject *args )
{
    PyObject *objInfo;
    
    if( ! PyArg_ParseTuple( args, "O:_gto.Reader.accessObject", &objInfo ) )
    {
        // Invalid parameters, let Python do a stack trace
        return NULL;
    }

    if (!PyObject_TypeCheck(objInfo, &ObjectInfoType))
    {
        PyErr_SetString( gtoError(), "accessObject requires an ObjectInfo instance" );
        return NULL;
    }

    PyReader *prdr = (PyReader*) self;

    if ( !prdr->m_reader || !prdr->m_isOpen )
    {
        return NULL;
    }
    
    Reader *reader = prdr->m_reader;
    
    if( reader->readMode() != Gto::Reader::RandomAccess )
    {
        PyErr_SetString( gtoError(), "file was not opened for random access." );
        return NULL;
    }

    if (((PyObjectInfo*)objInfo)->mInfo == NULL)
    {
        PyErr_SetString( gtoError(), "invalid object." );
        return NULL;
    }
    
    Gto::Reader::ObjectInfo &oi = *((Gto::Reader::ObjectInfo*) ((PyObjectInfo*)objInfo)->mInfo);
    
    if( reader->accessObject( oi ) )
    {
        Py_INCREF( Py_True );
        return Py_True;
    }
    else
    {
        Py_INCREF( Py_False );
        return Py_False;
    }
}

static PyObject *Reader_components( PyObject *self, PyObject * )
{
    PyReader *prdr = (PyReader*) self;

    if ( !prdr->m_reader || !prdr->m_isOpen )
    {
        return NULL;
    }
    
    Reader *reader = prdr->m_reader;
    
    if( reader->readMode() != Gto::Reader::RandomAccess )
    {
        PyErr_SetString( gtoError(), "file was not opened for random access." );
        return NULL;
    }
    
    Gto::Reader::Components &components = reader->components();
    
    PyObject *componentsTuple = PyTuple_New( components.size() );
    
    for( size_t i = 0; i < components.size(); ++i )
    {
        PyTuple_SetItem( componentsTuple, i, newComponentInfo( reader, components[i] ) );
    }
    
    return componentsTuple;    
}

static PyObject *Reader_accessComponent( PyObject *self, PyObject *args )
{
    PyObject *compInfo;
    
    if( ! PyArg_ParseTuple( args, "O:_gto.Reader.accessComponent", &compInfo ) )
    {
        // Invalid parameters, let Python do a stack trace
        return NULL;
    }

    if (!PyObject_TypeCheck(compInfo, &ComponentInfoType))
    {
        PyErr_SetString( gtoError(), "accessComponent requires a ComponentInfo instance" );
        return NULL;
    }

    PyReader *prdr = (PyReader*) self;

    if ( !prdr->m_reader || !prdr->m_isOpen )
    {
        return NULL;
    }
    
    Reader *reader = prdr->m_reader;
    
    if( reader->readMode() != Gto::Reader::RandomAccess )
    {
        PyErr_SetString( gtoError(), "file was not opened for random access." );
        return NULL;
    }
    
    if (((PyComponentInfo*)compInfo)->mInfo == NULL)
    {
        PyErr_SetString( gtoError(), "invalid component." );
        return NULL;
    }
    Gto::Reader::ComponentInfo &ci = *((Gto::Reader::ComponentInfo*) ((PyComponentInfo*)compInfo)->mInfo);
    
    if( reader->accessComponent( ci ) )
    {
        Py_INCREF( Py_True );
        return Py_True;
    }
    else
    {
        Py_INCREF( Py_False );
        return Py_False;
    }
}

static PyObject *Reader_properties( PyObject *self, PyObject * )
{
    PyReader *prdr = (PyReader*) self;

    if ( !prdr->m_reader || !prdr->m_isOpen )
    {
        return NULL;
    }
    
    Reader *reader = prdr->m_reader;
    
    if( reader->readMode() != Gto::Reader::RandomAccess )
    {
        PyErr_SetString( gtoError(), "file was not opened for random access." );
        return NULL;
    }
    
    Gto::Reader::Properties &properties = reader->properties();
    
    PyObject *propertiesTuple = PyTuple_New( properties.size() );
    
    for( size_t i = 0; i < properties.size(); ++i )
    {
        PyTuple_SetItem( propertiesTuple, i, newPropertyInfo( reader, properties[i] ) );
    }
    
    return propertiesTuple;    
}

static PyObject *Reader_accessProperty( PyObject *self, PyObject *args )
{
    PyObject *propInfo;
    
    if( ! PyArg_ParseTuple( args, "O:_gto.Reader.accessProperty", &propInfo ) )
    {
        // Invalid parameters, let Python do a stack trace
        return NULL;
    }

    if (!PyObject_TypeCheck(propInfo, &PropertyInfoType))
    {
        PyErr_SetString( gtoError(), "accessProperty requires a ComponentInfo instance" );
        return NULL;
    }

    PyReader *prdr = (PyReader*) self;

    if ( !prdr->m_reader || !prdr->m_isOpen )
    {
        return NULL;
    }
    
    Reader *reader = prdr->m_reader;
    
    if( reader->readMode() != Gto::Reader::RandomAccess )
    {
        PyErr_SetString( gtoError(), "file was not opened for random access." );
        return NULL;
    }
    
    if (((PyPropertyInfo*)propInfo)->mInfo == NULL)
    {
        PyErr_SetString( gtoError(), "invalid property." );
        return NULL;
    }
    Gto::Reader::PropertyInfo &pi = *((Gto::Reader::PropertyInfo*) ((PyPropertyInfo*)propInfo)->mInfo);
    
    if( reader->accessProperty( pi ) )
    {
        Py_INCREF( Py_True );
        return Py_True;
    }
    else
    {
        Py_INCREF( Py_False );
        return Py_False;
    }
}

static PyMethodDef ReaderMethods[] = 
{
    {"open", Reader_open, METH_VARARGS, "open( string filename )"},
    {"fail", Reader_fail, METH_VARARGS, "fail( string why )"},
    {"why", Reader_why, METH_VARARGS, "why()"},
    {"close", Reader_close, METH_VARARGS, "close()"},
    {"object", Reader_object, METH_VARARGS, "object( string name, string protocol, unsigned int protocolVersion, ObjectInfo header )"},
    {"component", Reader_component, METH_VARARGS, "component( string name, ComponentInfo header )"},
    {"property", Reader_property, METH_VARARGS, "property( string name, PropertyInfo header )"},
    {"dataRead", Reader_dataRead, METH_VARARGS, "dataRead( PropertyInfo pinfo )"},
    {"stringFromId", Reader_stringFromId, METH_VARARGS, "stringFromId( int i )"},
    {"stringTable", Reader_stringTable, METH_VARARGS, "stringTable()"},
    {"isSwapped", Reader_isSwapped, METH_VARARGS, "isSwapped()"},
    {"objects", Reader_objects, METH_VARARGS, "objects()"},
    {"accessObject", Reader_accessObject, METH_VARARGS, "accessObject( objectInfo )"},
    {"components", Reader_components, METH_VARARGS, "components()"},
    {"accessComponent", Reader_accessComponent, METH_VARARGS, "accessComponent( componentInfo )"},
    {"properties", Reader_properties, METH_VARARGS, "properties()"},
    {"accessProperty", Reader_accessProperty, METH_VARARGS, "accessProperty( propertyInfo )"},
    {NULL, NULL, 0, NULL}
};


PyTypeObject ReaderType;

bool initReader(PyObject *module)
{
    memset(&ReaderType, 0, sizeof(PyTypeObject));

    PyObject *value;

    PyObject *classMembers = PyDict_New();
    value = PyInt_FromLong( Gto::Reader::None );
    PyDict_SetItemString( classMembers, "None", value );
    Py_DECREF(value);
    value = PyInt_FromLong( Gto::Reader::HeaderOnly );
    PyDict_SetItemString( classMembers, "HeaderOnly", value );
    Py_DECREF(value);
    value = PyInt_FromLong( Gto::Reader::RandomAccess );
    PyDict_SetItemString( classMembers, "RandomAccess", value );
    Py_DECREF(value);
    value = PyInt_FromLong( Gto::Reader::BinaryOnly );
    PyDict_SetItemString( classMembers, "BinaryOnly", value );
    Py_DECREF(value);
    value = PyInt_FromLong( Gto::Reader::TextOnly );
    PyDict_SetItemString( classMembers, "TextOnly", value );
    Py_DECREF(value);

    INIT_REFCNT(ReaderType);
    ReaderType.tp_name = "_gto.Reader";
    ReaderType.tp_basicsize = sizeof(PyReader);
    ReaderType.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE;
    ReaderType.tp_new = Reader_new;
    ReaderType.tp_init = Reader_init;
    ReaderType.tp_dealloc = Reader_dealloc;
    ReaderType.tp_methods = ReaderMethods;
    ReaderType.tp_doc = ReaderDocString;
    ReaderType.tp_dict = classMembers;

    if (PyType_Ready(&ReaderType) < 0)
    {
        return false;
    }

    Py_INCREF(&ReaderType);
    PyModule_AddObject(module, "Reader", (PyObject *)&ReaderType);

    return true;
}


} // End namespace PyGto

