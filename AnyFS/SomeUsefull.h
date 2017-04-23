#ifndef _SOMEUSEFULL_H_
#define _SOMEUSEFULL_H_

struct string_less_no_case
 {
 public:
  inline bool operator()(const std::string &a, const std::string &b) const
   {
   return stricmp(a.c_str(),b.c_str()) <0;
   }
 };

struct wstring_less_no_case
 {
 public:
  inline bool operator()(const std::wstring &a, const std::wstring &b) const
   {
   return wcsicmp(a.c_str(),b.c_str()) <0;
   }
 };

#endif //_SOMEUSEFULL_H_