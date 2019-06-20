#ifndef __SOP_GtoImport_particles_h__
#define __SOP_GtoImport_particles_h__

#include "object.h"
#include "userattr.h"
#include <map>

class Transform;

class Particles : public Object
{
public:

   // Components
   enum
   {
      // Components
      POINTS_C = OBJECT_COMP_MAX,
      PARTICLE_C,
      POINT_ATTRIBS_C,
      OBJECT_ATTRIBS_C,
      // Properties
      POSITIONS_P = OBJECT_PROP_MAX,
      VELOCITY_P,
      ID_P,
      POINT_USER_P,
      PARTICLE_USER_P,
      POINT_ATTRIBS_USER_P,
      OBJECT_ATTRIBS_USER_P,
      BOUNDS_P
   };

public:

   Particles(const std::string &name, bool boundsOnly, bool asDiff=false);
  
   virtual ~Particles();
  
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
   
   
   Transform* getParent() const;

   void setParent(Transform *t);

   virtual bool switchParent(Reader *reader, size_t idx);
   
   void cleanup();

   //void scaleSize(Args *args, UserAttribute &attr);
   
   inline UT_BoundingBoxF getBounds() const { return UT_BoundingBoxF(mBounds[0], mBounds[1], mBounds[2], mBounds[3], mBounds[4], mBounds[5]); }

protected:

   Transform *mParent;
   
   int mNumParticles;
   float *mPositions;
   float *mVelocities;
   int *mIDs;
   std::map<int, int> mIDtoIdx;

   AttribDict mPointAttribs;
   AttribDict mPrimAttribs;

   bool mBoundsOnly;
   float mBounds[6];
};

#endif
