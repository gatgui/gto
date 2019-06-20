#include "curve.h"
#include "writer.h"
#include "transform.h"
#include <Gto/Protocols.h>
#include <GEO/GEO_Primitive.h>
#include <GU/GU_PrimNURBCurve.h>
//#include <GU/GU_PrimRBezCurve.h>
#if UT_MAJOR_VERSION_INT >= 12
#  include <GA/GA_Basis.h>
#else
#  include <GB/GB_Basis.h>
#  include <GEO/GEO_PointList.h>
#  include <GEO/GEO_PrimList.h>
#endif

#define GTO_NURBS_CURVE_VERSION 2

Curve::Curve(Transform *p)
  : Object()
  , mParent(p)
  , mName("")
  , mNumPoints(0)
  , mDegree(0)
  , mForm(1)
  , mDirty(true)
{
  if (mParent)
  {
    mParent->addChild(this);
  }
}

Curve::~Curve()
{
  cleanup();
}

#if UT_MAJOR_VERSION_INT >= 12
bool Curve::declare(HouGtoWriter *writer, GU_Detail *gdp, const std::vector<GA_Offset> &prims)
#else
bool Curve::declare(HouGtoWriter *writer, GU_Detail *gdp, const std::vector<unsigned int> &prims)
#endif
{
  if (!gdp)
  {
    return false;
  }
  
  cleanup();

  float globalScale = writer->getGlobalScale();
  
#if UT_MAJOR_VERSION_INT >= 12
  GA_Offset curvePrim = GA_INVALID_OFFSET;
#else
  const GEO_PointList &ptl = gdp->points();
  const GEO_PrimList &prl = gdp->primitives();
  unsigned int curvePrim = 0;
#endif
  
  bool foundCurve = false;
  
  if (prims.size() > 1)
  {
    std::cout << "*** More than one NURBS curve primitive in shape" << std::endl;
    return false;
  }
  else if (prims.size() == 1)
  {
#if UT_MAJOR_VERSION_INT >= 12
    const GU_PrimNURBCurve *crv = (const GU_PrimNURBCurve*) gdp->getGEOPrimitive(prims[0]);
#else
    const GU_PrimNURBCurve *crv = (const GU_PrimNURBCurve*) prl.entry(prims[0]);
#endif
    
    if (crv->getVertexCount() > 0)
    {
      curvePrim = prims[0];
      foundCurve = true;
    }
  } 
  
  if (foundCurve)
  {
    // collect curve data
#if UT_MAJOR_VERSION_INT >= 12
    const GU_PrimNURBCurve *crv = (const GU_PrimNURBCurve*) gdp->getGEOPrimitive(curvePrim);
    const GA_Basis *basis = crv->getBasis();
#else
    const GU_PrimNURBCurve *crv = (const GU_PrimNURBCurve*) prl.entry(curvePrim);
    const GB_Basis *basis = crv->getBasis();
#endif
    
    UT_Vector4 pos;
    
    mDegree = crv->getOrder() - 1;
    mNumPoints = crv->getVertexCount();
    mPositions.resize(mNumPoints * 3);
    mWeights.resize(mNumPoints);
    
    for (unsigned int i=0, j=0; i<mNumPoints; ++i, j+=3)
    {
#if UT_MAJOR_VERSION_INT >= 12
      UT_Vector4 pos = gdp->getPos4(crv->getPointOffset(i));
#else
      UT_Vector4 pos = crv->getVertex(i).getPt()->getPos();
#endif
      float w = (crv->isRational() ? crv->getWeight(i) : 1.0f);
      
      mPositions[j] = pos.x() * globalScale;
      mPositions[j+1] = pos.y() * globalScale;
      mPositions[j+2] = pos.z() * globalScale;
      
      mWeights[i] = w;
    }
    
#if UT_MAJOR_VERSION_INT >= 12
    const GA_KnotVector &knots = basis->getVector();
#else
    const UT_FloatArray &knots = basis->getVector();
#endif
    mKnots.resize(knots.entries());
    
    for (unsigned int i=0; i<knots.entries(); ++i)
    {
      mKnots[i] = knots(i);
    }
    
    // what about periodic?
    // -> special case of closed curve
    // -> closed curve means the last cv matches the first one
    // -> periodic curve means the last degree cvs matches the first degree ones
    //    with no knots multiplicity at begin or end of knots vector
    //    as the last order pair of knots interval must match the first order pair of knots interval
    //    in houdini speak this means a periodic curve cannot have the interpolatesEnd() flags set
    // mForm: 1=open, 2=closed, 3=periodic
#if UT_MAJOR_VERSION_INT >= 12
    mForm = (crv->isClosed() ? (crv->getEndInterpolation() ? 2 : 3) : 1);
#else
    mForm = (crv->isClosed() ? (crv->interpolatesEnds() ? 2 : 3) : 1);
#endif
    
    // declare GTO object
    
    writer->beginObject(mName.c_str(), GTO_PROTOCOL_NURBS_CURVE, GTO_NURBS_CURVE_VERSION);

    writer->beginComponent(GTO_COMPONENT_POINTS);
    writer->property(GTO_PROPERTY_POSITION, Gto::Float, mNumPoints, 3);
    writer->property(GTO_PROPERTY_WEIGHT, Gto::Float, mNumPoints, 1);
    writer->endComponent();

    if (!writer->isDiff())
    {
      writer->beginComponent(GTO_COMPONENT_SURFACE);
      writer->property(GTO_PROPERTY_DEGREE, Gto::Int, 1, 1);
      writer->property(GTO_PROPERTY_VKNOTS, Gto::Float, mKnots.size(), 1);
      writer->property(GTO_PROPERTY_VRANGE, Gto::Float, 2, 1);
      writer->property(GTO_PROPERTY_VFORM, Gto::Int, 1, 1);
      writer->endComponent();
    }
    
    writer->beginComponent(GTO_COMPONENT_OBJECT);
    writer->property(GTO_PROPERTY_BOUNDINGBOX, Gto::Float, 1, 6);
    if (writer->isDiff())
    {
      writer->intern(GTO_PROTOCOL_DIFFERENCE);
      writer->property(GTO_PROPERTY_PROTOCOL, Gto::String, 1, 1);
    }
    else
    {
      // What about instances?
      if (mParent != 0)
      {
        writer->property(GTO_PROPERTY_PARENT, Gto::String, 1, 1);
      }
    }
    writer->endComponent();
    
    writer->endObject();
  }
  
  return true;
}

void Curve::write(HouGtoWriter *writer)
{
  if (mNumPoints > 0)
  {
    
    writer->propertyData(&mPositions[0]);
    writer->propertyData(&mWeights[0]);

    if (!writer->isDiff())
    {
      float range[2] = {mKnots.front(), mKnots.back()};
      
      writer->propertyData(&mDegree);
      writer->propertyData(&mKnots[0]);
      writer->propertyData(range);
      writer->propertyData(&mForm);
    }
    
    writer->propertyData(mBBox);
    
    if (writer->isDiff())
    {
      int protocol = writer->lookup(GTO_PROTOCOL_DIFFERENCE);
      writer->propertyData(&protocol);
    }
    else
    {
      if (mParent != 0)
      {
        int name = writer->lookup(mParent->getName());
        writer->propertyData(&name);
      }
    }
  }
}

bool Curve::isValid()
{
  return !mDirty;
}

void Curve::invalidate()
{
  // NOOP ?
}

void Curve::update()
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
  
  // Transform points and compute bounding box

  box.makeInvalid();

  for (unsigned int i=0, j=0; i<mNumPoints; ++i, j+=3)
  {
    v.assign(mPositions[j], mPositions[j+1], mPositions[j+2], 1.0f);
    v = v * mat;
    
    box.enlargeBounds(v);
    
    mPositions[j] = v.x();
    mPositions[j+1] = v.y();
    mPositions[j+2] = v.z();
  }

  // Update bounding box

  mBBox[0] = box.xmin();
  mBBox[1] = box.ymin();
  mBBox[2] = box.zmin();
  mBBox[3] = box.xmax();
  mBBox[4] = box.ymax();
  mBBox[5] = box.zmax();
  
  mDirty = false;
}

void Curve::mergeBounds(const UT_Matrix4 &space, UT_BoundingBox &box)
{
  UT_BoundingBox thisBox(mBBox[0], mBBox[1], mBBox[2], mBBox[3], mBBox[4], mBBox[5]);
  
  thisBox.transform(space);
  
  box.enlargeBounds(thisBox);
}

void Curve::setName(const std::string &name)
{
  mName = name;
}

const std::string& Curve::getName() const
{
  return mName;
}

void Curve::setParent(Transform *t)
{
  mParent = t;
}

Transform* Curve::getParent() const
{
  return mParent;
}

void Curve::cleanup()
{
  mPositions.clear();
  mWeights.clear();
  mKnots.clear();
  mNumPoints = 0;
  mDegree = 0;
  mForm = 1;
}

