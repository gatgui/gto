#ifndef __py3compat_h__
#define __py3compat_h__

#include <string>

#if PY_MAJOR_VERSION >= 3

    #define PyInt_FromLong PyLong_FromLong
    #define PyInt_AsLong PyLong_AsLong
    #define PyInt_FromString PyLong_FromString
    #define PyInt_Check PyLong_Check
    #define PyString_FromString PyUnicode_FromString
    #define PyString_FromFormat PyUnicode_FromFormat
    #define PyString_Check PyUnicode_Check

    inline std::string PyString_AsStdString(PyObject *str)
    {
        PyObject *estr = PyUnicode_AsUTF8String(str);
        std::string rv = PyBytes_AsString(estr);
        Py_DECREF(estr);
        return rv;
    }

    #define INIT_REFCNT(type) type.ob_base.ob_base.ob_refcnt = 1
    #define MOD_INIT(name) PyObject* PyInit_ ## name()
    #define MOD_RETURN(v) return v
    #define MOD_NEW(module, name, methods, doc)\
        static PyModuleDef name ## _moddef = {\
            PyModuleDef_HEAD_INIT,\
            #name,\
            doc,\
            -1,\
            methods,\
            NULL,\
            NULL,\
            NULL,\
            NULL\
        };\
        PyObject *module = PyModule_Create(&name ## _moddef)

#else
    
    inline std::string PyString_AsStdString(PyObject *str)
    {
        return std::string(PyString_AsString(str));
    }

    #ifndef PyVarObject_HEAD_INIT
    #   define PyVarObject_HEAD_INIT(type, size) PyObject_HEAD_INIT(type) size,
    #endif

    #define INIT_REFCNT(type) type.ob_refcnt = 1
    #define MOD_INIT(name) void init ## name(void)
    #define MOD_NEW(module, name, methods, doc) PyObject *module = Py_InitModule3( #name, methods, doc )
    #define MOD_RETURN(v) return

#endif

#endif
