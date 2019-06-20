#ifndef __ROP_GtoExport_object_h__
#define __ROP_GtoExport_object_h__

#include <UT/UT_Version.h>
#if UT_MAJOR_VERSION_INT >= 12
#  include <GA/GA_Types.h>
#endif
#include <UT/UT_Matrix4.h>
#include <UT/UT_BoundingBox.h>
#include <vector>

class HouGtoWriter;
class GU_Detail;

class Object
{
  public:

#if UT_MAJOR_VERSION_INT >= 12
    static const std::vector<GA_Offset> NoPrimitives;
#else    
    static const std::vector<unsigned int> NoPrimitives;
#endif
    
    Object();
    virtual ~Object();
    
#if UT_MAJOR_VERSION_INT >= 12
    virtual bool declare(HouGtoWriter *writer, GU_Detail *gdp=0, const std::vector<GA_Offset> &prims=NoPrimitives);
#else
    virtual bool declare(HouGtoWriter *writer, GU_Detail *gdp=0, const std::vector<unsigned int> &prims=NoPrimitives);
#endif
    virtual void write(HouGtoWriter *writer);
    
    virtual void invalidate();
    virtual void update();
    virtual void mergeBounds(const UT_Matrix4 &space, UT_BoundingBox &box);
    virtual bool isValid();
};

#endif
