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

// *****************************************************************************
//          ObjectInfo Class
// *****************************************************************************

static int ObjectInfo_init( PyObject *self, PyObject *, PyObject * )
{
    PyObjectInfo *oi = (PyObjectInfo*) self;
    oi->mName = PyString_FromString("");
    oi->mProtocol = PyString_FromString("");
    oi->mProtocolVersion = PyInt_FromLong(0);
    oi->mNumComponents = PyInt_FromLong(0);
    oi->mPad = PyInt_FromLong(0);
    oi->mInfo = NULL;
    oi->mComponentsOffset = PyInt_FromLong(0);
    return 0;
}

static void ObjectInfo_dealloc(PyObject* self)
{
    PyObjectInfo *poi = (PyObjectInfo*) self;
    Py_XDECREF(poi->mName);
    Py_XDECREF(poi->mProtocol);
    Py_XDECREF(poi->mProtocolVersion);
    Py_XDECREF(poi->mNumComponents);
    Py_XDECREF(poi->mPad);
    Py_XDECREF(poi->mComponentsOffset);
    poi->mInfo = NULL;
    Py_TYPE(self)->tp_free(self);
}

static PyObject *ObjectInfo_repr( PyObject *self )
{
    PyObjectInfo *oi = (PyObjectInfo*) self;

    std::string cname = PyString_AsStdString( oi->mName );
    return PyString_FromFormat( "<ObjectInfo: '%s'>",  cname.c_str());
}

static PyObject* ObjectInfo_getName(PyObject *self, void *)
{
    PyObjectInfo *oi = (PyObjectInfo*) self;
    Py_INCREF(oi->mName);
    return oi->mName;
}

static PyObject* ObjectInfo_getProtocolName(PyObject *self, void *)
{
    PyObjectInfo *oi = (PyObjectInfo*) self;
    Py_INCREF(oi->mProtocol);
    return oi->mProtocol;
}

static PyObject* ObjectInfo_getProtocolVersion(PyObject *self, void *)
{
    PyObjectInfo *oi = (PyObjectInfo*) self;
    Py_INCREF(oi->mProtocolVersion);
    return oi->mProtocolVersion;
}

static PyObject* ObjectInfo_getNumComponents(PyObject *self, void *)
{
    PyObjectInfo *oi = (PyObjectInfo*) self;
    Py_INCREF(oi->mNumComponents);
    return oi->mNumComponents;
}

static PyObject* ObjectInfo_getPad(PyObject *self, void *)
{
    PyObjectInfo *oi = (PyObjectInfo*) self;
    Py_INCREF(oi->mPad);
    return oi->mPad;
}

static PyObject* ObjectInfo_getComponentsOffset(PyObject *self, void *)
{
    PyObjectInfo *oi = (PyObjectInfo*) self;
    Py_INCREF(oi->mComponentsOffset);
    return oi->mComponentsOffset;
}

static PyGetSetDef ObjectInfoGetSet[] =
{
    {(char*)"name", ObjectInfo_getName, NULL, NULL, NULL},
    {(char*)"protocolName", ObjectInfo_getProtocolName, NULL, NULL, NULL},
    {(char*)"protocolVersion", ObjectInfo_getProtocolVersion, NULL, NULL, NULL},
    {(char*)"numComponents", ObjectInfo_getNumComponents, NULL, NULL, NULL},
    {(char*)"pad", ObjectInfo_getPad, NULL, NULL, NULL},
    {(char*)"coffset", ObjectInfo_getComponentsOffset, NULL, NULL, NULL},
    {NULL, NULL, NULL, NULL, NULL},
};

PyTypeObject ObjectInfoType;

bool initObjectInfo(PyObject *module)
{
    memset(&ObjectInfoType, 0, sizeof(PyTypeObject));

    INIT_REFCNT(ObjectInfoType);
    ObjectInfoType.tp_name = (char*) "_gto.ObjectInfo";
    ObjectInfoType.tp_basicsize = sizeof(PyObjectInfo);
    ObjectInfoType.tp_flags = Py_TPFLAGS_DEFAULT;
    ObjectInfoType.tp_new = PyType_GenericNew;
    ObjectInfoType.tp_init = ObjectInfo_init;
    ObjectInfoType.tp_dealloc = ObjectInfo_dealloc;
    ObjectInfoType.tp_getset = ObjectInfoGetSet;
    ObjectInfoType.tp_repr = ObjectInfo_repr;

    if (PyType_Ready(&ObjectInfoType) < 0)
    {
        return false;
    }

    Py_INCREF(&ObjectInfoType);
    PyModule_AddObject(module, "ObjectInfo", (PyObject *)&ObjectInfoType);

    return true;
}

PyObject *newObjectInfo( Gto::Reader *reader, const Gto::Reader::ObjectInfo &oi )
{
    PyObject *self = PyObject_CallObject( (PyObject*) &ObjectInfoType, NULL );

    PyObjectInfo *poi = (PyObjectInfo*) self;

    Py_DECREF(poi->mName);
    poi->mName = PyString_FromString( reader->stringFromId( oi.name ).c_str() );
    
    Py_DECREF(poi->mProtocol);
    poi->mProtocol = PyString_FromString( reader->stringFromId( oi.protocolName ).c_str() );
    
    Py_DECREF(poi->mProtocolVersion);
    poi->mProtocolVersion = PyInt_FromLong( oi.protocolVersion );
    
    Py_DECREF(poi->mNumComponents);
    poi->mNumComponents = PyInt_FromLong( oi.numComponents );
    
    Py_DECREF(poi->mPad);
    poi->mPad = PyInt_FromLong( oi.pad );

    Py_DECREF(poi->mComponentsOffset);
    poi->mComponentsOffset = PyInt_FromLong( oi.componentOffset() );
    
    // Since Gto::Reader::accessObject() __requires__ that the objectInfo
    // reference given to it be from the reader's cache, not just a copy
    // with the same information, we store it here.
    poi->mInfo = &oi;
    
    return self;
}


// *****************************************************************************
//          ComponentInfo Class
// *****************************************************************************

static int ComponentInfo_init( PyObject *self, PyObject *, PyObject * )
{
    PyComponentInfo *pci = (PyComponentInfo*) self;

    pci->mName = PyString_FromString("");
    pci->mInterpretation = PyString_FromString("");
    pci->mFlags = PyInt_FromLong(0);
    pci->mNumProperties = PyInt_FromLong(0);
    pci->mPad = PyInt_FromLong(0);
    pci->mObjInfo = Py_None;
    pci->mPropertiesOffset = PyInt_FromLong(0);
    Py_INCREF(Py_None);
    pci->mInfo = NULL;
    return 0;
}

static void ComponentInfo_dealloc(PyObject* self)
{
    PyComponentInfo *pci = (PyComponentInfo*) self;

    Py_XDECREF(pci->mName);
    Py_XDECREF(pci->mInterpretation);
    Py_XDECREF(pci->mFlags);
    Py_XDECREF(pci->mNumProperties);
    Py_XDECREF(pci->mPad);
    Py_XDECREF(pci->mObjInfo);
    Py_XDECREF(pci->mPropertiesOffset);
    pci->mInfo = NULL;
    Py_TYPE(self)->tp_free(self);
}

static PyObject *ComponentInfo_repr( PyObject *self )
{
    PyComponentInfo *pci = (PyComponentInfo*) self;
    
    std::string cname = PyString_AsStdString( pci->mName );
    return PyString_FromFormat( "<ComponentInfo: '%s'>", cname.c_str() );
}

static PyObject* ComponentInfo_getName(PyObject *self, void*)
{
    PyComponentInfo *pci = (PyComponentInfo*) self;
    Py_INCREF(pci->mName);
    return pci->mName;
}

static PyObject* ComponentInfo_getInterpretation(PyObject *self, void*)
{
    PyComponentInfo *pci = (PyComponentInfo*) self;
    Py_INCREF(pci->mInterpretation);
    return pci->mInterpretation;
}

static PyObject* ComponentInfo_getFlags(PyObject *self, void*)
{
    PyComponentInfo *pci = (PyComponentInfo*) self;
    Py_INCREF(pci->mFlags);
    return pci->mFlags;
}

static PyObject* ComponentInfo_getPad(PyObject *self, void*)
{
    PyComponentInfo *pci = (PyComponentInfo*) self;
    Py_INCREF(pci->mPad);
    return pci->mPad;
}

static PyObject* ComponentInfo_getNumProperties(PyObject *self, void*)
{
    PyComponentInfo *pci = (PyComponentInfo*) self;
    Py_INCREF(pci->mNumProperties);
    return pci->mNumProperties;
}

static PyObject* ComponentInfo_getObject(PyObject *self, void*)
{
    PyComponentInfo *pci = (PyComponentInfo*) self;
    Py_INCREF(pci->mObjInfo);
    return pci->mObjInfo;
}

static PyObject* ComponentInfo_getPropertiesOffset(PyObject *self, void*)
{
    PyComponentInfo *pci = (PyComponentInfo*) self;
    Py_INCREF(pci->mPropertiesOffset);
    return pci->mPropertiesOffset;
}

static PyGetSetDef ComponentInfoGetSet[] =
{
    {(char*)"name", ComponentInfo_getName, NULL, NULL, NULL},
    {(char*)"interpretation", ComponentInfo_getInterpretation, NULL, NULL, NULL},
    {(char*)"pad", ComponentInfo_getPad, NULL, NULL, NULL},
    {(char*)"flags", ComponentInfo_getFlags, NULL, NULL, NULL},
    {(char*)"numProperties", ComponentInfo_getNumProperties, NULL, NULL, NULL},
    {(char*)"object", ComponentInfo_getObject, NULL, NULL, NULL},
    {(char*)"poffset", ComponentInfo_getPropertiesOffset, NULL, NULL, NULL},
    {NULL, NULL, NULL, NULL, NULL},
};

PyTypeObject ComponentInfoType;

bool initComponentInfo(PyObject *module)
{
    memset(&ComponentInfoType, 0, sizeof(PyTypeObject));

    INIT_REFCNT(ComponentInfoType);
    ComponentInfoType.tp_name = (char*) "_gto.ComponentInfo";
    ComponentInfoType.tp_basicsize = sizeof(PyComponentInfo);
    ComponentInfoType.tp_flags = Py_TPFLAGS_DEFAULT;
    ComponentInfoType.tp_new = PyType_GenericNew;
    ComponentInfoType.tp_init = ComponentInfo_init;
    ComponentInfoType.tp_dealloc = ComponentInfo_dealloc;
    ComponentInfoType.tp_getset = ComponentInfoGetSet;
    ComponentInfoType.tp_repr = ComponentInfo_repr;

    if (PyType_Ready(&ComponentInfoType) < 0)
    {
        return false;
    }

    Py_INCREF(&ComponentInfoType);
    PyModule_AddObject(module, "ComponentInfo", (PyObject *)&ComponentInfoType);

    return true;
}

PyObject *newComponentInfo( Gto::Reader *reader,
                            const Gto::Reader::ComponentInfo &ci )
{
    PyObject *self = PyObject_CallObject( (PyObject*) &ComponentInfoType, NULL );

    PyComponentInfo *pci = (PyComponentInfo*) self;

    Py_DECREF(pci->mName);
    pci->mName = PyString_FromString( reader->stringFromId( ci.name ).c_str() );
    
    Py_DECREF(pci->mInterpretation);
    pci->mInterpretation = PyString_FromString( reader->stringFromId( ci.interpretation ).c_str() );
    
    Py_DECREF(pci->mFlags);
    pci->mFlags = PyInt_FromLong( ci.flags );
    
    Py_DECREF(pci->mNumProperties);
    pci->mNumProperties = PyInt_FromLong( ci.numProperties );
    
    Py_DECREF(pci->mPad);
    pci->mPad = PyInt_FromLong( ci.pad );
    
    Py_DECREF(pci->mPropertiesOffset);
    pci->mPropertiesOffset = PyInt_FromLong( ci.propertyOffset() );

    Py_DECREF(pci->mObjInfo);
    pci->mObjInfo = newObjectInfo( reader, (*ci.object) );

    pci->mInfo = &ci;
    
    return self;
}


// *****************************************************************************
//          PropertyInfo Class
// *****************************************************************************

static int PropertyInfo_init( PyObject *self, PyObject *, PyObject * )
{
    PyPropertyInfo *ppi = (PyPropertyInfo*) self;

    ppi->mName = PyString_FromString("");
    ppi->mInterpretation = PyString_FromString("");
    ppi->mType = PyInt_FromLong(Gto::ErrorType);
    ppi->mSize = PyInt_FromLong(0);
    ppi->mWidth = PyInt_FromLong(0);
    ppi->mPad = PyInt_FromLong(0);
    ppi->mCompInfo = Py_None;
    ppi->mOffset = PyInt_FromLong(0);
    Py_INCREF(Py_None);
    ppi->mInfo = NULL;
    return 0;
}

static void PropertyInfo_dealloc(PyObject *self)
{
    PyPropertyInfo *ppi = (PyPropertyInfo*) self;

    Py_XDECREF(ppi->mName);
    Py_XDECREF(ppi->mInterpretation);
    Py_XDECREF(ppi->mType);
    Py_XDECREF(ppi->mSize);
    Py_XDECREF(ppi->mWidth);
    Py_XDECREF(ppi->mPad);
    Py_XDECREF(ppi->mCompInfo);
    Py_XDECREF(ppi->mOffset);
    ppi->mInfo = NULL;
    Py_TYPE(self)->tp_free(self);
}

static PyObject *PropertyInfo_repr( PyObject *self )
{
    PyPropertyInfo *ppi = (PyPropertyInfo*) self;

    std::string cname = PyString_AsStdString( ppi->mName );
    return PyString_FromFormat( "<PropertyInfo: '%s'>", cname.c_str() );
}

static PyObject* PropertyInfo_getName(PyObject *self, void*)
{
    PyPropertyInfo *ppi = (PyPropertyInfo*) self;
    Py_INCREF(ppi->mName);
    return ppi->mName;
}

static PyObject* PropertyInfo_getInterpretation(PyObject *self, void*)
{
    PyPropertyInfo *ppi = (PyPropertyInfo*) self;
    Py_INCREF(ppi->mInterpretation);
    return ppi->mInterpretation;
}

static PyObject* PropertyInfo_getSize(PyObject *self, void*)
{
    PyPropertyInfo *ppi = (PyPropertyInfo*) self;
    Py_INCREF(ppi->mSize);
    return ppi->mSize;
}

static PyObject* PropertyInfo_getType(PyObject *self, void*)
{
    PyPropertyInfo *ppi = (PyPropertyInfo*) self;
    Py_INCREF(ppi->mType);
    return ppi->mType;
}

static PyObject* PropertyInfo_getWidth(PyObject *self, void*)
{
    PyPropertyInfo *ppi = (PyPropertyInfo*) self;
    Py_INCREF(ppi->mWidth);
    return ppi->mWidth;
}

static PyObject* PropertyInfo_getPad(PyObject *self, void*)
{
    PyPropertyInfo *ppi = (PyPropertyInfo*) self;
    Py_INCREF(ppi->mPad);
    return ppi->mPad;
}

static PyObject* PropertyInfo_getComponent(PyObject *self, void*)
{
    PyPropertyInfo *ppi = (PyPropertyInfo*) self;
    Py_INCREF(ppi->mCompInfo);
    return ppi->mCompInfo;
}

static PyObject* PropertyInfo_getOffset(PyObject *self, void*)
{
    PyPropertyInfo *ppi = (PyPropertyInfo*) self;
    Py_INCREF(ppi->mOffset);
    return ppi->mOffset;
}

static PyGetSetDef PropertyInfoGetSet[] =
{
    {(char*)"name", PropertyInfo_getName, NULL, NULL, NULL},
    {(char*)"interpretation", PropertyInfo_getInterpretation, NULL, NULL, NULL},
    {(char*)"pad", PropertyInfo_getPad, NULL, NULL, NULL},
    {(char*)"size", PropertyInfo_getSize, NULL, NULL, NULL},
    {(char*)"width", PropertyInfo_getWidth, NULL, NULL, NULL},
    {(char*)"type", PropertyInfo_getType, NULL, NULL, NULL},
    {(char*)"component", PropertyInfo_getComponent, NULL, NULL, NULL},
    {(char*)"offset", PropertyInfo_getOffset, NULL, NULL, NULL},
    {NULL, NULL, NULL, NULL, NULL}
};

PyTypeObject PropertyInfoType;

bool initPropertyInfo(PyObject *module)
{
    memset(&PropertyInfoType, 0, sizeof(PyTypeObject));

    INIT_REFCNT(PropertyInfoType);
    PropertyInfoType.tp_name = (char*) "_gto.PropertyInfo";
    PropertyInfoType.tp_basicsize = sizeof(PyPropertyInfo);
    PropertyInfoType.tp_flags = Py_TPFLAGS_DEFAULT;
    PropertyInfoType.tp_new = PyType_GenericNew;
    PropertyInfoType.tp_init = PropertyInfo_init;
    PropertyInfoType.tp_dealloc = PropertyInfo_dealloc;
    PropertyInfoType.tp_getset = PropertyInfoGetSet;
    PropertyInfoType.tp_repr = PropertyInfo_repr;

    if (PyType_Ready(&PropertyInfoType) < 0)
    {
        return false;
    }

    Py_INCREF(&PropertyInfoType);
    PyModule_AddObject(module, "PropertyInfo", (PyObject *)&PropertyInfoType);

    return true;
}

PyObject *newPropertyInfo( Gto::Reader *reader,
                           const Gto::Reader::PropertyInfo &pi )
{
    PyObject *self = PyObject_CallObject( (PyObject*) &PropertyInfoType, NULL );

    PyPropertyInfo *ppi = (PyPropertyInfo*) self;

    Py_DECREF(ppi->mName);
    ppi->mName = PyString_FromString( reader->stringFromId( pi.name ).c_str() );
    
    Py_DECREF(ppi->mInterpretation);
    ppi->mInterpretation = PyString_FromString( reader->stringFromId( pi.interpretation ).c_str() );
    
    Py_DECREF(ppi->mType);
    ppi->mType = PyInt_FromLong( pi.type );
    
    Py_DECREF(ppi->mSize);
    ppi->mSize = PyInt_FromLong( pi.size );

    Py_DECREF(ppi->mWidth);
    ppi->mWidth = PyInt_FromLong( pi.width );
    
    Py_DECREF(ppi->mPad);
    ppi->mPad = PyInt_FromLong( pi.pad );

    Py_DECREF(ppi->mOffset);
    ppi->mOffset = PyInt_FromLong( pi.offset );
    
    Py_DECREF(ppi->mCompInfo);
    ppi->mCompInfo = newComponentInfo( reader, (*pi.component) );

    ppi->mInfo = &pi;
    
    return self;
}


}  //  End namespace PyGto
