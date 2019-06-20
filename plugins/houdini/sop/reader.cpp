#include "reader.h"
#include "transform.h"
#include "mesh.h"
#include "curve.h"
#include "particles.h"
#include <Gto/Protocols.h>
#include <set>

Reader::Reader(const std::string &shapeName, bool boundsOnly, bool asDiff, unsigned int mode)
  : Gto::Reader(mode)
  , mAsDiff(asDiff)
  , mBoundsOnly(boundsOnly)
  , mShapeName(shapeName)
  , mFrame(0.0f)
  , mIsFrame(false)
{
}

Reader::~Reader()
{
  Object::Set::iterator it = mObjects.begin();
  while (it != mObjects.end())
  {
    delete it->second;
    ++it;
  }
}

Gto::Reader::Request Reader::object(const std::string& name,
                                    const std::string& protocol,
                                    unsigned int protocolVersion,
                                    const Gto::Reader::ObjectInfo &header)
{
  if (mShapeName != "*" && name != mShapeName)
  {
    return Gto::Reader::Request(false);
  }
  
  if (protocol == GTO_PROTOCOL_TRANSFORM)
  {
    Object *o = findObject(name);
    if (!o)
    {
      o = new Transform(name, mAsDiff);
      mObjects[name] = o;
    }
    return Gto::Reader::Request(true, (void*)o);
  }
  else if (protocol == GTO_PROTOCOL_POLYGON ||
           protocol == GTO_PROTOCOL_CATMULL_CLARK)
  {
    Object *o = findObject(name);
    if (!o)
    {
      o = new Mesh(name, mBoundsOnly, mAsDiff);
      mObjects[name] = o;
    }
    return Gto::Reader::Request(true, (void*)o);
  }
  else if (protocol == GTO_PROTOCOL_NURBS_CURVE)
  {
    Object *o = findObject(name);
    if (!o)
    {
      o = new Curve(name, mBoundsOnly, mAsDiff);
      mObjects[name] = o;
    }
    return Gto::Reader::Request(true, (void*)o);
  }
  else if (protocol == GTO_PROTOCOL_PARTICLE)
  {
    Object *o = findObject(name);
    if (!o)
    {
      o = new Particles(name, mBoundsOnly, mAsDiff);
      mObjects[name] = o;
    }
    return Gto::Reader::Request(true, (void*)o);
  }
  else
  {
    return Gto::Reader::Request(false);
  }
}

Gto::Reader::Request Reader::component(const std::string& name,
                                       const std::string& interp,
                                       const Gto::Reader::ComponentInfo &header)
{
  Object *o = (Object*) header.object->objectData;
  if (o)
  {
    return o->component(this, name, interp, header);
  }
  else
  {
    return Gto::Reader::Request(false);
  }
}

Gto::Reader::Request Reader::property(const std::string& name,
                                      const std::string& interp,
                                      const Gto::Reader::PropertyInfo &header)
{
  Object *o = (Object*) header.component->object->objectData;
  if (o)
  {
    return o->property(this, name, interp, header);
  }
  else
  {
    return Gto::Reader::Request(false);
  }
}

void* Reader::data(const Gto::Reader::PropertyInfo &header, size_t bytes)
{
  Object *o = (Object*) header.component->object->objectData;
  if (o)
  {
    return o->data(this, header, bytes);
  }
  else
  {
    return NULL;
  }
}

void Reader::dataRead(const Gto::Reader::PropertyInfo &header)
{
  Object *o = (Object*) header.component->object->objectData;
  if (o)
  {
    return o->dataRead(this, header);
  }
}

Object* Reader::findObject(const std::string &name)
{
  // how to deal with instances in find... -> need to pass a sort of <path>
  // being only name based will suck
  Object::Set::iterator it = mObjects.find(name);
  if (it != mObjects.end())
  {
    return it->second;
  }
  else
  {
    return 0;
  }
}

bool Reader::toHoudini(OP_Context &ctx, GU_Detail *gdp, const ExpandOptions &opts, Object *obj, Reader *frm0, Reader *frm1, float blend, float t0, float t1)
{
  Object *obj0 = 0, *obj1 = 0;
        
  if (frm0)
  {
    obj0 = frm0->findObject(obj->getName());
  }
  
  if (frm1)
  {
    obj1 = frm1->findObject(obj->getName());
  }
  
  if (!obj->toHoudini(this, ctx, gdp, opts, obj0, obj1, blend, t0, t1))
  {
    return false;
  }
  
  return true;
}

bool Reader::toHoudini(OP_Context &ctx, GU_Detail *gdp, UT_Interrupt *progress, const ExpandOptions &opts, std::set<Object*> &processed, Object *p, Reader *frm0, Reader *frm1, float blend, float t0, float t1)
{
  if (progress->opInterrupt())
  {
    return false;
  }
  
  if (processed.find(p) != processed.end())
  {
    return true;
  }
  
  Object *pp = p->getParentObject();
  
  // make sure parent node is processed first to ensure valid transform
  if (pp && !toHoudini(ctx, gdp, progress, opts, processed, pp, frm0, frm1, blend, t0, t1))
  {
    return false;
  }
  
  if (!toHoudini(ctx, gdp, opts, p, frm0, frm1, blend, t0, t1))
  {
    return false;
  }
  
  processed.insert(p);
  
  return true;
}

bool Reader::toHoudini(OP_Context &ctx, GU_Detail *gdp, UT_Interrupt *progress, const ExpandOptions &opts, Reader *frm0, Reader *frm1, float blend, float t0, float t1)
{
  Object::Set::iterator it;
  
  bool singleMode = (opts.shapeName != "*");
  
  // resolve hierarchy
  
  it = mObjects.begin();
  while (it != mObjects.end())
  {
    if (progress->opInterrupt())
    {
      return false;
    }
    it->second->resolveParent(this);
    ++it;
  }
  
  // declare nodes
  
  std::set<Object*> processed;
  
  if (singleMode)
  {
    it = mObjects.find(opts.shapeName);
    if (it != mObjects.end())
    {
      if (!toHoudini(ctx, gdp, progress, opts, processed, it->second, frm0, frm1, blend, t0, t1))
      {
        return false;
      }
    }
  }
  else
  {
    it = mObjects.begin();

    while (it != mObjects.end())
    {
      if (it->second->getParentObject() == 0)
      {
        if (!toHoudini(ctx, gdp, progress, opts, processed, it->second, frm0, frm1, blend, t0, t1))
        {
          return false;
        }
      }
      ++it;
    }
  }
  
  // set details attributes
  
#if UT_MAJOR_VERSION_INT >= 12
  GA_Attribute *attr;
#else
  GB_Attribute *attr;
#endif
  
  attr = GetOrCreateStringAttrib(gdp, GetParentsInfoKey());
  if (attr)
  {
    SetParentsInfo(attr, mPInfo);
  }
  
  std::map<float, TransformsInfo>::iterator tit = mTInfo.begin();
  while (tit != mTInfo.end())
  {
    attr = GetOrCreateStringAttrib(gdp, GetTransformsInfoKey(tit->first));
    if (attr)
    {
      SetTransformsInfo(attr, tit->second);
    }
    ++tit;
  }
  
  return true;
}

void Reader::setTransform(float frame, const std::string &name, const UT_Matrix4 &mtx)
{
  mTInfo[frame][name] = mtx;
}

void Reader::setParent(const std::string &name, const std::string &pname)
{
  mPInfo[name].insert(pname);
}
