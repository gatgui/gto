#ifndef __sop_gto_bounds_h__
#define __sop_gto_bounds_h__

#include "reader.h"
#include <sstream>
#include <OP/OP_Context.h>
#include <GU/GU_Detail.h>
#include <GU/GU_PrimPoly.h>
#include <UT/UT_Vector.h>
#include <UT/UT_BoundingBox.h>

template <typename ObjectClass>
bool GenerateBox(Reader *reader,
                 OP_Context &ctx,
                 GU_Detail *gdp,
                 const ExpandOptions &opts,
                 ObjectClass *self,
                 int instanceNum,
                 Object *obj0,
                 Object *obj1,
                 float blend,
                 float t0,
                 float t1,
                 float padding=0.0f)
{
  UT_Matrix4 world(1.0f);
  UT_Matrix4 pworld(1.0f), nworld(1.0f);
  Transform *parent = self->getParent();

  UT_BoundingBoxF box;

#if UT_MAJOR_VERSION_INT >= 12
  GA_PrimitiveGroup *pmgroup = 0;
#else
  GB_PrimitiveGroup *pmgroup = 0;
#endif
  
  if (!opts.computeInstance || !opts.separateInstance)
  {
    pmgroup = gdp->findPrimitiveGroup(self->getName().c_str());
  }
  if (!pmgroup)
  {
    if (opts.separateInstance && instanceNum > 0)
    {
      std::ostringstream oss;
      oss << self->getName() << "_" << instanceNum;
      pmgroup = gdp->newPrimitiveGroup(static_cast<std::ostringstream&>(oss).str().c_str());
    }
    else
    {
      pmgroup = gdp->newPrimitiveGroup(self->getName().c_str());
    }
  }

  if (obj0)
  {
    if (obj1)
    {
      UT_Matrix4 wm0(1.0f);
      UT_Matrix4 wm1(1.0f);

      if (parent && self->getName() != opts.shapeName && !opts.ignoreTransforms)
      {
        wm0 = parent->updateWorldMatrix(opts.globalScale, -1);
        wm1 = parent->updateWorldMatrix(opts.globalScale,  1);
      }

      ObjectClass *c0 = (ObjectClass*) obj0;
      ObjectClass *c1 = (ObjectClass*) obj1;

      UT_BoundingBoxF box0 = c0->getBounds();
      UT_BoundingBoxF box1 = c1->getBounds();
      
      box0.expandBounds(0.0f, padding);
      box1.expandBounds(0.0f, padding);
      
      box0.initBounds(box0.minvec() * opts.globalScale, box0.maxvec() * opts.globalScale);
      box1.initBounds(box1.minvec() * opts.globalScale, box1.maxvec() * opts.globalScale);

      box0.transform(wm0);
      box1.transform(wm1);

      box = box0;
      box.enlargeBounds(box1);
    }
    else
    {
      if (parent && self->getName() != opts.shapeName && !opts.ignoreTransforms)
      {
        world = parent->updateWorldMatrix(opts.globalScale, -1);
      }

      ObjectClass *c = (ObjectClass*) obj0;

      box = c->getBounds();
      box.expandBounds(0.0f, padding);
      box.initBounds(box.minvec() * opts.globalScale, box.maxvec() * opts.globalScale);
      box.transform(world);
    }
  }
  else
  {
    box = self->getBounds();
    box.expandBounds(0.0f, padding);
    box.initBounds(box.minvec() * opts.globalScale, box.maxvec() * opts.globalScale);
    
    if (parent && self->getName() != opts.shapeName && !opts.ignoreTransforms)
    {
      if (blend <= 0.0f)
      {
        world = parent->updateWorldMatrix(opts.globalScale, -1);
        box.transform(world);
      }
      else
      {
        pworld = parent->updateWorldMatrix(opts.globalScale, -1);
        nworld = parent->updateWorldMatrix(opts.globalScale,  1);

        UT_BoundingBoxF box0 = box;
        UT_BoundingBoxF box1 = box;

        box0.transform(pworld);
        box1.transform(nworld);

        box = box0;
        box.enlargeBounds(box1);
      }
    }
  }

  // Create points

  UT_Vector4 p[8];
  GEO_PrimPoly *polies[6];

  p[0] = UT_Vector4(box.xmin(), box.ymin(), box.zmin(), 1.0f);
  p[1] = UT_Vector4(box.xmax(), box.ymin(), box.zmin(), 1.0f);
  p[2] = UT_Vector4(box.xmax(), box.ymax(), box.zmin(), 1.0f);
  p[3] = UT_Vector4(box.xmin(), box.ymax(), box.zmin(), 1.0f);
  p[4] = UT_Vector4(box.xmin(), box.ymin(), box.zmax(), 1.0f);
  p[5] = UT_Vector4(box.xmax(), box.ymin(), box.zmax(), 1.0f);
  p[6] = UT_Vector4(box.xmax(), box.ymax(), box.zmax(), 1.0f);
  p[7] = UT_Vector4(box.xmin(), box.ymax(), box.zmax(), 1.0f);

  polies[0] = GU_PrimPoly::build(gdp, 4, GU_POLY_OPEN, 0);
  polies[1] = GU_PrimPoly::build(gdp, 4, GU_POLY_OPEN, 0);
  polies[2] = GU_PrimPoly::build(gdp, 4, GU_POLY_OPEN, 0);
  polies[3] = GU_PrimPoly::build(gdp, 4, GU_POLY_OPEN, 0);
  polies[4] = GU_PrimPoly::build(gdp, 4, GU_POLY_OPEN, 0);
  polies[5] = GU_PrimPoly::build(gdp, 4, GU_POLY_OPEN, 0);

#if UT_MAJOR_VERSION_INT >= 12

  std::vector<GA_Offset> poffsets(8);

  for (int i=0; i<8; ++i)
  {
    poffsets[i] = gdp->appendPointOffset();
    gdp->setPos4(poffsets[i], p[i]);
  }

  polies[0]->setVertexPoint(0, poffsets[0]);
  polies[0]->setVertexPoint(1, poffsets[1]);
  polies[0]->setVertexPoint(2, poffsets[2]);
  polies[0]->setVertexPoint(3, poffsets[3]);
  
  polies[1]->setVertexPoint(0, poffsets[1]);
  polies[1]->setVertexPoint(1, poffsets[5]);
  polies[1]->setVertexPoint(2, poffsets[6]);
  polies[1]->setVertexPoint(3, poffsets[2]);
  
  polies[2]->setVertexPoint(0, poffsets[5]);
  polies[2]->setVertexPoint(1, poffsets[4]);
  polies[2]->setVertexPoint(2, poffsets[7]);
  polies[2]->setVertexPoint(3, poffsets[6]);

  polies[3]->setVertexPoint(0, poffsets[2]);
  polies[3]->setVertexPoint(1, poffsets[6]);
  polies[3]->setVertexPoint(2, poffsets[7]);
  polies[3]->setVertexPoint(3, poffsets[3]);

  polies[4]->setVertexPoint(0, poffsets[0]);
  polies[4]->setVertexPoint(1, poffsets[3]);
  polies[4]->setVertexPoint(2, poffsets[7]);
  polies[4]->setVertexPoint(3, poffsets[4]);

  polies[5]->setVertexPoint(0, poffsets[0]);
  polies[5]->setVertexPoint(1, poffsets[4]);
  polies[5]->setVertexPoint(2, poffsets[5]);
  polies[5]->setVertexPoint(3, poffsets[1]);

#else

  GEO_PointList &pl = gdp->points();
  size_t poffset = pl.entries();

  for (int i=0; i<8; ++i)
  {
    GEO_Point *pp = gdp->appendPoint();
    pp->setPos(p[i]);
  }

  polies[0]->getVertex(0).setPt(pl.entry(poffset + 0));
  polies[0]->getVertex(1).setPt(pl.entry(poffset + 1));
  polies[0]->getVertex(2).setPt(pl.entry(poffset + 2));
  polies[0]->getVertex(3).setPt(pl.entry(poffset + 3));

  polies[1]->getVertex(0).setPt(pl.entry(poffset + 1));
  polies[1]->getVertex(1).setPt(pl.entry(poffset + 5));
  polies[1]->getVertex(2).setPt(pl.entry(poffset + 6));
  polies[1]->getVertex(3).setPt(pl.entry(poffset + 2));

  polies[2]->getVertex(0).setPt(pl.entry(poffset + 5));
  polies[2]->getVertex(1).setPt(pl.entry(poffset + 4));
  polies[2]->getVertex(2).setPt(pl.entry(poffset + 7));
  polies[2]->getVertex(3).setPt(pl.entry(poffset + 6));

  polies[3]->getVertex(0).setPt(pl.entry(poffset + 2));
  polies[3]->getVertex(1).setPt(pl.entry(poffset + 6));
  polies[3]->getVertex(2).setPt(pl.entry(poffset + 7));
  polies[3]->getVertex(3).setPt(pl.entry(poffset + 3));

  polies[4]->getVertex(0).setPt(pl.entry(poffset + 0));
  polies[4]->getVertex(1).setPt(pl.entry(poffset + 3));
  polies[4]->getVertex(2).setPt(pl.entry(poffset + 7));
  polies[4]->getVertex(3).setPt(pl.entry(poffset + 4));

  polies[5]->getVertex(0).setPt(pl.entry(poffset + 0));
  polies[5]->getVertex(1).setPt(pl.entry(poffset + 4));
  polies[5]->getVertex(2).setPt(pl.entry(poffset + 5));
  polies[5]->getVertex(3).setPt(pl.entry(poffset + 1));
  
#endif

  if (pmgroup)
  {
    pmgroup->add(polies[0]);
    pmgroup->add(polies[1]);
    pmgroup->add(polies[2]);
    pmgroup->add(polies[3]);
    pmgroup->add(polies[4]);
    pmgroup->add(polies[5]);
  }

  if (parent != 0)
  {
    reader->setParent(self->getName(), parent->getName());
  }

  return true;
}

#endif
