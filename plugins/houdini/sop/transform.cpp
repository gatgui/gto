#include "transform.h"
#include "mesh.h"
#include "particles.h"
#include "reader.h"
#include <Gto/Protocols.h>

Transform::Transform(const std::string &name, bool asDiff)
  : Object(name, asDiff), mParent(0)
{
}

Transform::~Transform()
{
}

Gto::Reader::Request Transform::component(Reader *rdr,
                                          const std::string& name,
                                          const std::string& interp,
                                          const Gto::Reader::ComponentInfo &header)
{
  if (name == GTO_COMPONENT_OBJECT)
  {
    return Gto::Reader::Request(true, (void*) READ_OBJECT);
  }

  return Object::component(rdr, name, interp, header);
}

Gto::Reader::Request Transform::property(Reader *rdr,
                                         const std::string& name,
                                         const std::string& interp,
                                         const Gto::Reader::PropertyInfo &header)
{
  if (header.component->componentData == (void*) READ_OBJECT && name == GTO_PROPERTY_LOCAL_MATRIX)
  {
    return Gto::Reader::Request(true, (void*) READ_LOCAL_MATRIX);
  }

  return Object::property(rdr, name, interp, header);
}

void* Transform::data(Reader *rdr, const Gto::Reader::PropertyInfo &header, size_t bytes)
{
  if (header.propertyData == (void*) READ_LOCAL_MATRIX)
  {
    return ((void*) mLocal.data());
  }

  return Object::data(rdr, header, bytes);
}

void Transform::dataRead(Reader *rdr, const Gto::Reader::PropertyInfo &header)
{
  if (header.propertyData == (void*) READ_LOCAL_MATRIX)
  {
    mLocal.transpose();
  }
  
  Object::dataRead(rdr, header);
}

bool Transform::toHoudini(Reader *reader,
                          OP_Context &ctx,
                          GU_Detail *gdp,
                          const ExpandOptions &opts,
                          Object *obj0,
                          Object *obj1,
                          float blend,
                          float t0,
                          float t1)
{
  Object::toHoudini(reader, ctx, gdp, opts, obj0, obj1, blend, t0, t1);
  
  ++mInstanceNum;

  if (obj0)
  {
    mPrevLocal = ((Transform*) obj0)->mLocal;

    if (obj1)
    {
      mNextLocal = ((Transform*) obj1)->mLocal;
    }
    else
    {
      mNextLocal = mLocal;
    }
  }
  else
  {
    // no frames no transform animation
    mPrevLocal = mLocal;
    mNextLocal = mLocal;
  }
  
  UT_Matrix4 local = mLocal;
  applySceneScale(local, opts.globalScale);
  
  reader->setTransform(ctx.getFloatFrame(), getName(), local);
  if (getParent())
  {
    reader->setParent(getName(), getParent()->getName());
  }

  if (opts.shapeName != "*")
  {
    // Noop
  }
  else
  {
    for (size_t i=0; i<mChildren.size(); ++i)
    {
      Object *child = mChildren[i];

      if (!child)
      {
        continue;
      }

      int pi = child->getParentIndex(getName());

      if (pi != -1 && (opts.computeInstance || pi == 0))
      {
        if (!child->switchParent(reader, pi))
        {
          continue;
        }

        Object *childObj0 = (opts.frame0 ? opts.frame0->findObject(child->getName()) : 0);
        Object *childObj1 = (opts.frame1 ? opts.frame1->findObject(child->getName()) : 0);

        child->toHoudini(reader, ctx, gdp, opts, childObj0, childObj1, blend, t0, t1);
      }
    }
  }
  
  return true;
}

void Transform::resolveParent(Reader *reader)
{
  if (mNumParents >= 1)
  {
    Object *o = reader->findObject(mParentNames[0]);
    if (o != 0)
    {
      setParent(dynamic_cast<Transform*>(o));
    }
  }

  Object::resolveParent(reader);
}

Transform* Transform::getParent() const
{
  return mParent;
}

void Transform::setParent(Transform *t)
{
  mParent = t;
  setParentObject(t);
}

bool Transform::switchParent(Reader *reader, size_t idx)
{
  if (Object::switchParent(reader, idx))
  {
    mParent = dynamic_cast<Transform*>(mParentObject);
    return true;
  }
  return false;
}

const UT_Matrix4& Transform::getWorldMatrix() const
{
  return mWorld;
}

void Transform::applySceneScale(UT_Matrix4 &m, float scl) const
{
  if (scl != 1.0f)
  {
    UT_Vector3 T;

    m.getTranslates(T);
    
    T.x() *= scl;
    T.y() *= scl;
    T.z() *= scl;
    
    m.setTranslates(T);
  }
}

const UT_Matrix4& Transform::updateWorldMatrix(float sceneScale, int which)
{
  UT_Matrix4 local;
  
  if (which == 0)
  {
    local = mLocal;
  }
  else if (which < 0)
  {
    local = mPrevLocal;
  }
  else
  {
    local = mNextLocal;
  }
  
  applySceneScale(local, sceneScale);
  
  if (mParent)
  {
    mParent->updateWorldMatrix(sceneScale, which);
    mWorld = local * mParent->getWorldMatrix();
  }
  else
  {
    mWorld = local;
  }
  
  return mWorld;
}
