#include "particles.h"
#include "writer.h"
#include "transform.h"
#include <Gto/Protocols.h>
#include <GEO/GEO_Primitive.h>
#include <GU/GU_PrimPart.h>
#include <HOM/HOM_Module.h>
#if UT_MAJOR_VERSION_INT < 12
#  include <GEO/GEO_PointList.h>
#  include <GEO/GEO_PrimList.h>
#endif

#ifdef _WIN32
#  undef max
#  undef min
#endif
#include <limits>

Particles::Particles(Transform *p)
  : Object()
  , mParent(p)
  , mName("")
  , mNumPoints(0)
  , mMaxVelocityMag(0.0f)
  , mMaxSize(0.0f)
  , mMaxMultiRadius(0.0f)
  , mSizePerParticle(false)
  , mMultiCountPerParticle(false)
  , mMultiRadiusPerParticle(false)
  , mRenderType("")
  , mDirty(true)
{
  mMinVelocity[0] = std::numeric_limits<float>::max();
  mMinVelocity[1] = std::numeric_limits<float>::max();
  mMinVelocity[2] = std::numeric_limits<float>::max();
  mMaxVelocity[0] = -std::numeric_limits<float>::max();
  mMaxVelocity[1] = -std::numeric_limits<float>::max();
  mMaxVelocity[2] = -std::numeric_limits<float>::max();
  if (mParent)
  {
    mParent->addChild(this);
  }
}

Particles::~Particles()
{
  cleanup();
}

#if UT_MAJOR_VERSION_INT >= 12
bool _GetAttribute(GU_Detail *gdp, const GEO_Primitive *prim, const char *name, GA_StorageClass type, int dim, bool pointsOnly, GA_RWAttributeRef &hdl, bool &perParticle)
{  
  GA_RWAttributeRef ref = gdp->findPointAttribute(GA_SCOPE_PUBLIC, name);
  if (ref.isValid() && ref.getStorageClass() == type && ref.getTupleSize() == dim)
  {
    hdl = ref;
    perParticle = true;
    return true;
  }
  else if (!pointsOnly)
  {
    ref = gdp->findPrimitiveAttribute(GA_SCOPE_PUBLIC, name);
    if (ref.isValid() && ref.getStorageClass() == type && ref.getTupleSize() == dim)
    {
      hdl = ref;
      perParticle = false;
      return true;
    }
  }
  hdl.clear();
  return false;
}
#else
bool _GetAttribute(GU_Detail *gdp, const GEO_Primitive *prim, const char *name, GB_AttribType type, int dim, bool pointsOnly, GEO_AttributeHandle &hdl, bool &perParticle)
{
  int bs = 0;
  switch (type)
  {
  case GB_ATTRIB_FLOAT:
    bs = sizeof(float);
    break;
  case GB_ATTRIB_INT:
  case GB_ATTRIB_INDEX:
    bs = sizeof(int);
    break;
  case GB_ATTRIB_VECTOR:
    bs = 3*sizeof(float);
    break;
  default:
    hdl.invalidate();
    return false;
  }
  
  GB_AttributeRef ref = gdp->findPointAttrib(name, dim*bs, type);
  if (ref.isValid())
  {
    hdl = gdp->getPointAttribute(name);
    perParticle = true;
    return true;
  }
  else if (!pointsOnly)
  {
    ref = gdp->findPrimAttrib(name, dim*bs, type);
    if (ref.isValid())
    {
      hdl = gdp->getPrimAttribute(name);
      hdl.setElement(prim);
      perParticle = false;
      return true;
    }
  }
  hdl.invalidate();
  return false;
}
#endif

#if UT_MAJOR_VERSION_INT >= 12
bool Particles::declare(HouGtoWriter *writer, GU_Detail *gdp, const std::vector<GA_Offset> &prims)
#else
bool Particles::declare(HouGtoWriter *writer, GU_Detail *gdp, const std::vector<unsigned int> &prims)
#endif
{
  if (!gdp)
  {
    return false;
  }
  
  cleanup();

  float globalScale = writer->getGlobalScale();
  
#if UT_MAJOR_VERSION_INT >= 12
  GA_Offset partPrim = GA_INVALID_OFFSET;
#else
  const GEO_PointList &ptl = gdp->points();
  const GEO_PrimList &prl = gdp->primitives();
  unsigned int partPrim = 0;
#endif
  
  bool foundParticles = false;
  
  if (prims.size() > 1)
  {
    std::cout << "*** More than one Particle primitive in shape" << std::endl;
    return false;
  }
  else if (prims.size() == 1)
  {
#if UT_MAJOR_VERSION_INT >= 12
    const GU_PrimParticle *parts = (const GU_PrimParticle*) gdp->getGEOPrimitive(prims[0]);
#else
    const GU_PrimParticle *parts = (const GU_PrimParticle*) prl.entry(prims[0]);
#endif
    
    if (parts->getVertexCount() > 0)
    {
      partPrim = prims[0];
      foundParticles = true;
    }
  }
  
  if (foundParticles)
  {
    // Do we handle primitiveGroups as different buckets?
    
    // collect particles data
#if UT_MAJOR_VERSION_INT >= 12
    const GU_PrimParticle *parts = (const GU_PrimParticle*) gdp->getGEOPrimitive(partPrim);
#else
    const GU_PrimParticle *parts = (const GU_PrimParticle*) prl.entry(partPrim);
#endif
    
    UT_Vector4 pos;
    
    // No sprite in houdini?
    // TODO: Allow usage of a string primitive attribute "renderType" for that
    switch (parts->getRenderAttribs().getType())
    {
    case GEO_PARTICLE_SPHERE:
      mRenderType = "sphere";
      break;
    case GEO_PARTICLE_CIRCLE:
      mRenderType = "point";
      break;
    case GEO_PARTICLE_LINE:
    case GEO_PARTICLE_TUBE:
    case GEO_PARTICLE_TUBEC:
    case GEO_PARTICLE_TUBES:
      mRenderType = "line";
      break;
    default:
      mRenderType = "point";
      break;
    }
    
    writer->intern(mRenderType.c_str());
    
    mNumPoints = parts->getVertexCount();
    mPositions.resize(mNumPoints * 3);
    mVelocities.resize(mNumPoints * 3);
    mIDs.resize(mNumPoints);
    
    bool hasId=false, hasSize=false, hasVelocity=false, hasMultiRad=false, hasMultiCnt=false, multiPoint=false, dummy;
    
#if UT_MAJOR_VERSION_INT >= 12
    GA_RWAttributeRef idAttr;
    GA_RWAttributeRef szAttr;
    GA_RWAttributeRef velAttr;
    GA_RWAttributeRef mrAttr;
    GA_RWAttributeRef mcAttr;
    
    hasId = _GetAttribute(gdp, parts, "id", GA_STORECLASS_INT, 1, true, idAttr, dummy);
    hasVelocity = _GetAttribute(gdp, parts, "v", GA_STORECLASS_FLOAT, 3, true, velAttr, dummy);
    hasSize = _GetAttribute(gdp, parts, "size", GA_STORECLASS_FLOAT, 1, false, szAttr, mSizePerParticle);
    hasMultiRad = _GetAttribute(gdp, parts, "multiRadius", GA_STORECLASS_FLOAT, 1, false, mrAttr, mMultiRadiusPerParticle);
    if (hasMultiRad)
    {
      hasMultiCnt = _GetAttribute(gdp, parts, "multiCount", GA_STORECLASS_INT, 1, false, mcAttr, mMultiCountPerParticle);
      if (hasMultiCnt)
      {
        multiPoint = true;
      }
    }
#else
    GEO_AttributeHandle idAttr;
    GEO_AttributeHandle szAttr;
    GEO_AttributeHandle velAttr;
    GEO_AttributeHandle mrAttr;
    GEO_AttributeHandle mcAttr;
    
    hasId = _GetAttribute(gdp, parts, "id", GB_ATTRIB_INT, 1, true, idAttr, dummy);
    hasVelocity = _GetAttribute(gdp, parts, "v", GB_ATTRIB_VECTOR, 1, true, velAttr, dummy);
    if (!hasVelocity)
    {
      hasVelocity = _GetAttribute(gdp, parts, "v", GB_ATTRIB_FLOAT, 3, true, velAttr, dummy);
    }
    hasSize = _GetAttribute(gdp, parts, "size", GB_ATTRIB_FLOAT, 1, false, szAttr, mSizePerParticle);
    hasMultiRad = _GetAttribute(gdp, parts, "multiRadius", GB_ATTRIB_FLOAT, 1, false, mrAttr, mMultiRadiusPerParticle);
    if (hasMultiRad)
    {
      hasMultiCnt = _GetAttribute(gdp, parts, "multiCount", GB_ATTRIB_INT, 1, false, mcAttr, mMultiCountPerParticle);
      if (hasMultiCnt)
      {
        multiPoint = true;
      }
    }
#endif
    
    if (!hasVelocity)
    {
      mMinVelocity[0] = 0.0f;
      mMinVelocity[1] = 0.0f;
      mMinVelocity[2] = 0.0f;
      mMaxVelocity[0] = 0.0f;
      mMaxVelocity[1] = 0.0f;
      mMaxVelocity[2] = 0.0f;
      mMaxVelocityMag = 0.0f;
    }
    
    if (!hasSize)
    {
      mSizes.resize(1);
      mSizes[0] = parts->getRenderAttribs().getSize() * globalScale;
      mMaxSize = mSizes[0];
    }
    else if (!mSizePerParticle)
    {
      mSizes.resize(1);
#if UT_MAJOR_VERSION_INT >= 12
      float sz = 0.0f;
      szAttr.getAIFTuple()->get(szAttr.getAttribute(), partPrim, sz, 0);
      mSizes[0] = sz * globalScale;
#else
      mSizes[0] = szAttr.getF(0) * globalScale;
#endif
      mMaxSize = mSizes[0];
    }
    else
    {
      mSizes.resize(mNumPoints);
    }
    
    if (multiPoint)
    {
      if (mMultiRadiusPerParticle)
      {
        mMultiRadius.resize(mNumPoints);
      }
      else
      {
        mMultiRadius.resize(1);
#if UT_MAJOR_VERSION_INT >= 12
        float rad = 0.0f;
        mrAttr.getAIFTuple()->get(mrAttr.getAttribute(), partPrim, rad, 0);
        mMultiRadius[0] = rad * globalScale;
#else
        mMultiRadius[0] = mrAttr.getF(0) * globalScale;
#endif
        
        mMaxMultiRadius = mMultiRadius[0];
      }
      if (mMultiCountPerParticle)
      {
        mMultiCount.resize(mNumPoints);
      }
      else
      {
        mMultiCount.resize(1);
#if UT_MAJOR_VERSION_INT >= 12
        mcAttr.getAIFTuple()->get(mcAttr.getAttribute(), partPrim, mMultiCount[0], 0);
#else
        mMultiCount[0] = mcAttr.getI(0);
#endif
      }
    }
    
    mPointAttribs.addIgnore("P");
    mPointAttribs.addIgnore("v");
    mPointAttribs.addIgnore("id");
    mPointAttribs.addIgnore("size");
    mPointAttribs.addIgnore("multiRadius");
    mPointAttribs.addIgnore("multiCount");
    mPointAttribs.collectPoints(gdp, prims);
    if (globalScale != 1.0f)
    {
      mPointAttribs.scaleBy(globalScale);
    }
    
    // ignore all the same attributes as mPointAttribs + the ones collected
    mObjectAttribs.addIgnores(mPointAttribs);
    mObjectAttribs.collectPrimitives(gdp, prims);
    if (globalScale != 1.0f)
    {
      mObjectAttribs.scaleBy(globalScale);
    }
    
    unsigned int i=0, j=0;   
#if UT_MAJOR_VERSION_INT >= 12
#if UT_MAJOR_VERSION_INT >= 16
    for (GA_Size vi=0; vi<parts->getVertexCount(); ++vi)
    {
      GA_Offset pt = parts->getPointOffset(vi);
#else
    GA_Primitive::const_iterator vit;
    parts->beginVertex(vit);
    while (!vit.atEnd())
    {
      GA_Offset pt = vit.getPointOffset();
      vit.advance();
#endif
      UT_Vector4 pos = gdp->getPos4(pt);
      
      mPositions[j] = pos.x() * globalScale;
      mPositions[j+1] = pos.y() * globalScale;
      mPositions[j+2] = pos.z() * globalScale;
      
      if (hasId)
      {
        idAttr.getAIFTuple()->get(idAttr.getAttribute(), pt, mIDs[i], 0);
      }
      else
      {
        mIDs[i] = (int)i;
      }
      
      if (hasVelocity)
      {
        float vx = 0.0f, vy = 0.0f, vz = 0.0f;
        velAttr.getAIFTuple()->get(velAttr.getAttribute(), pt, vx, 0);
        velAttr.getAIFTuple()->get(velAttr.getAttribute(), pt, vy, 1);
        velAttr.getAIFTuple()->get(velAttr.getAttribute(), pt, vz, 2);
        mVelocities[j] = vx * globalScale;
        mVelocities[j+1] = vy * globalScale;
        mVelocities[j+2] = vz * globalScale;
        if (mVelocities[j]   < mMinVelocity[0]) mMinVelocity[0] = mVelocities[j];
        if (mVelocities[j+1] < mMinVelocity[1]) mMinVelocity[1] = mVelocities[j+1];
        if (mVelocities[j+2] < mMinVelocity[2]) mMinVelocity[2] = mVelocities[j+2];
        if (mVelocities[j]   > mMaxVelocity[0]) mMaxVelocity[0] = mVelocities[j];
        if (mVelocities[j+1] > mMaxVelocity[1]) mMaxVelocity[1] = mVelocities[j+1];
        if (mVelocities[j+2] > mMaxVelocity[2]) mMaxVelocity[2] = mVelocities[j+2];
        float mag = float( sqrt( mVelocities[j]   * mVelocities[j] +
                                 mVelocities[j+1] * mVelocities[j+1] +
                                 mVelocities[j+2] * mVelocities[j+2] ) );
        if (mag > mMaxVelocityMag) mMaxVelocityMag = mag;
      }
      else
      {
        mVelocities[j] = 0.0f;
        mVelocities[j+1] = 0.0f;
        mVelocities[j+2] = 0.0f;
      }
      
      if (mSizePerParticle)
      {
        float sz = 0.0f;
        szAttr.getAIFTuple()->get(szAttr.getAttribute(), pt, sz, 0);
        mSizes[i] = sz * globalScale;
        if (mSizes[i] > mMaxSize) mMaxSize = mSizes[i];
      }
      
      if (mMultiRadiusPerParticle)
      {
        float rad = 0.0f;
        mrAttr.getAIFTuple()->get(mrAttr.getAttribute(), pt, rad, 0);
        mMultiRadius[i] = rad * globalScale;
        if (mMultiRadius[i] > mMaxMultiRadius) mMaxMultiRadius = mMultiRadius[i];
      }
      
      if (mMultiCountPerParticle)
      {
        mcAttr.getAIFTuple()->get(mcAttr.getAttribute(), pt, mMultiCount[i], 0);
      }
      
      ++i;
      j += 3;
    }
#else
    GEO_ParticleVertex *vtx = parts->iterateInit();
    while (vtx)
    {
      const GEO_Point *pt = vtx->getPt();
      vtx = parts->iterateNext(vtx);
      UT_Vector4 pos = pt->getPos();
      
      mPositions[j] = pos.x() * globalScale;
      mPositions[j+1] = pos.y() * globalScale;
      mPositions[j+2] = pos.z() * globalScale;
      
      if (hasId)
      {
        idAttr.setElement(pt);
        mIDs[i] = idAttr.getI(0);
      }
      else
      {
        mIDs[i] = (int)i;
      }
      
      if (hasVelocity)
      {
        velAttr.setElement(pt);
        mVelocities[j] = velAttr.getF(0) * globalScale;
        mVelocities[j+1] = velAttr.getF(1) * globalScale;
        mVelocities[j+2] = velAttr.getF(2) * globalScale;
        if (mVelocities[j]   < mMinVelocity[0]) mMinVelocity[0] = mVelocities[j];
        if (mVelocities[j+1] < mMinVelocity[1]) mMinVelocity[1] = mVelocities[j+1];
        if (mVelocities[j+2] < mMinVelocity[2]) mMinVelocity[2] = mVelocities[j+2];
        if (mVelocities[j]   > mMaxVelocity[0]) mMaxVelocity[0] = mVelocities[j];
        if (mVelocities[j+1] > mMaxVelocity[1]) mMaxVelocity[1] = mVelocities[j+1];
        if (mVelocities[j+2] > mMaxVelocity[2]) mMaxVelocity[2] = mVelocities[j+2];
        float mag = float( sqrt( mVelocities[j]   * mVelocities[j] +
                                 mVelocities[j+1] * mVelocities[j+1] +
                                 mVelocities[j+2] * mVelocities[j+2] ) );
        if (mag > mMaxVelocityMag) mMaxVelocityMag = mag;
      }
      else
      {
        mVelocities[j] = 0.0f;
        mVelocities[j+1] = 0.0f;
        mVelocities[j+2] = 0.0f;
      }
      
      if (mSizePerParticle)
      {
        szAttr.setElement(pt);
        mSizes[i] = szAttr.getF(0) * globalScale;
        if (mSizes[i] > mMaxSize) mMaxSize = mSizes[i];
      }
      
      if (mMultiRadiusPerParticle)
      {
        mrAttr.setElement(pt);
        mMultiRadius[i] = mrAttr.getF(0) * globalScale;
        if (mMultiRadius[i] > mMaxMultiRadius) mMaxMultiRadius = mMultiRadius[i];
      }
      
      if (mMultiCountPerParticle)
      {
        mcAttr.setElement(pt);
        mMultiCount[i] = mcAttr.getI(0);
      }
      
      ++i;
      j += 3;
    }
#endif
    
    // declare GTO object
    
    writer->beginObject(mName.c_str(), GTO_PROTOCOL_PARTICLE, 1);

    writer->beginComponent(GTO_COMPONENT_POINTS);
    writer->property(GTO_PROPERTY_POSITION, Gto::Float, mNumPoints, 3);
    writer->property("velocity", Gto::Float, mNumPoints, 3, "vector");
    writer->property("id", Gto::Int, mNumPoints, 1);
    if (mSizePerParticle)
    {
      writer->property(GTO_PROPERTY_SIZE, Gto::Float, mNumPoints, 1);
    }
    if (multiPoint && mMultiCountPerParticle)
    {
      writer->property("multiCount", Gto::Int, mNumPoints, 1);
    }
    if (multiPoint && mMultiRadiusPerParticle)
    {
      writer->property("multiRadius", Gto::Float, mNumPoints, 1);
    }
    mPointAttribs.declareGto(writer);
    writer->endComponent();
    
    writer->beginComponent(GTO_COMPONENT_PARTICLE);
    if (!mSizePerParticle)
    {
      writer->property(GTO_PROPERTY_SIZE, Gto::Float, 1, 1);
    }
    if (multiPoint && !mMultiCountPerParticle)
    {
      writer->property("multiCount", Gto::Int, 1, 1);
    }
    if (multiPoint && !mMultiRadiusPerParticle)
    {
      writer->property("multiRadius", Gto::Float, 1, 1);
    }
    mObjectAttribs.declareGto(writer);
    writer->property( "renderType", Gto::String, 1, 1 ); // point/sphere/line/sprite
    writer->property( "minVelocity", Gto::Float, 1, 3 );
    writer->property( "maxVelocity", Gto::Float, 1, 3 );
    writer->property( "maxVelocityMag", Gto::Float, 1, 1 );
    writer->property( "maxSize", Gto::Float, 1, 1 ); // radius
    if (multiPoint)
    {
      writer->property( "maxMultiRadius", Gto::Float, 1, 1 ); // not dealt with yet
    }
    writer->endComponent();
    
    writer->beginComponent(GTO_COMPONENT_OBJECT);
    writer->property(GTO_PROPERTY_BOUNDINGBOX, Gto::Float, 1, 6);
    if (writer->isDiff())
    {
      writer->intern(GTO_PROTOCOL_DIFFERENCE);
      writer->property(GTO_PROPERTY_PROTOCOL, Gto::String, 1, 1);
    }
    else if (mParent != 0)
    {
      writer->property(GTO_PROPERTY_PARENT, Gto::String, 1, 1);
    }
    writer->endComponent();
    
    writer->endObject();
  }
  
  return true;
}

void Particles::write(HouGtoWriter *writer)
{
  if (mNumPoints > 0)
  {
    writer->propertyData(&mPositions[0]);
    writer->propertyData(&mVelocities[0]);
    writer->propertyData(&mIDs[0]);
    if (mSizes.size() > 0 && mSizePerParticle)
    {
      writer->propertyData(&mSizes[0]);
    }
    if (mMultiCount.size() > 0 && mMultiCountPerParticle)
    {
      writer->propertyData(&mMultiCount[0]);
    }
    if (mMultiRadius.size() > 0 && mMultiRadiusPerParticle)
    {
      writer->propertyData(&mMultiRadius[0]);
    }
    mPointAttribs.outputGto(writer);
    
    if (mSizes.size() > 0 && !mSizePerParticle)
    {
      writer->propertyData(&mSizes[0]);
    }
    if (mMultiCount.size() > 0 && !mMultiCountPerParticle)
    {
      writer->propertyData(&mMultiCount[0]);
    }
    if (mMultiRadius.size() > 0 && !mMultiRadiusPerParticle)
    {
      writer->propertyData(&mMultiRadius[0]);
    }
    mObjectAttribs.outputGto(writer);
    int rt = writer->lookup(mRenderType.c_str());
    writer->propertyData(&rt);
    writer->propertyData(mMinVelocity);
    writer->propertyData(mMaxVelocity);
    writer->propertyData(&mMaxVelocityMag);
    writer->propertyData(&mMaxSize);
    if (mMultiRadius.size() > 0 && mMultiCount.size() > 0)
    {
      writer->propertyData(&mMaxMultiRadius);
    }
    
    writer->propertyData(mBBox);
    if (writer->isDiff())
    {
      int protocol = writer->lookup(GTO_PROTOCOL_DIFFERENCE);
      writer->propertyData(&protocol);
    }
    else if (mParent != 0)
    {
      int name = writer->lookup(mParent->getName());
      writer->propertyData(&name);
    }
  }
}

bool Particles::isValid()
{
  return !mDirty;
}

void Particles::invalidate()
{
  // NOOP ?
}

void Particles::update()
{
  if (!mDirty)
  {
    return;
  }
  
  // beware that houdini object transform shall not be included
  // in the parent world matrix at this time
  // (though it will appear added to the root transform in the gto file)
  
  UT_Matrix4 mat;
  UT_Vector4 v;
  UT_BoundingBox box;

  if (getParent() == 0)
  {
    mat.identity();
  }
  else
  {
    mat = getParent()->getWorld();
    mat.invert();
  }

  // NOTE: changes are irreversible... declare must be call again if there's any
  //       need to call several times update for the same object
  //       this shall not be an issue ATM
  
  // Transform points and velocities and compute bounding box

  box.makeInvalid();
  
  for (unsigned int i=0, j=0; i<mNumPoints; ++i, j+=3)
  {
    v.assign(mPositions[j], mPositions[j+1], mPositions[j+2], 1.0f);
    v = v * mat;
    
    mPositions[j] = v.x();
    mPositions[j+1] = v.y();
    mPositions[j+2] = v.z();
    
    box.enlargeBounds(v);
    
    v.assign(mVelocities[j], mVelocities[j+1], mVelocities[j+2], 0.0f);
    v = v * mat;
    
    mVelocities[j] = v.x();
    mVelocities[j+1] = v.y();
    mVelocities[j+2] = v.z();
  }
  
  // Update bounding box
  // 
  HOM_Module &hom = HOM();
  float vscl = 1.0f;
  if (hom.fps() > 0.0f)
  {
    vscl /= hom.fps();
  }
  
  float e = vscl * mMaxVelocityMag + mMaxSize + mMaxMultiRadius;
  v.assign(e, 0.0f, 0.0f, 0.0f);
  v = v * mat;
  e = v.length();
  
  mBBox[0] = box.xmin() - e;
  mBBox[1] = box.ymin() - e;
  mBBox[2] = box.zmin() - e;
  mBBox[3] = box.xmax() + e;
  mBBox[4] = box.ymax() + e;
  mBBox[5] = box.zmax() + e;
  
  mDirty = false;
}

void Particles::mergeBounds(const UT_Matrix4 &space, UT_BoundingBox &box)
{
  UT_BoundingBox thisBox(mBBox[0], mBBox[1], mBBox[2], mBBox[3], mBBox[4], mBBox[5]);
  
  thisBox.transform(space);
  
  box.enlargeBounds(thisBox);
}

void Particles::setName(const std::string &name)
{
  mName = name;
}

const std::string& Particles::getName() const
{
  return mName;
}

void Particles::setParent(Transform *t)
{
  mParent = t;
}

Transform* Particles::getParent() const
{
  return mParent;
}

void Particles::cleanup()
{
  mPositions.clear();
  mIDs.clear();
  mSizes.clear();
  mVelocities.clear();
  mPointAttribs.clear();
  mObjectAttribs.clear();
  mNumPoints = 0;
  mMinVelocity[0] = std::numeric_limits<float>::max();
  mMinVelocity[1] = std::numeric_limits<float>::max();
  mMinVelocity[2] = std::numeric_limits<float>::max();
  mMaxVelocity[0] = -std::numeric_limits<float>::max();
  mMaxVelocity[1] = -std::numeric_limits<float>::max();
  mMaxVelocity[2] = -std::numeric_limits<float>::max();
  mMaxVelocityMag = 0.0f;
  mMaxSize = 0.0f;
  mMaxMultiRadius = 0.0f;
  mSizePerParticle = false;
  mMultiRadiusPerParticle = false;
  mMultiCountPerParticle = false;
  mRenderType = "";
}

