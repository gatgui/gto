#include "utils.h"
#include <sstream>

#ifndef round
template <typename T>
inline T round(const T &v)
{
   return T( floor( double(v) + 0.5 ) );
}
#endif

void Strip(std::string &s)
{
  size_t p0, p1;
  
  p0 = s.find_first_not_of(" \n\t\v");
  if (p0 != std::string::npos)
  {
    p1 = s.find_last_not_of(" \n\t\v");
    s = s.substr(p0, p1-p0+1);
  }
}

size_t Split(const std::string &val, char delim, std::vector<std::string> &vals, bool allowEmpty, bool stripVals)
{
  std::string sval;
  
  size_t p0 = 0;
  size_t p1 = val.find(delim, p0);
  
  vals.clear();
  
  while (p1 != std::string::npos)
  {
    sval = val.substr(p0, p1-p0);
    
    if (stripVals)
    {
      Strip(sval);
    }
    
    if (allowEmpty || sval.length() > 0)
    {
      vals.push_back(sval);
    }
    
    p0 = p1 + 1;
    p1 = val.find(delim, p0);
  }
  
  sval = val.substr(p0);
  
  if (stripVals)
  {
    Strip(sval);
  }
  
  if (allowEmpty || sval.length() > 0)
  {
    vals.push_back(sval);
  }
  
  return vals.size();
}

std::string GetTransformsInfoKey(float frame)
{
  std::ostringstream oss;
  
  int fframe = int(floor(frame));
  int sframe = int(round((frame - fframe) * 1000.0f));
  
  oss << "transforms_" << fframe << "_" << sframe;
  
  return oss.str();
}

std::string GetParentsInfoKey()
{
  return "parenting";
}

#if UT_MAJOR_VERSION_INT >= 12

const GA_Attribute* GetStringAttrib(const GU_Detail *gdp, const std::string &name)
{
  GA_ROAttributeRef h = gdp->findGlobalAttribute(GA_SCOPE_PUBLIC, name.c_str());
  return (h.isValid() && h.isString() && h.getTupleSize() == 1 ? h.getAttribute(): 0);
}

void GetTransformsInfo(const GA_Attribute *attr, TransformsInfo &info)
{
  const GA_AIFStringTuple *aifStr = attr->getAIFSharedStringTuple();
  if (!aifStr)
  {
    aifStr = attr->getAIFStringTuple();
    if (!aifStr)
    {
      return;
    }
  }
  
  const char *tmp = aifStr->getString(attr, 0, 0);
  std::string val = (tmp ? tmp : "");
  
  info.clear();
  
  std::vector<std::string> items;
  std::vector<std::string> item;
  UT_Matrix4 mtx;
  bool invalid;
  float *fval;
  
  Split(val, ',', items);
  
  for (size_t i=0; i<items.size(); ++i)
  {
    if (Split(items[i], ' ', item) == 17) // name + 16 floats
    {
      invalid = false;
      
      fval = mtx.data();
      
      for (size_t j=1; j<17; ++j, ++fval)
      {
        if (sscanf(item[j].c_str(), "%f", fval) != 1)
        {
          invalid = true;
          break;
        }
      }
      
      if (!invalid)
      {
        info[item[0]] = mtx;
      }
    }
  }
}

void GetParentsInfo(const GA_Attribute *attr, ParentsInfo &info)
{
  const GA_AIFStringTuple *aifStr = attr->getAIFSharedStringTuple();
  if (!aifStr)
  {
    aifStr = attr->getAIFStringTuple();
    if (!aifStr)
    {
      return;
    }
  }
  
  const char *tmp = aifStr->getString(attr, 0, 0); // detail attributes offset are always 0?
  std::string val = (tmp ? tmp : "");
  
  info.clear();
  
  std::vector<std::string> items;
  std::vector<std::string> item;
  
  Split(val, ',', items);
  
  for (size_t i=0; i<items.size(); ++i)
  {
    if (Split(items[i], ' ', item) >= 2) // at least 2 names (child parent0 ... parentN)
    {
      for (size_t j=1; j<item.size(); ++j)
      {
        info[item[0]].insert(item[j]);
      }
    }
  }
}


#else

const GB_Attribute* GetStringAttrib(const GU_Detail *gdp, const std::string &name)
{
  return gdp->attribs().find(name.c_str(), GB_ATTRIB_INDEX);
}

void GetTransformsInfo(const GB_Attribute *attr, TransformsInfo &info)
{
  std::string val = attr->getIndex(0);
  
  info.clear();
  
  std::vector<std::string> items;
  std::vector<std::string> item;
  UT_Matrix4 mtx;
  bool invalid;
  float *fval;
  
  Split(val, ',', items);
  
  for (size_t i=0; i<items.size(); ++i)
  {
    if (Split(items[i], ' ', item) == 17) // name + 16 floats
    {
      invalid = false;
      
      fval = mtx.data();
      
      for (size_t j=1; j<17; ++j, ++fval)
      {
        if (sscanf(item[j].c_str(), "%f", fval) != 1)
        {
          invalid = true;
          break;
        }
      }
      
      if (!invalid)
      {
        info[item[0]] = mtx;
      }
    }
  }
}

void GetParentsInfo(const GB_Attribute *attr, ParentsInfo &info)
{
  std::string val = attr->getIndex(0);
  
  info.clear();
  
  std::vector<std::string> items;
  std::vector<std::string> item;
  
  Split(val, ',', items);
  
  for (size_t i=0; i<items.size(); ++i)
  {
    if (Split(items[i], ' ', item) >= 2) // at least 2 names (child parent0 ... parentN)
    {
      for (size_t j=1; j<item.size(); ++j)
      {
        info[item[0]].insert(item[j]);
      }
    }
  }
}

#endif

