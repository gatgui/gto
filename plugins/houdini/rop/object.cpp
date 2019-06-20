#include "object.h"

#if UT_MAJOR_VERSION_INT >= 12
const std::vector<GA_Offset> Object::NoPrimitives;
#else
const std::vector<unsigned int> Object::NoPrimitives;
#endif

Object::Object()
{
}

Object::~Object()
{
}

#if UT_MAJOR_VERSION_INT >= 12
bool Object::declare(HouGtoWriter *writer, GU_Detail *gdp, const std::vector<GA_Offset> &prims)
#else
bool Object::declare(HouGtoWriter *writer, GU_Detail *gdp, const std::vector<unsigned int> &prims)
#endif
{
  return false;
}

void Object::write(HouGtoWriter *writer)
{
}

void Object::invalidate()
{
}

void Object::update()
{
}

void Object::mergeBounds(const UT_Matrix4 &space, UT_BoundingBox &box)
{
}

bool Object::isValid()
{
  return true;
}
