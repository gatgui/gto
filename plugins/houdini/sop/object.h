#ifndef __SOP_GtoImport_object_h_
#define __SOP_GtoImport_object_h_

#include <UT/UT_Version.h>
#include <OP/OP_Context.h>
#include <GU/GU_Detail.h>
#include <Gto/Reader.h>
#include <map>
#include <vector>
#include <string>
#include <set>


class Reader;
struct ExpandOptions;

class Object
{
  public:
    
    enum
    {
      READ_OBJECT = 0,
      OBJECT_COMP_MAX,

      READ_PARENT_NAME = 0,
      READ_VISIBILITY,
      OBJECT_PROP_MAX
    };
  
    typedef std::map<std::string, Object*> Set;
    
    Object(const std::string &name, bool asDiff=false);
    virtual ~Object();
    
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
    
    
    Object* getParentObject();
    
    const std::string& getName() const;
    
    bool isDiff() const;
    
    bool isVisible(int which=0) const;

    void addChild(Object *o);

    size_t numChildren() const;
    
    Object* getChild(size_t idx) const;

    void cleanup();

    size_t numParents() const;

    int getParentIndex(const std::string &name) const;

    virtual bool switchParent(Reader *reader, size_t idx);

    inline int getInstanceNumber() const { return mInstanceNum; }
    
    
  protected:
    
    void setParentObject(Object *o);
    
    
  protected:

    std::string mName;
    bool mAsDiff;
    Object *mParentObject;
    std::vector<Object*> mChildren;
    int mNumParents;
    int *mParentNameIndices;
    std::vector<std::string> mParentNames;
    int mInstanceNum;
    Gto::uint8 mVisibility;
    Gto::uint8 mPrevVisibility;
    Gto::uint8 mNextVisibility;
};


#endif
