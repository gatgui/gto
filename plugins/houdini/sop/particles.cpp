#include "particles.h"
#include "reader.h"
#include "transform.h"
#include "bounds.h"
#include <Gto/Protocols.h>
#include <UT/UT_Version.h>
#include <GU/GU_Detail.h>
#include <GU/GU_PrimPart.h>


Particles::Particles(const std::string &name, bool boundsOnly, bool asDiff)
   : Object(name, asDiff)
   , mParent(0)
   , mNumParticles(-1)
   , mPositions(0)
   , mVelocities(0)
   , mIDs(0)
   , mBoundsOnly(boundsOnly)
{
}

Particles::~Particles()
{
   cleanup();
}

Gto::Reader::Request Particles::component(Reader *rdr,
                                          const std::string& name,
                                          const std::string& interp,
                                          const Gto::Reader::ComponentInfo &header)
{
   if (mBoundsOnly)
   {
      return Object::component(rdr, name, interp, header);
   }

   if (name == GTO_COMPONENT_POINTS)
   {
      return Gto::Reader::Request(true, (void*) POINTS_C);
   }
   else if (name == GTO_COMPONENT_PARTICLE)
   {
      return Gto::Reader::Request(true, (void*) PARTICLE_C);
   }
   else if (name == GTO_COMPONENT_POINT_ATTRIBS)
   {
      return Gto::Reader::Request(true, (void*) POINT_ATTRIBS_C);
   }
   else if (name == GTO_COMPONENT_OBJECT_ATTRIBS)
   {
      return Gto::Reader::Request(true, (void*) OBJECT_ATTRIBS_C);
   }
   
   return Object::component(rdr, name, interp, header);
}

Gto::Reader::Request Particles::property(Reader *rdr,
                                         const std::string& name,
                                         const std::string& interp,
                                         const Gto::Reader::PropertyInfo &header)
{
   void *what = header.component->componentData;
   
   if (mBoundsOnly)
   {
      if (what == (void*) READ_OBJECT)
      {
         if (name == GTO_PROPERTY_BOUNDINGBOX)
         {
            return Gto::Reader::Request(true, (void*) BOUNDS_P);
         }
      }

      return Object::property(rdr, name, interp, header);
   }

   // other special properties
   // [str] particle.renderType = line|sprite|sphere|point
   // [float] points.size -> particle.size
   // [int] points.multiCount -> particle.multiCount
   // [float] points.multiRadius -> particle.multiRadius
   // [float] points.rotation [for sprite renderType]
   // [float] points.aspect [for sprite renderType]

   if (what == (void*) POINTS_C)
   {
      if (name == GTO_PROPERTY_POSITION)
      {
         return Gto::Reader::Request(true, (void*) POSITIONS_P);
      }
      else if (name == GTO_PROPERTY_VELOCITY)
      {
         return Gto::Reader::Request(true, (void*) VELOCITY_P);
      }
      else if (name == "id")
      {
         return Gto::Reader::Request(true, (void*) ID_P);
      }
      else
      {
         std::string name = rdr->stringFromId(header.name);
         std::string interp = rdr->stringFromId(header.interpretation);
         AttribDictEntry &entry = mPointAttribs[name];
         if (entry.second.init((Gto::DataType)header.type, header.width, interp))
         {
            entry.second.resize(header.size);
            return Gto::Reader::Request(true, (void*) POINT_USER_P);
         }
         else
         {
            mPointAttribs.erase(mPointAttribs.find(name));
            return Gto::Reader::Request(false);
         }
      }
   }
   else if (what == (void*) PARTICLE_C)
   {
      std::string name = rdr->stringFromId(header.name);
      std::string interp = rdr->stringFromId(header.interpretation);
      AttribDictEntry &entry = mPrimAttribs[name];
      if (entry.second.init((Gto::DataType)header.type, header.width, interp))
      {
         entry.second.resize(header.size);
         return Gto::Reader::Request(true, (void*) PARTICLE_USER_P);
      }
      else
      {
         mPrimAttribs.erase(mPrimAttribs.find(name));
         return Gto::Reader::Request(false);
      }
   }
   else if (what == (void*) POINT_ATTRIBS_C)
   {
      std::string name = rdr->stringFromId(header.name);
      std::string interp = rdr->stringFromId(header.interpretation);
      AttribDictEntry &entry = mPointAttribs[name];
      if (entry.second.init((Gto::DataType)header.type, header.width, interp))
      {
         entry.second.resize(header.size);
         return Gto::Reader::Request(true, (void*) POINT_ATTRIBS_USER_P);
      }
      else
      {
         mPointAttribs.erase(mPointAttribs.find(name));
         return Gto::Reader::Request(false);
      }
   }
   else if (what == (void*) OBJECT_ATTRIBS_C)
   {
      std::string name = rdr->stringFromId(header.name);
      std::string interp = rdr->stringFromId(header.interpretation);
      AttribDictEntry &entry = mPrimAttribs[name];
      if (entry.second.init((Gto::DataType)header.type, header.width, interp))
      {
         entry.second.resize(header.size);
         return Gto::Reader::Request(true, (void*) OBJECT_ATTRIBS_USER_P);
      }
      else
      {
         mPrimAttribs.erase(mPrimAttribs.find(name));
         return Gto::Reader::Request(false);
      }
   }

   return Object::property(rdr, name, interp, header);
}

void* Particles::data(Reader *rdr, const Gto::Reader::PropertyInfo &header, size_t bytes)
{
   void *what = header.propertyData;

   if (mBoundsOnly)
   {
      if (what == (void*) BOUNDS_P)
      {
         if (header.type != Gto::Float || header.width != 6 || header.size != 1)
         {
            return NULL;
         }
         return mBounds;
      }

      return Object::data(rdr, header, bytes);
   }

   if (what == (void*) POSITIONS_P)
   {
      if (header.type != Gto::Float || header.width != 3 || (mNumParticles >= 0 && header.size != mNumParticles))
      {
         return NULL;
      }
      mNumParticles = header.size;
      mPositions = new float[header.width * header.size];
      return mPositions;
   }
   else if (what == (void*) VELOCITY_P)
   {
      if (header.type != Gto::Float || header.width != 3 || (mNumParticles >= 0 && header.size != mNumParticles))
      {
         return NULL;
      }
      mNumParticles = header.size;
      mVelocities = new float[header.width * header.size];
      return mVelocities;
   }
   else if (what == (void*) ID_P)
   {
      if (header.type != Gto::Int || header.width != 1 || (mNumParticles >= 0 && header.size != mNumParticles))
      {
         return NULL;
      }
      mNumParticles = header.size;
      mIDs = new int[header.size];
      return mIDs;
   }
   else if (what == (void*) POINT_ATTRIBS_USER_P || what == (void*) POINT_USER_P)
   {
      std::string name = rdr->stringFromId(header.name);
      return mPointAttribs[name].second.data();
   }
   else if (what == (void*) OBJECT_ATTRIBS_USER_P || what == (void*) PARTICLE_USER_P)
   {
      std::string name = rdr->stringFromId(header.name);
      return mPrimAttribs[name].second.data();
   }
   
   return Object::data(rdr, header, bytes);
}

void Particles::dataRead(Reader *rdr, const Gto::Reader::PropertyInfo &header)
{
   if (!mBoundsOnly)
   {
      void *what = header.propertyData;
      
      if (what == (void*) ID_P)
      {
         for (int i=0; i<mNumParticles; ++i)
         {
            mIDtoIdx[mIDs[i]] = i;
         }
      }
   }

   Object::dataRead(rdr, header);
}

bool Particles::toHoudini(Reader *reader,
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
   
   if (!opts.ignoreVisibility && !isVisible(-1))
   {
      return true;
   }
   
   if (mBoundsOnly)
   {
      return GenerateBox<Particles>(reader, ctx, gdp, opts, this, mInstanceNum, obj0, obj1, blend, t0, t1);
   }

   UT_Matrix4 world(1.0f);
   UT_Matrix4 pworld(1.0f), nworld(1.0f);
      
   if (mNumParticles <= 0)
   {
      return false;
   }
   
#if UT_MAJOR_VERSION_INT >= 12
   GA_PrimitiveGroup *pmgroup = 0;
#else
   GB_PrimitiveGroup *pmgroup = 0;
#endif
   
   if (!opts.computeInstance || !opts.separateInstance)
   {
      pmgroup = gdp->findPrimitiveGroup(getName().c_str());
   }
   if (!pmgroup)
   {
      if (opts.separateInstance && mInstanceNum > 0)
      {
         std::ostringstream oss;
         oss << getName() << "_" << mInstanceNum;
         pmgroup = gdp->newPrimitiveGroup(oss.str().c_str());
      }
      else
      {
         pmgroup = gdp->newPrimitiveGroup(getName().c_str());
      }
   }
      
   // compute interpolated positions if necessary
   float *positions = mPositions;
   float *velocities = mVelocities;
   int *ids = mIDs;
   int numParticles = mNumParticles;
   bool interpolated = false;
   AttribDict *pointAttribs = &mPointAttribs;
   AttribDict *primAttribs = &mPrimAttribs;
   
   // Not that simple for particles [see options in arnold:gto]
   // MultiSample particles?
   //   -> if so, remove dead particles?
   // MultiPoint expansion? Not here
   // NO:
   //   => only do most basic interpolation
   if (obj0)
   {
      if (obj1)
      {
         float a = 1.0f - blend;
         float b = blend;
         
         interpolated = true;
         
         UT_Matrix4 wm0(1.0f);
         UT_Matrix4 wm1(1.0f);
         
         if (mParent && getName() != opts.shapeName && !opts.ignoreTransforms)
         {
            wm0 = mParent->updateWorldMatrix(opts.globalScale, -1);
            wm1 = mParent->updateWorldMatrix(opts.globalScale,  1);
         }
         
         Particles *p0 = (Particles*)obj0;
         Particles *p1 = (Particles*)obj1;
         std::map<int, int>::const_iterator iit;
         std::set<int> processedIds;

         numParticles = p0->mNumParticles;
         for (int i=0; i<p1->mNumParticles; ++i)
         {
            if (p0->mIDtoIdx.find(p1->mIDs[i]) == p0->mIDtoIdx.end())
            {
               ++numParticles;
            }
         }
         
         positions = new float[numParticles * 3];
         velocities = new float[numParticles * 3];
         ids = new int[numParticles];
         pointAttribs = new AttribDict(p0->mPointAttribs);
         primAttribs = new AttribDict(p0->mPrimAttribs);

         for (int i=0, j=0; i<p0->mNumParticles; ++i, j+=3)
         {
            UT_Vector4 pp(p0->mPositions[j] * opts.globalScale, p0->mPositions[j+1] * opts.globalScale, p0->mPositions[j+2] * opts.globalScale, 1.0f);
            UT_Vector4 pv(p0->mVelocities[j] * opts.globalScale, p0->mVelocities[j+1] * opts.globalScale, p0->mVelocities[j+2] * opts.globalScale, 0.0f);
            pp = pp * wm0;
            pv = pv * wm0;

            iit = p1->mIDtoIdx.find(p0->mIDs[i]);
            if (iit != p1->mIDtoIdx.end())
            {
               int k = iit->second * 3;
               UT_Vector4 np(p1->mPositions[k] * opts.globalScale, p1->mPositions[k+1] * opts.globalScale, p1->mPositions[k+2] * opts.globalScale, 1.0f);
               UT_Vector4 nv(p1->mVelocities[k] * opts.globalScale, p1->mVelocities[k+1] * opts.globalScale, p1->mVelocities[k+2] * opts.globalScale, 0.0f);
               np = np * wm1;
               nv = nv * wm1;
               
               positions[j]   = a * pp.x() + b * np.x();
               positions[j+1] = a * pp.y() + b * np.y();
               positions[j+2] = a * pp.z() + b * np.z();

               velocities[j]   = a * pv.x() + b * nv.x();
               velocities[j+1] = a * pv.y() + b * nv.y();
               velocities[j+2] = a * pv.z() + b * nv.z();
            }
            else
            {
               // extrapolate forwards
               
               positions[j]   = pp.x();
               positions[j+1] = pp.y();
               positions[j+2] = pp.z();

               velocities[j]   = pv.x();
               velocities[j+1] = pv.y();
               velocities[j+2] = pv.z();
            }

            ids[i] = p0->mIDs[i];
         }

         for (int i=0, j=0, k=p0->mNumParticles; i<p1->mNumParticles; ++i, j+=3)
         {
            iit = p0->mIDtoIdx.find(p1->mIDs[i]);
            if (iit == p0->mIDtoIdx.end())
            {
               ids[k] = p1->mIDs[i];

               int l = k * 3;
               
               // extrapolate backwards
               
               UT_Vector4 p(p1->mPositions[j] * opts.globalScale, p1->mPositions[j+1] * opts.globalScale, p1->mPositions[j+2] * opts.globalScale, 1.0f);
               UT_Vector4 v(p1->mVelocities[j] * opts.globalScale, p1->mVelocities[j+1] * opts.globalScale, p1->mVelocities[j+2] * opts.globalScale, 0.0f);
               p = p * wm1;
               v = v * wm1;
               
               positions[l]   = p.x();
               positions[l+1] = p.y();
               positions[l+2] = p.z();

               velocities[l]   = v.x();
               velocities[l+1] = v.y();
               velocities[l+2] = v.z();
               
               // expand pointAttribs and primAttribs data
               // => new API in Attrib
               AttribDict::iterator it0, it1;

               it0 = pointAttribs->begin();
               while (it0 != pointAttribs->end())
               {
                  it1 = p1->mPointAttribs.find(it0->first);
                  if (it1 != p1->mPointAttribs.end())
                  {
                     Attrib &a0 = it0->second.second;
                     Attrib &a1 = it1->second.second;
                     a0.append(a1, k);
                  }
                  ++it0;
               }

               it0 = primAttribs->begin();
               while (it0 != primAttribs->end())
               {
                  it1 = p1->mPrimAttribs.find(it0->first);
                  if (it1 != p1->mPrimAttribs.end())
                  {
                     Attrib &a0 = it0->second.second;
                     Attrib &a1 = it1->second.second;
                     a0.append(a1, k);
                  }
                  ++it0;
               }

               ++k;
            }
         }
      }
      else
      {
         Particles *p0 = (Particles*)obj0;

         positions = p0->mPositions;
         velocities = p0->mVelocities;
         ids = p0->mIDs;
         pointAttribs = &(p0->mPointAttribs);
         primAttribs = &(p0->mPrimAttribs);
         numParticles = p0->mNumParticles;

         if (mParent && getName() != opts.shapeName && !opts.ignoreTransforms)
         {
            world = mParent->updateWorldMatrix(opts.globalScale, -1);
         }
      }
   }
   else
   {
      if (obj1)
      {
         Particles *p1 = (Particles*)obj1;

         positions = p1->mPositions;
         velocities = p1->mVelocities;
         ids = p1->mIDs;
         pointAttribs = &(p1->mPointAttribs);
         primAttribs = &(p1->mPrimAttribs);
         numParticles = p1->mNumParticles;

         if (mParent && getName() != opts.shapeName && !opts.ignoreTransforms)
         {
            world = mParent->updateWorldMatrix(opts.globalScale, 1);
         }
      }
      else
      {
         // no interpolation/extrapolation needed
         if (mParent && getName() != opts.shapeName && !opts.ignoreTransforms)
         {
            if (blend <= 0.0f)
            {
               world = mParent->updateWorldMatrix(opts.globalScale, -1);
            }
            else
            {
               pworld = mParent->updateWorldMatrix(opts.globalScale, -1);
               nworld = mParent->updateWorldMatrix(opts.globalScale,  1);
            }
         }
      }
   }
   
   GU_PrimParticle *parts = GU_PrimParticle::build(gdp, 0);

#if UT_MAJOR_VERSION_INT >= 12
   std::vector<GA_Offset> poffsets(numParticles);
   //pmgroup->makeOrdered();
#else
   GEO_PointList &pl = gdp->points();
   size_t poffset = pl.entries();
#endif

   // add points
   for (int i=0, j=0; i<numParticles; ++i, j+=3)
   {
      UT_Vector4 p(positions[j], positions[j+1], positions[j+2], 1.0f);
      
      if (!interpolated)
      {
         p.x() *= opts.globalScale;
         p.y() *= opts.globalScale;
         p.z() *= opts.globalScale;
         
         if (blend <= 0.0f)
         {
            p = p * world;
         }
         else
         {
            p = (1.0f - blend) * (p * pworld) + blend * (p * nworld);
         }
      }
      
#if UT_MAJOR_VERSION_INT >= 12
      GA_Offset po = gdp->appendPointOffset();
      gdp->setPos4(po, p);
      poffsets[i] = po;
#else
      GEO_Point *pp = gdp->appendPoint();
      pp->setPos(p);
#endif
   }
   
   // all particles already created just set vertex offsets
#if UT_MAJOR_VERSION_INT >= 12
   for (int i=0; i<numParticles; ++i)
   {
      parts->appendParticle(poffsets[i]);
   }
#else
   for (int i=0; i<numParticles; ++i)
   {
      parts->appendParticle(pl[poffset+i]);
   }
#endif
   
#if UT_MAJOR_VERSION_INT >= 12
   
   GA_RWAttributeRef hvel = gdp->findPointAttribute("v");
   if (!hvel.isValid())
   {
      hvel = gdp->addFloatTuple(GA_ATTRIB_POINT, GA_SCOPE_PUBLIC, "v", 3);
      hvel.getAttribute()->setTypeInfo(GA_TYPE_VECTOR);
   }
   
   GA_RWAttributeRef hid = gdp->findPointAttribute("id");
   if (!hid.isValid())
   {
      hid = gdp->addIntTuple(GA_ATTRIB_POINT, GA_SCOPE_PUBLIC, "id", 1);
   }
   
   GA_Attribute *avel = hvel.getAttribute();
   GA_Attribute *aid = hid.getAttribute();
   
   const GA_AIFTuple *aifvel = hvel.getAIFTuple();
   const GA_AIFTuple *aifid = hid.getAIFTuple();
   
   for (size_t i=0, j=0; i<poffsets.size(); ++i, j+=3)
   {
      aifid->set(aid, poffsets[i], ids[i], 0);

      if (velocities)
      {
         UT_Vector4 v(velocities[j], velocities[j+1], velocities[j+2], 0.0f);
        
         if (!interpolated)
         {
            v.x() *= opts.globalScale;
            v.y() *= opts.globalScale;
            v.z() *= opts.globalScale;

            if (blend <= 0.0f)
            {
               v = v * world;
            }
            else
            {
               v = (1.0f - blend) * (v * pworld) + blend * (v * nworld);
            }
         }
        
         aifvel->set(avel, poffsets[i], v.x(), 0);
         aifvel->set(avel, poffsets[i], v.y(), 1);
         aifvel->set(avel, poffsets[i], v.z(), 2);
      }
      else
      {
         aifvel->set(avel, poffsets[i], 0.0, 0);
         aifvel->set(avel, poffsets[i], 0.0, 1);
         aifvel->set(avel, poffsets[i], 0.0, 2);
      }
   }

   pointAttribs->create(gdp, GA_ATTRIB_POINT, poffsets);
   pointAttribs->toHoudini(reader, opts.globalScale);

   poffsets.resize(1);
   poffsets[0] = gdp->primitiveOffset(gdp->getNumPrimitives()-1);
   primAttribs->create(gdp, GA_ATTRIB_PRIMITIVE, poffsets);
   primAttribs->toHoudini(reader, opts.globalScale);

   // Add instance number primitive attribute
   GA_RWAttributeRef aInstNum = gdp->findAttribute(GA_ATTRIB_PRIMITIVE, GA_SCOPE_PUBLIC, "instance_num");
   if (!aInstNum.isValid())
   {
      aInstNum = gdp->addIntTuple(GA_ATTRIB_PRIMITIVE, GA_SCOPE_PUBLIC, "instance_num", 1);
   }
   aInstNum.getAIFTuple()->set(aInstNum.get(), gdp->primitiveOffset(gdp->getNumPrimitives()-1), mInstanceNum, 0);

#else
   
   GEO_AttributeHandle hvel = gdp->getAttribute(GEO_POINT_DICT, "v");
   if (!hvel.isAttributeValid())
   {
      float def[3] = {0.0f, 0.0f, 0.0f};
      gdp->addAttribute("v", 3*sizeof(float), GB_ATTRIB_VECTOR, def, GEO_POINT_DICT);
      hvel = gdp->getAttribute(GEO_POINT_DICT, "v");
   }

   GEO_AttributeHandle hid = gdp->getAttribute(GEO_POINT_DICT, "id");
   if (!hid.isAttributeValid())
   {
      int def[1] = {0};
      gdp->addAttribute("id", sizeof(int), GB_ATTRIB_INT, def, GEO_POINT_DICT);
      hid = gdp->getAttribute(GEO_POINT_DICT, "id");
   }

   for (int i=0, j=0; i<numParticles; ++i, j+=3)
   {
      GEO_Point *pt = pl[poffset + i];

      hid.setElement(pt);
      hid.setI(ids[i]);

      hvel.setElement(pt);

      if (velocities)
      {
         UT_Vector4 v(velocities[j], velocities[j+1], velocities[j+2], 0.0f);
         
         if (!interpolated)
         {
            v.x() *= opts.globalScale;
            v.y() *= opts.globalScale;
            v.z() *= opts.globalScale;
            
            if (blend <= 0.0f)
            {
               v = v * world;
            }
            else
            {
               v = (1.0f - blend) * (v * pworld) + blend * (v * nworld);
            }
         }

         hvel.setF(v.x(), 0);
         hvel.setF(v.y(), 1);
         hvel.setF(v.z(), 2);
      }
      else
      {
         hvel.setF(0.0, 0);
         hvel.setF(0.0, 1);
         hvel.setF(0.0, 2);
      }
   }

   pointAttribs->create(gdp, GEO_POINT_DICT, poffset, numParticles);
   pointAttribs->toHoudini(reader, opts.globalScale);

   primAttribs->create(gdp, GEO_PRIMITIVE_DICT, gdp->primitives().entries()-1, 1);
   primAttribs->toHoudini(reader, opts.globalScale);

   // Add instance number primitive attribute
   GEO_AttributeHandle aInstNum = gdp->getAttribute(GEO_PRIMITIVE_DICT, "instance_num");
   if (!aInstNum.isAttributeValid())
   {
      gdp->addAttribute("instance_num", sizeof(int), GB_ATTRIB_INT, NULL, GEO_PRIMITIVE_DICT);
      aInstNum = gdp->getAttribute(GEO_PRIMITIVE_DICT, "instance_num");
   }
   aInstNum.setElement(crv);
   aInstNum.setI(mInstanceNum, 0);
   
#endif
   
   if (interpolated)
   {
      delete[] positions;
      delete[] velocities;
      delete[] ids;
      delete pointAttribs;
      delete primAttribs;
   }
  
   // add primitive to group
   if (pmgroup)
   {
      pmgroup->add(parts);
   }
  
   if (getParent() != 0)
   {
      reader->setParent(getName(), getParent()->getName());
   }
   
   return true;
}

void Particles::resolveParent(Reader *reader)
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

Transform* Particles::getParent() const
{
   return mParent;
}

void Particles::setParent(Transform *t)
{
   mParent = t;
   setParentObject(t);
}

bool Particles::switchParent(Reader *reader, size_t idx)
{
  if (Object::switchParent(reader, idx))
  {
    mParent = dynamic_cast<Transform*>(mParentObject);
    return true;
  }
  return false;
}

void Particles::cleanup()
{
   Object::cleanup();

   if (mPositions != 0)
   {
      delete[] mPositions;
   }
   if (mVelocities != 0)
   {
      delete[] mVelocities;
   }
   if (mIDs != 0)
   {
      delete[] mIDs;
   }
   
   mParent = 0;
   mNumParticles = 0;
   mPositions = 0;
   mVelocities = 0;
   mIDs = 0;
   mIDtoIdx.clear();
}
