#ifndef __ROP_GtoExport_particles_h__
#define __ROP_GtoExport_particles_h__

#include <UT/UT_Version.h>
#include <UT/UT_Vector3.h>
#include <GU/GU_Detail.h>
#if UT_MAJOR_VERSION_INT >= 12
#  include <GA/GA_Group.h>
#else
#  include <GB/GB_Group.h>
#endif
#include <Gto/Header.h>
#include <string>
#include <map>
#include "object.h"
#include "userattr.h"

class Transform;
class HouGtoWriter;

class Particles : public Object
{
public:

  Particles(Transform *p=0);
  virtual ~Particles();
  
#if UT_MAJOR_VERSION_INT >= 12
  virtual bool declare(HouGtoWriter *writer, GU_Detail *gdp=0, const std::vector<GA_Offset> &prims=NoPrimitives);
#else
  virtual bool declare(HouGtoWriter *writer, GU_Detail *gdp=0, const std::vector<unsigned int> &prims=NoPrimitives);
#endif
  virtual void write(HouGtoWriter *writer);
  
  virtual bool isValid();
  virtual void invalidate();
  virtual void update();
  virtual void mergeBounds(const UT_Matrix4 &space, UT_BoundingBox &box);
  
  
  void setName(const std::string &name);
  const std::string& getName() const;
  
  void setParent(Transform *t);
  Transform* getParent() const;
  
  void cleanup();

protected:

  Transform *mParent;
  std::string mName;
  
  unsigned int mNumPoints;
  
  float mBBox[6];
  
  std::vector<float> mPositions;
  std::vector<float> mVelocities;
  std::vector<int> mIDs;
  std::vector<float> mSizes;
  std::vector<float> mMultiRadius;
  std::vector<int> mMultiCount;
  float mMinVelocity[3];
  float mMaxVelocity[3];
  float mMaxVelocityMag;
  float mMaxSize;
  float mMaxMultiRadius;
  bool mSizePerParticle;
  bool mMultiCountPerParticle;
  bool mMultiRadiusPerParticle;
  std::string mRenderType;
  
  AttribDict mPointAttribs;
  AttribDict mObjectAttribs;
  
  bool mDirty;
};

#endif

