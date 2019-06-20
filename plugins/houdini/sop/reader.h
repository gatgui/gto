#ifndef __SOP_GtoImport_reader_h_
#define __SOP_GtoImport_reader_h_

#include "object.h"
#include "utils.h"
#include <UT/UT_Interrupt.h>

struct ExpandOptions
{
   bool computeMirror;
   bool computeInstance;
   bool separateInstance;
   bool ignoreTransforms;
   bool ignoreVisibility;
   float globalScale;
   std::string shapeName;

   UT_Interrupt *progress;
   Reader *frame0;
   Reader *frame1;
};

class Reader : public Gto::Reader
{
  public:
    
    Reader(const std::string &shapeName, bool boundsOnly, bool asDiff, unsigned int mode=0);
    virtual ~Reader();
    
    
    virtual Gto::Reader::Request object(const std::string& name,
                                        const std::string& protocol,
                                        unsigned int protocolVersion,
                                        const Gto::Reader::ObjectInfo &header);
    
    virtual Gto::Reader::Request component(const std::string& name,
                                           const std::string& interp,
                                           const Gto::Reader::ComponentInfo &header);
    
    virtual Gto::Reader::Request property(const std::string& name,
                                          const std::string& interp,
                                          const Gto::Reader::PropertyInfo &header);
    
    virtual void* data(const Gto::Reader::PropertyInfo &header, size_t bytes);
    
    virtual void dataRead(const Gto::Reader::PropertyInfo &header);
    
    
    Object* findObject(const std::string &name);
    
    bool toHoudini(OP_Context &ctx, GU_Detail *gdp, UT_Interrupt *progress, const ExpandOptions &opts, Reader *frm0=0, Reader *frm1=0, float blend=0.0f, float t0=0.0f, float t1=0.0f);
    
    
    void setTransform(float frame, const std::string &name, const UT_Matrix4 &mtx);
    void setParent(const std::string &name, const std::string &pname);
    
    inline bool isFrame() const { return mIsFrame; }
    inline void setFrame(float f) { mFrame = f; mIsFrame = true; }
    inline float getFrame() const { return mFrame; }
    
  private:
    
    bool toHoudini(OP_Context &ctx, GU_Detail *gdp, UT_Interrupt *progress, const ExpandOptions &opts, std::set<Object*> &processed, Object *obj, Reader *frm0, Reader *frm1, float blend, float t0, float t1);
    bool toHoudini(OP_Context &ctx, GU_Detail *gdp, const ExpandOptions &opts, Object *obj, Reader *frm0, Reader *frm1, float blend, float t0, float t1);
    
    
  private:
    
    Object::Set mObjects;
    bool mAsDiff;
    bool mBoundsOnly;
    std::map<float, TransformsInfo> mTInfo;
    ParentsInfo mPInfo;
    std::string mShapeName;
    float mFrame;
    bool mIsFrame;
};

#endif

