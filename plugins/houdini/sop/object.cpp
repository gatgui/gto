#include "object.h"
#include "reader.h"
#include <algorithm>
#include <sstream>
#include <Gto/Protocols.h>

Object::Object(const std::string &name, bool asDiff)
  : mName(name)
  , mAsDiff(asDiff)
  , mParentObject(0)
  , mNumParents(0)
  , mParentNameIndices(0)
  , mInstanceNum(-1)
  , mVisibility(1)
  , mPrevVisibility(1)
  , mNextVisibility(1)
{
}

Object::~Object()
{
  cleanup();
}

void Object::cleanup()
{
  if (mParentNameIndices)
  {
    delete[] mParentNameIndices;
    mParentNameIndices = 0;
  }
  mParentNames.clear();
  mNumParents = 0;
}

bool Object::toHoudini(Reader *reader,
                       OP_Context &ctx,
                       GU_Detail *gdp,
                       const ExpandOptions &opts,
                       Object *obj0,
                       Object *obj1,
                       float blend,
                       float t0,
                       float t1)
{
  if (obj0)
  {
    mPrevVisibility = obj0->mVisibility;
    
    if (obj1)
    {
      mNextVisibility = obj1->mVisibility;
    }
    else
    {
      mNextVisibility = mVisibility;
    }
  }
  else
  {
    mPrevVisibility = mVisibility;
    mNextVisibility = mVisibility;
  }
  
  return true;
}

Gto::Reader::Request Object::component(Reader *rdr,
                                       const std::string& name,
                                       const std::string& interp,
                                       const Gto::Reader::ComponentInfo &header)
{
  if (name == GTO_COMPONENT_OBJECT)
  {
    return Gto::Reader::Request(true, (void*) READ_OBJECT);
  }
  else
  {
    return Gto::Reader::Request(false);
  }
}

Gto::Reader::Request Object::property(Reader *rdr,
                                      const std::string& name,
                                      const std::string& interp,
                                      const Gto::Reader::PropertyInfo &header)
{
  if (header.component->componentData == (void*) READ_OBJECT)
  {
    if (name == GTO_PROPERTY_PARENT)
    {
      return Gto::Reader::Request(!isDiff(), (void*) READ_PARENT_NAME);
    }
    else if (name == GTO_PROPERTY_VISIBILITY)
    {
      return Gto::Reader::Request(true, (void*) READ_VISIBILITY);
    }
  }
  
  return Gto::Reader::Request(false);
}

void* Object::data(Reader *rdr, const Gto::Reader::PropertyInfo &header, size_t bytes)
{
  if (header.propertyData == (void*) READ_PARENT_NAME)
  {
    if (header.width == 1)
    {
      mNumParents = header.size;
      mParentNameIndices = new int[mNumParents];
      return mParentNameIndices;
    }
  }
  else if (header.propertyData == (void*) READ_VISIBILITY)
  {
    if (header.width == 1 && header.size == 1 && header.type == Gto::Byte)
    {
      return &mVisibility;
    }
  }
  
  return NULL;
}

void Object::dataRead(Reader *rdr, const Gto::Reader::PropertyInfo &header)
{
  if (header.propertyData == (void*) READ_PARENT_NAME)
  {
    mParentNames.resize(mNumParents);
    for (int i=0; i<mNumParents; ++i)
    {
      mParentNames[i] = rdr->stringFromId(mParentNameIndices[i]);
    }
  }
}

void Object::resolveParent(Reader *reader)
{
  if (mNumParents >= 1)
  {
    if (!mParentObject)
    {
      mParentObject = reader->findObject(mParentNames[0]);
    }
    for (int i=0; i<mParentNames.size(); ++i)
    {
      Object *p = reader->findObject(mParentNames[i]);
      if (p)
      {
        p->addChild(this);
      }
    }
  }
  mInstanceNum = -1;
}

void Object::addChild(Object *o)
{
  if (o && std::find(mChildren.begin(), mChildren.end(), o) == mChildren.end())
  {
    mChildren.push_back(o);
  }
}

Object* Object::getParentObject()
{
  return mParentObject;
}

bool Object::isVisible(int which) const
{
  if (!mParentObject || mParentObject->isVisible(which))
  {
    return (which < 0 ? (mPrevVisibility != 0) : (which > 0 ? (mNextVisibility != 0) : (mVisibility != 0)));
  }
  else
  {
    return false;
  }
}

size_t Object::numParents() const
{
  return mParentNames.size();
}

int Object::getParentIndex(const std::string &name) const
{
  std::vector<std::string>::const_iterator it = std::find(mParentNames.begin(), mParentNames.end(), name);
  if (it == mParentNames.end())
  {
    return -1;
  }
  else
  {
    return int(it - mParentNames.begin());
  }
}

bool Object::switchParent(Reader *reader, size_t idx)
{
  if (idx >= mParentNames.size())
  {
    return false;
  }
  Object *p = reader->findObject(mParentNames[idx]);
  if (!p)
  {
    return false;
  }
  mParentObject = p;
  return true;
}

const std::string& Object::getName() const
{
  return mName;
}

size_t Object::numChildren() const
{
  return mChildren.size();
}

Object* Object::getChild(size_t idx) const
{
  return (idx < mChildren.size() ? mChildren[idx] : 0);
}

bool Object::isDiff() const
{
  return mAsDiff;
}

void Object::setParentObject(Object *o)
{
  mParentObject = o;
}

