#ifndef __ROP_GtoExport_transform_h__
#define __ROP_GtoExport_transform_h__

#include <OBJ/OBJ_Node.h>
#include <UT/UT_BoundingBox.h>
#include <UT/UT_Matrix4.h>
#include "object.h"

class Transform : public Object
{
  public:
    
    Transform(OBJ_Node *node=0, Transform *p=0);
    virtual ~Transform();
    
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
    
    void setName(const std::string &name);
    const std::string& getName() const;
    
    void setParent(Transform *t);
    Transform* getParent() const;
    
    void setBounds(const UT_BoundingBox &bbox);
    void setBoundsFromNode(HouGtoWriter *writer);
    
    void setLocal(const UT_Matrix4 &local);
    void setLocalFromNode(HouGtoWriter *writer);
    const UT_Matrix4& getLocal() const;
    
    const UT_Matrix4& getWorld() const;
    void updateWorld();
    
    void addChild(Object *o);
    void remChild(Object *o);

    
  protected:
    
    Transform *mParent;
    std::vector<Object*> mChildren;
    OBJ_Node *mNode;
    std::string mName;
    float mBBox[6]; // local space
    UT_Matrix4 mLocal;
    UT_Matrix4 mWorld;
    bool mDirty;
};

#endif

