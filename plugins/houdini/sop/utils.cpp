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

GA_Attribute* GetOrCreateStringAttrib(GU_Detail *gdp, const std::string &name)
{
  GA_RWAttributeRef rwRef = gdp->findGlobalAttribute(GA_SCOPE_PUBLIC, name.c_str());
  if (!rwRef.isValid())
  {
    rwRef = gdp->createStringAttribute(GA_ATTRIB_GLOBAL, GA_SCOPE_PUBLIC, name.c_str());
  }
  return rwRef.getAttribute();
}

#else

GB_Attribute* GetOrCreateStringAttrib(GU_Detail *gdp, const std::string &name)
{
  GB_Attribute *attr = gdp->attribs().find(name.c_str(), GB_ATTRIB_INDEX);
  if (!attr)
  {
    int def = 0;
    gdp->addAttrib(name.c_str(), sizeof(int), GB_ATTRIB_INDEX, (void*) &def);
    attr = gdp->attribs().find(name.c_str(), GB_ATTRIB_INDEX);
    attr->addIndex("");
  }
  return attr;
}

#endif

#if UT_MAJOR_VERSION_INT >= 12
void GetTransformsInfo(GA_Attribute *attr, TransformsInfo &info)
{
  const GA_AIFStringTuple *strTuple = attr->getAIFSharedStringTuple();
  if (!strTuple)
  {
    strTuple = attr->getAIFStringTuple();
    if (!strTuple)
    {
      return;
    }
  }
  const char *tmp = strTuple->getString(attr, 0, 0);
  std::string val = (tmp ? tmp : "");
#else
void GetTransformsInfo(GB_Attribute *attr, TransformsInfo &info)
{
  std::string val = attr->getIndex(0);
#endif
  
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

#if UT_MAJOR_VERSION_INT >= 12
void SetTransformsInfo(GA_Attribute *attr, const TransformsInfo &info)
{
  const GA_AIFStringTuple *strTuple = attr->getAIFSharedStringTuple();
  if (!strTuple)
  {
    strTuple = attr->getAIFStringTuple();
    if (!strTuple)
    {
      return;
    }
  }
#else
void SetTransformsInfo(GB_Attribute *attr, const TransformsInfo &info)
{
#endif
  TransformsInfo::const_iterator it;
  const float *fval;
  std::ostringstream oss;
  std::string val;
  
  it = info.begin();
  
  while (it != info.end())
  {
    oss << it->first << " ";
    
    fval = it->second.data();
    
    for (int i=0; i<16; ++i, ++fval)
    {
      oss << *fval << " ";
    }
    
    oss << ",";
    
    ++it;
  }
  
  val = oss.str();
  
  if (val.length() > 0 && val[val.length()-1] == ',')
  {
    val = val.substr(0, val.length()-1);
  }
  
#if UT_MAJOR_VERSION_INT >= 12
  strTuple->setString(attr, 0, val.c_str(), 0);
#else  
  attr->renameIndex(0, val.c_str());
#endif
}
  
#if UT_MAJOR_VERSION_INT >= 12
void GetParentsInfo(GA_Attribute *attr, ParentsInfo &info)
{
  const GA_AIFStringTuple *strTuple = attr->getAIFSharedStringTuple();
  if (!strTuple)
  {
    strTuple = attr->getAIFStringTuple();
    if (!strTuple)
    {
      return;
    }
  }
  const char *tmp = strTuple->getString(attr, 0, 0);
  std::string val = (tmp ? tmp : "");
#else
void GetParentsInfo(GB_Attribute *attr, ParentsInfo &info)
{
  std::string val = attr->getIndex(0);
#endif
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

#if UT_MAJOR_VERSION_INT >= 12
void SetParentsInfo(GA_Attribute *attr, const ParentsInfo &info)
{
  const GA_AIFStringTuple *strTuple = attr->getAIFSharedStringTuple();
  if (!strTuple)
  {
    strTuple = attr->getAIFStringTuple();
    if (!strTuple)
    {
      return;
    }
  }
#else
void SetParentsInfo(GB_Attribute *attr, const ParentsInfo &info)
{
#endif
  ParentsInfo::const_iterator it;
  std::ostringstream oss;
  std::string val;
  
  it = info.begin();
  
  while (it != info.end())
  {
    oss << it->first << " ";
    
    std::set<std::string>::const_iterator pit = it->second.begin();
    while (pit != it->second.end())
    {
      oss << *pit << " ";
      ++pit;
    }
    
    oss << ",";
    
    ++it;
  }
  
  val = oss.str();
  
  if (val.length() > 0 && val[val.length()-1] == ',')
  {
    val = val.substr(0, val.length()-1);
  }
  
#if UT_MAJOR_VERSION_INT >= 12
  strTuple->setString(attr, 0, val.c_str(), 0);
#else
  attr->renameIndex(0, val.c_str());
#endif
}

