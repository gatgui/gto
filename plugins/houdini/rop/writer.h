#ifndef __ROP_GtoExport_writer_h__
#define __ROP_GtoExport_writer_h__

#include <Gto/Writer.h>
#include <UT/UT_Version.h>
#if UT_MAJOR_VERSION_INT >= 12
#  include <GA/GA_Types.h>
#endif
#include <OP/OP_Context.h>
#include <vector>
#include "utils.h"

class Object;
class Transform;
class OBJ_Node;

class HouGtoWriter : public Gto::Writer
{
  public:
    
    struct ShapeInfo
    {
      enum Type
      {
        MESH = 0,
        CURVE,
        PARTICLES,
        NONE
      } type;
      
      std::string name;
#if UT_MAJOR_VERSION_INT >= 12
      std::vector<GA_Offset> prims;
#else
      std::vector<unsigned int> prims;
#endif
    };
    
    HouGtoWriter(float time, bool asDiff=false, float scale=1.0f, bool ignorePrimGroups=false);
    virtual ~HouGtoWriter();
    
    bool addSOP(const std::string &sopPath);
    void write();
    
    OP_Context& getContext();
    
    bool isDiff() const;
    
    inline const std::string& getErrorString() const
    {
      return mErrStr;
    }

    inline float getGlobalScale() const
    {
      return mGlobalScale;
    }
    
  protected:
    
    void cleanup();
    
    std::string getUniqueName(const std::string &name, int *offset=0) const;
    bool declareShapes(const GU_Detail *gdp, const std::string &bname, Transform *parent=0);
    Object* declareTransforms(const GU_Detail *gdp, OBJ_Node *obj, Transform *root, const std::string &nname);
    
  protected:
    
    bool mIsDiff;
    std::vector<Object*> mObjects;
    std::map<std::string, Object*> mObjectMap;
    OP_Context mCtx;
    ParentsInfo mPInfo;
    TransformsInfo mTInfo;
    std::string mErrStr;
    float mGlobalScale;
    bool mIgnorePrimitiveGroups;
};


#endif

