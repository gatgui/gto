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

#include "gtoHeader.h"

namespace PyGto {
using namespace std;

// *****************************************************************************
//          ObjectInfo Class
// *****************************************************************************

// *****************************************************************************
// Implements ObjectInfo.__init__(self)
PyObject *ObjectInfo_init( PyObject *_self, PyObject *args )
{
    Py_INCREF( Py_None );
    return Py_None;
}

// *****************************************************************************
// Implements ObjectInfo.__repr__(self)
PyObject *ObjectInfo_repr( PyObject *_self, PyObject *args )
{
    PyObject *self;

    if( ! PyArg_ParseTuple( args, "O", &self ) )
    {
        // Invalid parameters, let Python do a stack trace
        return NULL;
    }
    
    PyObject *reprStr = NULL;
    
    PyObject *name = PyObject_GetAttrString( self, "name" );
    
    if( name == NULL )
    {
        reprStr = PyString_FromString( "<INVALID ObjectInfo object>" );
    }
    else
    {
      reprStr = PyString_FromFormat( "<ObjectInfo: '%s'>", PyString_AsString( name ) );
      Py_DECREF( name );
    }
    
    return reprStr;
}

// *****************************************************************************
PyObject *newObjectInfo( Gto::Reader *reader, const Gto::Reader::ObjectInfo &oi )
{
    PyObject *module = PyImport_AddModule( "_gto" );
    PyObject *moduleDict = PyModule_GetDict( module );
            
    PyObject *classObj = PyDict_GetItemString( moduleDict, "ObjectInfo" );

    PyObject *args = Py_BuildValue( "()" ); // Empty tuple
    PyObject *objInfo = PyInstance_New( classObj, args, NULL );
    Py_DECREF(args);  
    
    PyObject *val;
    
    val = PyString_FromString( reader->stringFromId( oi.name ).c_str() );
    PyObject_SetAttrString( objInfo, "name", val );
    Py_DECREF(val);
    
    val = PyString_FromString( reader->stringFromId( oi.protocolName ).c_str() );
    PyObject_SetAttrString( objInfo, "protocolName", val );
    Py_DECREF(val);
    
    val = PyInt_FromLong( oi.protocolVersion );
    PyObject_SetAttrString( objInfo, "protocolVersion", val );
    Py_DECREF(val);
    
    val = PyInt_FromLong( oi.numComponents );
    PyObject_SetAttrString( objInfo, "numComponents", val );
    Py_DECREF(val);
    
    val = PyInt_FromLong( oi.pad );
    PyObject_SetAttrString( objInfo, "pad", val );
    Py_DECREF(val);
    
    // Since Gto::Reader::accessObject() __requires__ that the objectInfo
    // reference given to it be from the reader's cache, not just a copy
    // with the same information, we store it here.
    val = PyCObject_FromVoidPtr( (void *)&oi, NULL );
    PyObject_SetAttrString( objInfo, "__objInfoPtr", val );
    Py_DECREF(val);
    
    return objInfo;
}


// *****************************************************************************
//          ComponentInfo Class
// *****************************************************************************

// *****************************************************************************
// Implements ComponentInfo.__repr__(self)
PyObject *ComponentInfo_init( PyObject *_self, PyObject *args )
{
    Py_INCREF( Py_None );
    return Py_None;
}

// *****************************************************************************
// Implements ComponentInfo.__repr__(self)
PyObject *ComponentInfo_repr( PyObject *_self, PyObject *args )
{
    PyObject *self;

    if( ! PyArg_ParseTuple( args, "O", &self ) )
    {
        // Invalid parameters, let Python do a stack trace
        return NULL;
    }
    
    PyObject *reprStr = NULL;
    PyObject *name = PyObject_GetAttrString( self, "name" );
    
    if( name == NULL )
    {
        reprStr = PyString_FromString( "<INVALID ComponentInfo object>" );
    }
    else
    {
        reprStr = PyString_FromFormat( "<ComponentInfo: '%s'>", PyString_AsString( name ) );
        Py_DECREF( name );
    }
    
    return reprStr;
}

// *****************************************************************************
PyObject *newComponentInfo( Gto::Reader *reader,
                            const Gto::Reader::ComponentInfo &ci )
{
    PyObject *module = PyImport_AddModule( "_gto" );
    PyObject *moduleDict = PyModule_GetDict( module );
            
    PyObject *classObj = PyDict_GetItemString( moduleDict, "ComponentInfo" );

    PyObject *args = Py_BuildValue( "()" ); // Empty tuple
    PyObject *compInfo = PyInstance_New( classObj, args, NULL );
    Py_DECREF(args);  
    
    PyObject *val;
    
    val = PyString_FromString( reader->stringFromId( ci.name ).c_str() );
    PyObject_SetAttrString( compInfo, "name", val );
    Py_DECREF(val);
    
    val = PyInt_FromLong( ci.numProperties );
    PyObject_SetAttrString( compInfo, "numProperties", val );
    Py_DECREF(val);
    
    val = PyInt_FromLong( ci.flags );
    PyObject_SetAttrString( compInfo, "flags", val );
    Py_DECREF(val);
    
    val = PyString_FromString( reader->stringFromId( ci.interpretation ).c_str() );
    PyObject_SetAttrString( compInfo, "interpretation", val );
    Py_DECREF(val);
    
    val = PyInt_FromLong( ci.pad );
    PyObject_SetAttrString( compInfo, "pad", val );
    Py_DECREF(val);

    val = newObjectInfo( reader, (*ci.object) );
    PyObject_SetAttrString( compInfo, "object", val );
    Py_DECREF(val);
    
    return compInfo;
}


// *****************************************************************************
//          PropertyInfo Class
// *****************************************************************************

// *****************************************************************************
// Implements PropertyInfo.__init__(self)
PyObject *PropertyInfo_init( PyObject *_self, PyObject *args )
{
    Py_INCREF( Py_None );
    return Py_None;
}

// *****************************************************************************
// Implements PropertyInfo.__repr__(self)
PyObject *PropertyInfo_repr( PyObject *_self, PyObject *args )
{
    PyObject *self;

    if( ! PyArg_ParseTuple( args, "O", &self ) )
    {
        // Invalid parameters, let Python do a stack trace
        return NULL;
    }
    
    PyObject *reprStr = NULL;
    PyObject *name = PyObject_GetAttrString( self, "name" );
    
    if( name == NULL )
    {
        reprStr = PyString_FromString( "<INVALID PropertyInfo object>" );
    }
    else
    {
        reprStr = PyString_FromFormat( "<PropertyInfo: '%s'>", PyString_AsString( name ) );
        Py_DECREF(name);
    }
    
    return reprStr;
}

// *****************************************************************************
PyObject *newPropertyInfo( Gto::Reader *reader,
                           const Gto::Reader::PropertyInfo &pi )
{
    PyObject *module = PyImport_AddModule( "_gto" );
    PyObject *moduleDict = PyModule_GetDict( module );
    
    PyObject *classObj = PyDict_GetItemString( moduleDict, "PropertyInfo" );
    
    PyObject *args = Py_BuildValue( "()" ); // Empty tuple
    PyObject *propInfo = PyInstance_New( classObj, args, NULL );
    Py_DECREF(args);
    
    PyObject *val;
    
    val = PyString_FromString( reader->stringFromId( pi.name ).c_str() );
    PyObject_SetAttrString( propInfo, "name", val );
    Py_DECREF(val);
    
    val = PyInt_FromLong( pi.size );
    PyObject_SetAttrString( propInfo, "size", val );
    Py_DECREF(val);
    
    val = PyInt_FromLong( pi.type );
    PyObject_SetAttrString( propInfo, "type", val );
    Py_DECREF(val);
    
    val = PyInt_FromLong( pi.width );
    PyObject_SetAttrString( propInfo, "width", val );
    Py_DECREF(val);
    
    val = PyString_FromString( reader->stringFromId( pi.interpretation ).c_str() );
    PyObject_SetAttrString( propInfo, "interpretation", val );
    Py_DECREF(val);
    
    val = PyInt_FromLong( pi.pad );
    PyObject_SetAttrString( propInfo, "pad", val );
    Py_DECREF(val);
    
    val = newComponentInfo( reader, (*pi.component) );
    PyObject_SetAttrString( propInfo, "component", val );
    Py_DECREF(val);
    
    return propInfo;
}


}  //  End namespace PyGto
