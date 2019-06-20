#ifndef __SOP_GtoImport_transform_h_
#define __SOP_GtoImport_transform_h_

#include "object.h"
#include <UT/UT_Matrix4.h>

class Transform : public Object
{
  public:
    
    enum
    {
      READ_LOCAL_MATRIX = OBJECT_PROP_MAX,
      TRANSFORM_PROP_MAX
    };
    
    Transform(const std::string &name, bool asDiff=false);
    virtual ~Transform();
    
    virtual Gto::Reader::Request component(Reader *rdr,
                                           const std::string& name,
                                           const std::string& interp,
                                           const Gto::Reader::ComponentInfo &header);
    
    virtual Gto::Reader::Request property(Reader *rdr,
                                          const std::string& name,
                                          const std::string& interp,
                                          const Gto::Reader::PropertyInfo &header);
    
    virtual void* data(Reader *rdr, const Gto::Reader::PropertyInfo &header, size_t bytes);
    
    virtual void dataRead(Reader *rdr, const Gto::Reader::PropertyInfo &header);
    
    
    virtual bool toHoudini(Reader *reader,
                           OP_Context &ctx,
                           GU_Detail *gdp,
                           const ExpandOptions &opts,
                           Object *obj0=0,
                           Object *obj1=0,
                           float blend=0.0f,
                           float t0=0.0f,
                           float t1=0.0f);
    
    virtual void resolveParent(Reader *reader);
    
    Transform* getParent() const;
    
    void setParent(Transform *t);

    virtual bool switchParent(Reader *reader, size_t idx);
    
    const UT_Matrix4& getWorldMatrix() const;
    
    void applySceneScale(UT_Matrix4 &m, float scl) const;

    // which: -1 = prev, 0 = current, 1 = next
    const UT_Matrix4& updateWorldMatrix(float sceneScale, int which);
  
  protected:
    
    Transform *mParent;
    UT_Matrix4 mLocal;
    UT_Matrix4 mWorld;
    UT_Matrix4 mPrevLocal;
    UT_Matrix4 mNextLocal;
};

#endif
