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

//
// This is the gto module for Python 2.2.1.  It may or may not work with other
// versions.
//

#include "gtomodule.h"
#include "gtoHeader.h"
#include "gtoReader.h"
#include "gtoWriter.h"

namespace PyGto {

// A python exception object
static PyObject *g_gtoError = NULL;

// *****************************************************************************
// Just returns a pointer to the module-wide g_gtoError object
PyObject *gtoError()
{
    return g_gtoError;
}

// *****************************************************************************
// Returns the Python type name of an object as a string
std::string PyTypeName( PyObject *object )
{
    // Figure out the class name (as a string)
    PyObject *itemClass = PyObject_GetAttrString( object, "__class__" );
    assert( itemClass != NULL );
    
    PyObject *itemClassName = PyObject_GetAttrString( itemClass, "__name__" );
    assert( itemClassName != NULL );
    
    std::string rv = PyString_AsStdString( itemClassName );
    
    Py_DECREF(itemClass);
    Py_DECREF(itemClassName);
    
    return rv;
}

} // End namespace PyGto


// *****************************************************************************
// This function is called by Python when the module is imported
extern "C" {

#ifdef _WIN32
  __declspec(dllexport)
#else
  __attribute__ ((visibility ("default")))
#endif 
  MOD_INIT(_gto)
  {
      // Create a new gto module object
      MOD_NEW( module, _gto, NULL, "gto I/O module  v3.5.4\n(c) 2003 Tweak Films" );
      
      PyModule_AddIntConstant(module, "Boolean", Gto::Boolean);
      PyModule_AddIntConstant(module, "Byte", Gto::Byte);
      PyModule_AddIntConstant(module, "Short", Gto::Short);
      PyModule_AddIntConstant(module, "Int", Gto::Int);
      PyModule_AddIntConstant(module, "Half", Gto::Half);
      PyModule_AddIntConstant(module, "Float", Gto::Float);
      PyModule_AddIntConstant(module, "Double", Gto::Double);
      PyModule_AddIntConstant(module, "String", Gto::String);
      PyModule_AddIntConstant(module, "ErrorType", Gto::ErrorType);
      PyModule_AddIntConstant(module, "Matrix", Gto::Matrix);
      PyModule_AddIntConstant(module, "Transposed", Gto::Transposed);
      PyModule_AddIntConstant(module, "GTO_VERSION", GTO_VERSION);

      // Create the exception and add it to the module
      PyGto::g_gtoError = PyErr_NewException( (char*)"_gto.Error", NULL, NULL );
      PyModule_AddObject( module, "Error", PyGto::g_gtoError );

      // Create info classes
      if (!PyGto::initObjectInfo(module))
      {
          MOD_RETURN(NULL);
      }
      if (!PyGto::initComponentInfo(module))
      {
          MOD_RETURN(NULL);
      }
      if (!PyGto::initPropertyInfo(module))
      {
          MOD_RETURN(NULL);
      }
      if (!PyGto::initReader(module))
      {
          MOD_RETURN(NULL);
      }
      if (!PyGto::initWriter(module))
      {
          MOD_RETURN(NULL);
      }

      MOD_RETURN(module);
  }
}
