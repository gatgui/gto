#ifndef __SOP_GtoImport_utils_h__
#define __SOP_GtoImport_utils_h__

#include "object.h"

typedef std::map<std::string, std::set<std::string> > ParentsInfo;
typedef std::map<std::string, UT_Matrix4> TransformsInfo;

void Strip(std::string &s);
size_t Split(const std::string &val, char delim, std::vector<std::string> &vals, bool allowEmpty=false, bool stripVals=true);

std::string GetTransformsInfoKey(float frame);
std::string GetParentsInfoKey();

#if UT_MAJOR_VERSION_INT >= 12

GA_Attribute* GetOrCreateStringAttrib(GU_Detail *gdp, const std::string &name);

void GetTransformsInfo(GA_Attribute *attr, TransformsInfo &info);
void SetTransformsInfo(GA_Attribute *attr, const TransformsInfo &info);

void GetParentsInfo(GA_Attribute *attr, ParentsInfo &info);
void SetParentsInfo(GA_Attribute *attr, const ParentsInfo &info);

#else

GB_Attribute* GetOrCreateStringAttrib(GU_Detail *gdp, const std::string &name);

void GetTransformsInfo(GB_Attribute *attr, TransformsInfo &info);
void SetTransformsInfo(GB_Attribute *attr, const TransformsInfo &info);

void GetParentsInfo(GB_Attribute *attr, ParentsInfo &info);
void SetParentsInfo(GB_Attribute *attr, const ParentsInfo &info);

#endif

#endif
