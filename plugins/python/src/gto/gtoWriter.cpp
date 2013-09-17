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

#include <sstream>
#include "gtoWriter.h"

namespace PyGto {

static char WriterDocString[] = 
"\n"
"The Writer class is designed as an API to a state machine. You indicate\n"
"a conceptual hierarchy to the file and then all the data. The writer\n"
"handles generating the string table, the header information, etc.\n"
"\n";


// *****************************************************************************
// We start with a few utility functions...
// *****************************************************************************

// *****************************************************************************
// Flatten a tuple or list into a C array of any type using the converter
// supplied.  'start' is used internally for recursive purposes.  Returns
// the number of items in the C array.
template<typename T> int flatten( PyObject *object,
                                  T *data,
                                  int maxItems,
                                  const char *expectedTypeStr,
                                  T (*converter)(PyObject *),
                                  bool start = true )
{
    static int pos;
    
    if( start )
    {
        pos = 0;
    }
    
    if( pos > maxItems )
    {
        return pos;
    }
    
    if (!PyList_Check(object) && !PyTuple_Check(object))
    {
        std::string classname = PyTypeName(object);
        PyErr_Format( gtoError(), "Can't handle '%s' class data directly. Convert it to a tuple or list first.", classname.c_str() );
        return -1;
    }

    // Put atoms directly into the buffer, and recurse on more complex types
    for( int i = 0; i < PySequence_Size( object ); ++i )
    {
        PyObject *item = PySequence_GetItem( object, i );
        
        // Get rid of this PyInstance_Check
        if( PyTuple_Check( item ) || PyList_Check( item ) )
        {
            flatten( item, data, maxItems, expectedTypeStr, converter, false );
        }
        else
        {
            // Add the atom to the buffer and move on
            data[pos] = converter( item );
            
            if( PyErr_Occurred() )
            {
                Py_DECREF(item);
                
                if( PyErr_ExceptionMatches( PyExc_TypeError ) )
                {
                    // Data of a type not handled by the converter
                    PyErr_Format( gtoError(), "Expected data of type '%s', but got '%s'", expectedTypeStr, PyTypeName( item ).c_str() );
                }
                
                return -1;
            }
            
            ++pos;
            
            if( pos > maxItems )
            {
                return pos;
            }
        }
        
        Py_DECREF(item);
    }
    
    return pos;
}



// *****************************************************************************
// The next several functions implement the methods on the Python _gto.Writer
// class.
// *****************************************************************************

static PyObject* Writer_new( PyTypeObject *type, PyObject *, PyObject * )
{
    PyWriter *self = (PyWriter*) type->tp_alloc(type, 0);
    if (self)
    {
        self->m_writer = 0;
        self->m_propCount = 0;
        self->m_beginDataCalled = false;
        self->m_objectDef = false;
        self->m_componentDef = false;
        self->m_propertyNames = 0;
    }
    return (PyObject*) self;
}

static void Writer_dealloc( PyObject *self )
{
    PyWriter *pw = (PyWriter *)self;
    if (pw->m_writer)
    {
        delete pw->m_writer;
    }
    if (pw->m_propertyNames)
    {
        delete pw->m_propertyNames;
    }
    Py_TYPE(self)->tp_free(self);
}

static int Writer_init( PyObject *self, PyObject *, PyObject * )
{
    PyWriter *pw = (PyWriter*) self;

    pw->m_writer = new Gto::Writer();
    pw->m_propertyNames = new std::vector<std::string>();

    return 0;
}

static PyObject *Writer_open( PyObject *self, PyObject *args )
{
    char *filename;
    Gto::Writer::FileType filemode = Gto::Writer::BinaryGTO;

    if( ! PyArg_ParseTuple( args, "s|i:_gto.Writer.open", &filename, &filemode ) )
    {
        // Invalid parameters, let Python do a stack trace
        return NULL;
    }

    PyWriter *writer = (PyWriter*) self;

    if (!writer->m_writer || !writer->m_propertyNames)
    {
        return NULL;
    }

    // Ask the writer to open the given file
    if( ! writer->m_writer->open( filename, filemode ) )
    {
        PyErr_Format( gtoError(), "Unable to open specified file: %s", filename );
        return NULL;
    }

    writer->m_propCount = 0;
    writer->m_beginDataCalled = false;
    writer->m_objectDef = false;
    writer->m_componentDef = false;
    writer->m_propertyNames->clear();
    
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *Writer_close( PyObject *self, PyObject * )
{
    PyWriter *writer = (PyWriter*) self;
    
    if (!writer->m_writer || !writer->m_propertyNames)
    {
        return NULL;
    }

    writer->m_writer->close();

    writer->m_propCount = 0;
    writer->m_beginDataCalled = false;
    writer->m_objectDef = false;
    writer->m_componentDef = false;
    writer->m_propertyNames->clear();
    
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *Writer_beginObject( PyObject *self, PyObject *args )
{
    char *name;
    char *protocol;
    unsigned int protocolVersion;
    
    if( ! PyArg_ParseTuple( args, "ssi:_gto.Writer.beginObject", &name, &protocol, &protocolVersion ) )
    {
        // Invalid parameters, let Python do a stack trace
        return NULL;
    }
    
    PyWriter *writer = (PyWriter*) self;
    
    if (!writer->m_writer)
    {
        return NULL;
    }

    // Check for dumbassness
    if( writer->m_objectDef == true )
    {
        PyErr_SetString( gtoError(), "Can't nest object declarations" );
        return NULL;
    }
    
    if( writer->m_beginDataCalled == true )
    {
        PyErr_SetString( gtoError(), "Once beginData is called, no new objects can be declared" );
        return NULL;
    }
    
    // Make it so
    writer->m_writer->beginObject( name, protocol, protocolVersion );
    writer->m_objectDef = true;

    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *Writer_endObject( PyObject *self, PyObject * )
{
    PyWriter *writer = (PyWriter*) self;
    
    if (!writer->m_writer)
    {
        return NULL;
    }

    // Check for dumbassness
    if( writer->m_objectDef == false )
    {
        PyErr_SetString( gtoError(), "endObject called before beginObject" );
        return NULL;
    }
    
    // Make it so
    writer->m_writer->endObject();
    writer->m_objectDef = false;
    
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *Writer_beginComponent( PyObject *self, PyObject *args )
{
    char *name;
    char *interp = (char*)"";
    int flags = 0;
    
    // Try GTOv2 prototype first...
    if( ! PyArg_ParseTuple( args, "s|i:_gto.Writer.beginComponent", &name, &flags ) )
    {
        PyErr_Clear();
        // If that doesn't work, try the GTOv3 prototype
        if( ! PyArg_ParseTuple( args, "ss|i:_gto.Writer.beginComponent", &name, &interp, &flags ) )
        {
            // Invalid parameters, let Python do a stack trace
            return NULL;
        }
    }
    
    PyWriter *writer = (PyWriter*) self;
    
    if (!writer->m_writer)
    {
        return NULL;
    }
    
    // Check for dumbassness
    if( writer->m_objectDef == false )
    {
        PyErr_SetString( gtoError(), "Components can only exist inside object blocks" );
        return NULL;
    }
    
    if( writer->m_componentDef == true )
    {
        PyErr_SetString( gtoError(), "Can't nest component declarations" );
        return NULL;
    }
    
    // Make it so
    writer->m_writer->beginComponent( name, interp, flags );
    writer->m_componentDef = true;
    
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *Writer_endComponent( PyObject *self, PyObject * )
{
    PyWriter *writer = (PyWriter*) self;
    
    if (!writer->m_writer)
    {
        return NULL;
    }
    
    // Check for dumbassness
    if( writer->m_componentDef == false )
    {
        PyErr_SetString( gtoError(), "endComponent called before beginComponent" );
        return NULL;
    }
    
    // Make it so
    writer->m_writer->endComponent();
    writer->m_componentDef = false;
    
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *Writer_property( PyObject *self, PyObject *args )
{
    char *name;
    int type;
    int numElements;
    int width = 1;
    char *interp = (char*)"";
    
    if( ! PyArg_ParseTuple( args, "sii|is:_gto.Writer.property", &name, &type, &numElements,  &width, &interp ) )
    {
        // Invalid parameters, let Python do a stack trace
        return NULL;
    }
    
    PyWriter *writer = (PyWriter*) self;
    
    if (!writer->m_writer || !writer->m_propertyNames)
    {
        return NULL;
    }
    
    // Check for dumbassness    
    if( writer->m_objectDef == false || writer->m_componentDef == false )
    {
        PyErr_SetString( gtoError(), "Properties can only exist inside object/component blocks" );
        return NULL;
    }
    
    // Store name for later dumbassness checking in propertyData()
    writer->m_propertyNames->push_back( name );
    
    // Make it so
    writer->m_writer->property( name,
                                (Gto::DataType)type,
                                numElements, 
                                width,
                                interp );
    
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *Writer_intern( PyObject *self, PyObject *args )
{
    PyObject *data;

    if( ! PyArg_ParseTuple( args, "O:_gto.Writer.intern", &data ) )
    {
        // Invalid parameters, let Python do a stack trace
        return NULL;
    }
    
    PyWriter *writer = (PyWriter*) self;
    
    if (!writer->m_writer)
    {
        return NULL;
    }
    
    // Handle a single string
    if( PyString_Check( data ) )
    {
        writer->m_writer->intern( PyString_AsStdString( data ) );
    }
    // Handle a bunch of strings all at once
    else if( PySequence_Check( data ) )
    {
        for( int i = 0; i < PySequence_Size( data ); ++i )
        {
            PyObject *pstr = PySequence_GetItem( data, i );
            if( ! PyString_Check( pstr ) )
            {
                Py_DECREF(pstr);
                PyErr_SetString( gtoError(), "Non-string in sequence" );
                return NULL;
            }
            writer->m_writer->intern( PyString_AsStdString( pstr ) );
            Py_DECREF(pstr);
        }
    }
    // We can't handle what we were given
    else
    {
        PyErr_SetString( gtoError(), "intern requires a string or a sequence of strings" );
        return NULL;
    }
    
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *Writer_lookup( PyObject *self, PyObject *args )
{
    char *str;
    
    if( ! PyArg_ParseTuple( args, "s:_gto.Writer.lookup", &str ) )
    {
        // Invalid parameters, let Python do a stack trace
        return NULL;
    }
    
    PyWriter *writer = (PyWriter*) self;
    
    if (!writer->m_writer)
    {
        return NULL;
    }
    
    // Check for dumbassness
    if( writer->m_beginDataCalled == false )
    {
        PyErr_SetString( gtoError(), "lookup() cannot be used until beginData() is called" );
        return NULL;
    }
    
    // Make it so
    return PyInt_FromLong( writer->m_writer->lookup( str ) );
}

static PyObject *Writer_beginData( PyObject *self, PyObject * )
{
    PyWriter *writer = (PyWriter*) self;
    
    if (!writer->m_writer)
    {
        return NULL;
    }
    
    // Check for dumbassness
    if( writer->m_writer->properties().size() == 0 )
    {
        PyErr_SetString( gtoError(), "There are no properties to write" );
        return NULL;
    }
    
    // Make it so
    writer->m_writer->beginData();
    writer->m_beginDataCalled = true;
    
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *Writer_endData( PyObject *self, PyObject * )
{
    PyWriter *writer = (PyWriter*) self;
    
    if (!writer->m_writer)
    {
        return NULL;
    }
    
    // Check for dumbassness
    if( writer->m_beginDataCalled == false )
    {
        PyErr_SetString( gtoError(), "endData called before beginData" );
        return NULL;
    }
    
    // Make it so
    writer->m_writer->endData();
    
    Py_INCREF( Py_None );
    return Py_None;
}

static PyObject *Writer_propertyData( PyObject *self, PyObject *args )
{
    PyObject *rawdata;
    bool rawdataOverride = false;
    
    if( ! PyArg_ParseTuple( args, "O:_gto.Writer.propertyData", &rawdata ) )
    {
        // Invalid parameters, let Python do a stack trace
        return NULL;
    }
    
    PyWriter *writer = (PyWriter*) self;
    
    if (!writer->m_writer || !writer->m_propertyNames)
    {
        return NULL;
    }
    
    // Check for dumbassness
    if( ! writer->m_beginDataCalled )
    {
        PyErr_SetString( gtoError(), "propertyData called before beginData" );
        return NULL;
    }

    // If we're handed a single value, tuple-ize it for the code below
    if( PyInt_Check( rawdata ) || PyFloat_Check( rawdata ) || PyString_Check( rawdata ) )
    {
        rawdataOverride = true;
        
        PyObject *tmp = PyTuple_New( 1 );
        
        Py_INCREF(rawdata);
        PyTuple_SetItem( tmp, 0, rawdata );
        
        rawdata = tmp;
    }

    // Get a handle to the property definition for the current property
    // and do some sanity checking
    Gto::PropertyHeader prop = writer->m_writer->properties()[writer->m_propCount];
    
    if( size_t(writer->m_propCount) >= writer->m_writer->properties().size() )
    {
        if( rawdataOverride )
        {
            // rawdata pointer was replace by a newly allocated tuple
            Py_DECREF(rawdata);
        }
        PyErr_SetString( gtoError(), "Undeclared data." );
        return NULL;
    }
    
    const char *currentPropName = (*writer->m_propertyNames)[writer->m_propCount].c_str();

    // Determine how many elements we have in the data
    int dataSize = prop.size * prop.width;
    PyObject *retval = NULL;
    
    // Write that data!
    switch( prop.type )
    {
    case Gto::Int:
    {
        int *data = new int[dataSize];
        int numItems = flatten( rawdata, data, dataSize, "int", PyInt_AsInt );
        
        if( PyErr_Occurred() == NULL )
        {
            if( numItems != dataSize )
            {
                PyErr_Format( gtoError(), "Property '%s' was declared as having %d"
                              " x %d values, but %d values were given for writing",
                              currentPropName, prop.size, prop.width,
                              numItems );
            }
            else
            {
                writer->m_writer->propertyData( data );
                writer->m_propCount++;
                retval = Py_None;
            }
        }
        
        delete[] data;
        break;
    }
    case Gto::Float:
    {
        float *data = new float[dataSize];
        int numItems = flatten( rawdata, data, dataSize, "float", PyFloat_AsFloat );
        
        if( PyErr_Occurred() == NULL )
        {
            if( numItems != dataSize )
            {
                PyErr_Format( gtoError(), "Property '%s' was declared as having %d"
                              " x %d values, but %d values were given for writing",
                              currentPropName, prop.size, prop.width,
                              numItems );
            }
            else
            {
                writer->m_writer->propertyData( data );
                writer->m_propCount++;
                retval = Py_None;
            }
        }
        
        delete[] data;
        break;
    }
    case Gto::Double:
    {
        double *data = new double[dataSize];
        int numItems = flatten( rawdata, data, dataSize, "double", PyFloat_AsDouble );
        
        if( PyErr_Occurred() == NULL )
        {
            if( numItems != dataSize )
            {
                PyErr_Format( gtoError(), "Property '%s' was declared as having %d"
                              " x %d values, but %d values were given for writing",
                              currentPropName, prop.size, prop.width,
                              numItems );
            }
            else
            {
                writer->m_writer->propertyData( data );
                writer->m_propCount++;
                retval = Py_None;
            }
        }
        
        delete[] data;
        break;
    }
    case Gto::Short:
    {
        unsigned short *data = new unsigned short[dataSize];
        int numItems = flatten( rawdata, data, dataSize, "short", PyInt_AsShort );
        
        if( PyErr_Occurred() == NULL )
        {
            if( numItems != dataSize )
            {
                PyErr_Format( gtoError(), "Property '%s' was declared as having %d"
                              " x %d values, but %d values were given for writing",
                              currentPropName, prop.size, prop.width,
                              numItems );
            }
            else
            {
                writer->m_writer->propertyData( data );
                writer->m_propCount++;
                retval = Py_None;
            }
        }
        
        delete[] data;
        break;
    }
    case Gto::Byte:
    {
        unsigned char *data = new unsigned char[dataSize];
        int numItems = flatten( rawdata, data, dataSize, "byte", PyInt_AsByte );
        
        if( PyErr_Occurred() == NULL )
        {
            if( numItems != dataSize )
            {
                PyErr_Format( gtoError(), "Property '%s' was declared as having %d"
                              " x %d values, but %d values were given for writing",
                              currentPropName, prop.size, prop.width,
                              numItems );
            }
            else
            {
                writer->m_writer->propertyData( data );
                writer->m_propCount++;
                retval = Py_None;
            }
        }
        
        delete[] data;
        break;
    }
    case Gto::String:
    {
        std::string *strings = new std::string[dataSize];
        int numItems = flatten( rawdata, strings, dataSize, "string", PyString_AsStdString );
        
        if( PyErr_Occurred() == NULL )
        {
            if( numItems != dataSize )
            {
                PyErr_Format( gtoError(), "Property '%s' was declared as having %d"
                              " x %d values, but %d values were given for writing",
                              currentPropName, prop.size, prop.width,
                              numItems );
            }
            else
            {
                int *data = new int[dataSize];
                
                int i = 0;
                for(; i < numItems; ++i )
                {
                    data[i] = writer->m_writer->lookup( strings[i] );
                    if( data[i] == -1 )
                    {
                        PyErr_Format( gtoError(),
                                      "'%s' needs to be \"interned\" before it can "
                                      "be used as data in property #%d",
                                      strings[i].c_str(), writer->m_propCount );
                        break;
                    }
                }
                
                if( i == numItems )
                {
                    // all items processed properly
                    writer->m_writer->propertyData( data );
                    writer->m_propCount++;
                    retval = Py_None;
                }
                
                delete[] data;
            }
        }
        
        delete[] strings;
        break;
    }
    default:
        PyErr_Format( gtoError(), "Undefined property type: %d  in property '%s'", prop.type, currentPropName );
        break;
    }
    
    if( rawdataOverride )
    {
        Py_DECREF(rawdata);
    }
    
    Py_XINCREF( retval );
    return retval;
}

static PyMethodDef WriterMethods[] = 
{
    {"open", Writer_open, METH_VARARGS, "open( filename )"},
    {"close", Writer_close, METH_VARARGS, "close()"},
    {"beginObject", Writer_beginObject, METH_VARARGS, "beginObject( string name, string protocol )"},
    {"endObject", Writer_endObject, METH_VARARGS, "endObject()"},
    {"beginComponent", Writer_beginComponent, METH_VARARGS, "beginComponent( string name, transposed = false )"},
    {"endComponent", Writer_endComponent, METH_VARARGS, "endComponent()"},
    {"property", Writer_property, METH_VARARGS, "property( string name, int type, int numElements, int partsPerElement  )"},
    {"intern", Writer_intern, METH_VARARGS, "intern(string)"},
    {"lookup", Writer_lookup, METH_VARARGS, "lookup(string)"},
    {"beginData", Writer_beginData, METH_VARARGS, "beginData()"},
    {"propertyData", Writer_propertyData, METH_VARARGS, "propertyData( tuple data )"},
    {"endData", Writer_endData, METH_VARARGS, "endData()"},
    {NULL, NULL, 0, NULL}
};

PyTypeObject WriterType;

bool initWriter(PyObject *module)
{
    memset(&WriterType, 0, sizeof(PyTypeObject));

    PyObject *value;

    PyObject *classMembers = PyDict_New();
    value = PyInt_FromLong( Gto::Writer::BinaryGTO );
    PyDict_SetItemString( classMembers, "BinaryGTO", value );
    Py_DECREF(value);
    value = PyInt_FromLong( Gto::Writer::TextGTO );
    PyDict_SetItemString( classMembers, "TextGTO", value );
    Py_DECREF(value);
    value = PyInt_FromLong( Gto::Writer::CompressedGTO );
    PyDict_SetItemString( classMembers, "CompressedGTO", value );
    Py_DECREF(value);

    INIT_REFCNT(WriterType);
    WriterType.tp_name = "_gto.Writer";
    WriterType.tp_basicsize = sizeof(PyWriter);
    WriterType.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE;
    WriterType.tp_new = Writer_new;
    WriterType.tp_init = Writer_init;
    WriterType.tp_dealloc = Writer_dealloc;
    WriterType.tp_methods = WriterMethods;
    WriterType.tp_doc = WriterDocString;
    WriterType.tp_dict = classMembers;

    if (PyType_Ready(&WriterType) < 0)
    {
        return false;
    }

    Py_INCREF(&WriterType);
    PyModule_AddObject(module, "Writer", (PyObject *)&WriterType);

    return true;
}

}; // End namespace PyGto
