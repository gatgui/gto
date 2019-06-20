#ifndef __ROP_GtoExport_mesh_h__
#define __ROP_GtoExport_mesh_h__

#include "object.h"
#include "userattr.h"
#include <UT/UT_Vector3.h>
#include <GU/GU_Detail.h>
#if UT_MAJOR_VERSION_INT >= 12
#  include <GA/GA_Group.h>
#else
#  include <GB/GB_Group.h>
#endif
#include <Gto/Header.h>
#include <Gto/Writer.h>
#include <vector>
#include <map>
#include <string>

class Transform;

class Mesh : public Object
{
  public:
    
    Mesh(Transform *p=0);
    virtual ~Mesh();
    
#if UT_MAJOR_VERSION_INT >= 12
    virtual bool declare(HouGtoWriter *writer, GU_Detail *gdp=0, const std::vector<GA_Offset> &prims=NoPrimitives);
#else    
    virtual bool declare(HouGtoWriter *writer, GU_Detail *gdp=0, const std::vector<unsigned int> &prims=NoPrimitives);
#endif
    virtual void write(HouGtoWriter *writer);
    
    virtual bool isValid();
    virtual void invalidate();
    virtual void update();
    virtual void mergeBounds(const UT_Matrix4 &space, UT_BoundingBox &box);
    
    
    void setName(const std::string &name);
    const std::string& getName() const;
    
    void setParent(Transform *t);
    Transform* getParent() const;
    
    void cleanup();
    
    
    
  protected:
    
    void findUVAndColourSetNames(GU_Detail *gdp);

  protected:
    
    Transform *mParent;
    std::string mName;
    
    unsigned int mNumFaceVertex;
    unsigned int mNumVertex;
    
    float mBBox[6];
    
    std::vector<float> mPositions;
    std::vector<unsigned short> mPolyVertexCount;
    std::vector<int> mPolyVertexIndices;
#if UT_MAJOR_VERSION_INT >= 12
    std::map<GA_Offset, int> mVertexIndicesMap;
#else
    std::map<int, int> mVertexIndicesMap;
#endif
    
    std::map<std::string, std::string> mColourSetNamesMap;
    std::map<std::string, std::string> mUVSetNamesMap;
    std::map<std::string, std::vector<std::string> > mUsedUVSets;
    std::map<std::string, std::vector<std::string> > mUsedColourSets;
    
    AttribDict mVertexAttrib;
    AttribDict mPointAttrib;
    AttribDict mPrimAttrib;
    AttribDict mDetailAttrib;
    AttribDict mUVSets;
    AttribDict mColourSets;
    Attrib mNormals;
    Attrib mHardEdges;
    
    bool mDirty;
    
    bool mExportVelocityInfo;
    float mMinVelocity[3];
    float mMaxVelocity[3];
    float mMaxVelocityMag;
};

#endif


