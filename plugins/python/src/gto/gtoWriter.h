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

#ifndef __GTOWRITER_H__
#define __GTOWRITER_H__

#include "gtomodule.h"
#include <Gto/Writer.h>
#include <string>
#include <vector>

namespace PyGto {

typedef struct
{
    PyObject_HEAD
    // C++ Writer instance to use
    Gto::Writer *m_writer;
    // property currently being written
    int m_propCount;
    // For graceful sanity checking...
    bool m_beginDataCalled;
    bool m_objectDef;
    bool m_componentDef;
    std::vector<std::string> *m_propertyNames;
} PyWriter;

bool initWriter(PyObject *module);

// *****************************************************************************
// Since Python can't convert directly to int...
inline int PyInt_AsInt( PyObject *p )
{
    return (int)PyInt_AsLong( p );
}

// *****************************************************************************
// Since Python can't convert directly to float...
inline float PyFloat_AsFloat( PyObject *p )
{
    return (float)PyFloat_AsDouble( p );
}

// *****************************************************************************
// Since Python can't convert directly to short...
inline unsigned short PyInt_AsShort( PyObject *p )
{
    return (unsigned short)PyInt_AsLong( p );
}

// *****************************************************************************
// Since Python can't convert directly to char...
inline unsigned char PyInt_AsByte( PyObject *p )
{
    return (unsigned char)PyInt_AsLong( p );
}

}; // End namespace PyGto


#endif    // End #ifdef __GTOWRITER_H__
