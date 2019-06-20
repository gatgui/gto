#include "transform.h"
#include "writer.h"
#include <UT/UT_BoundingBox.h>
#include <UT/UT_Matrix4.h>
#include <UT/UT_DMatrix4.h>
#include <Gto/Protocols.h>
#include <algorithm>

#define GTO_TRANSFORM_VERSION 2

Transform::Transform(OBJ_Node *node, Transform *p)
  : Object(), mParent(p), mNode(node), mDirty(true)
{
  mLocal.identity();
  mWorld.identity();
  if (mNode)
  {
    mName = mNode->getName().nonNullBuffer();
  }
  if (mParent)
  {
    mParent->addChild(this);
  }
}

Transform::~Transform()
{
}

bool Transform::isValid()
{
  return !mDirty;
}

void Transform::invalidate()
{
  mDirty = true;
  
  for (size_t i=0; i<mChildren.size(); ++i)
  {
    mChildren[i]->invalidate();
  }
}

void Transform::update()
{
  if (!mDirty)
  {
    return;
  }
  
  updateWorld();
  
  UT_BoundingBox box;
    
  box.makeInvalid();
  
  for (size_t i=0; i<mChildren.size(); ++i)
  {
    mChildren[i]->update();
    mChildren[i]->mergeBounds(mLocal, box);
  }
  
  setBounds(box);
  
  mDirty = false;
}

void Transform::mergeBounds(const UT_Matrix4 &space, UT_BoundingBox &box)
{
  UT_BoundingBox thisBox(mBBox[0], mBBox[1], mBBox[2], mBBox[3], mBBox[4], mBBox[4]);
  
  thisBox.transform(space);
  
  box.enlargeBounds(thisBox);
}

void Transform::setName(const std::string &name)
{
  mName = name;
}

const std::string& Transform::getName() const
{
  return mName;
}

void Transform::setParent(Transform *t)
{
  if (mParent)
  {
    mParent->remChild(this);
  }
  mParent = t;
  if (mParent)
  {
    mParent->addChild(this);
  }
}

Transform* Transform::getParent() const
{
  return mParent;
}

void Transform::addChild(Object *o)
{
  if (!o)
  {
    return;
  }
  if (std::find(mChildren.begin(), mChildren.end(), o) == mChildren.end())
  {
    mChildren.push_back(o);
    // dirty this node and the new child sub-tree
    mDirty = true;
    o->invalidate();
  }
}

void Transform::remChild(Object *o)
{
  if (!o)
  {
    return;
  }
  std::vector<Object*>::iterator it = std::find(mChildren.begin(), mChildren.end(), o);
  if (it != mChildren.end())
  {
    mChildren.erase(it);
    // dirty this node and the old child sub-tree
    mDirty = true;
    o->invalidate();
  }
}

void Transform::setBounds(const UT_BoundingBox &bbox)
{
  mBBox[0] = bbox.xmin();
  mBBox[1] = bbox.ymin();
  mBBox[2] = bbox.zmin();
  mBBox[3] = bbox.xmax();
  mBBox[4] = bbox.ymax();
  mBBox[5] = bbox.zmax();
}

void Transform::setLocalFromNode(HouGtoWriter *writer)
{
  if (mNode)
  {
    UT_DMatrix4 dmat;
    
    mNode->getWorldTransform(dmat, writer->getContext());

    float scl = writer->getGlobalScale();
    if (scl != 1.0f)
    {
      UT_Vector3 T;
      dmat.getTranslates(T);
      T.x() *= scl;
      T.y() *= scl;
      T.z() *= scl;
      dmat.setTranslates(T);
    }

    mLocal = dmat;
  }
}

void Transform::setBoundsFromNode(HouGtoWriter *writer)
{
  if (mNode)
  {
    UT_BoundingBox box;
    
    mNode->getBoundingBox(box, writer->getContext());

    float scl = writer->getGlobalScale();
    if (scl != 1.0f)
    {
      UT_Vector3 S(scl, scl, scl);
      UT_Vector3 T(0.0f, 0.0f, 0.0f);
      box.scaleOffset(S, T);
    }

    setBounds(box);
  }
}

#if UT_MAJOR_VERSION_INT >= 12
bool Transform::declare(HouGtoWriter *writer, GU_Detail *gdp, const std::vector<GA_Offset> &prims)
#else
bool Transform::declare(HouGtoWriter *writer, GU_Detail *gdp, const std::vector<unsigned int> &prims)
#endif
{
  writer->intern(mName.c_str());
  
  writer->beginObject(mName.c_str(), GTO_PROTOCOL_TRANSFORM, GTO_TRANSFORM_VERSION);
    writer->beginComponent(GTO_COMPONENT_OBJECT);
      writer->property(GTO_PROPERTY_LOCAL_MATRIX, Gto::Float, 1, 16);
      writer->property(GTO_PROPERTY_BOUNDINGBOX, Gto::Float, 1, 6);
      if (writer->isDiff())
      {
        writer->intern(GTO_PROTOCOL_DIFFERENCE);
        writer->property(GTO_PROPERTY_PROTOCOL, Gto::String, 1, 1);
      }
      else
      {
        // What about instances
        if (mParent != 0)
        {
          writer->property(GTO_PROPERTY_PARENT, Gto::String, 1, 1);
        }
      }
    writer->endComponent();
  writer->endObject();
  
  return true;
}

void Transform::write(HouGtoWriter *writer)
{
  UT_Matrix4 mat;
  
  if (mNode)
  {
    UT_BoundingBox bbox;
    UT_DMatrix4 dmat;

    // Get object world matrix
    mNode->getWorldTransform(dmat, writer->getContext());
    mat = dmat;
    
    // Get object local bounding box
    mNode->getBoundingBox(bbox, writer->getContext());

    // Apply global scale
    float scl = writer->getGlobalScale();
    if (scl != 1.0f)
    {
      UT_Vector3 T;
      UT_Vector3 S(scl, scl, scl);

      mat.getTranslates(T);
      T.x() *= scl;
      T.y() *= scl;
      T.z() *= scl;
      mat.setTranslates(T);

      T = UT_Vector3(0.0f, 0.0f, 0.0f);
      bbox.scaleOffset(S, T);
    }

    // Put it in world space
    bbox.transform(mat);
    setBounds(bbox);
    
    // Now apply gto root offset if any
    mat = mLocal * mat;
  }
  else
  {
    mat = mLocal;
  }
  
  mat.transpose();
  
  writer->propertyData(mat.data());
  writer->propertyData(mBBox);
  
  if (writer->isDiff())
  {
    int protocol = writer->lookup(GTO_PROTOCOL_DIFFERENCE);
    writer->propertyData(&protocol);
  }
  else
  {
    if (mParent)
    {
      int parent = writer->lookup(mParent->getName());
      writer->propertyData(&parent);
    }
  }
}

void Transform::setLocal(const UT_Matrix4 &local)
{
  mLocal = local;
  invalidate();
}

void Transform::updateWorld()
{
  // this supposes parent is already up to date
  
  if (mParent)
  {
    mWorld = mLocal * mParent->getWorld();
  }
  else
  {
    mWorld = mLocal;
  }
}

const UT_Matrix4& Transform::getLocal() const
{
  return mLocal;
}

const UT_Matrix4& Transform::getWorld() const
{
  return mWorld;
}

