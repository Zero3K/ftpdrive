#pragma once
class ResHolder
 {
 private:
  void LoadModule(const TCHAR *module);
 public:
  ResHolder();
  ~ResHolder();
  std::wstring GetString(UINT uID);
  std::wstring GetString(UINT uID,unsigned int i);
  std::wstring GetString(UINT uID,const std::wstring &s);
  HMODULE Module;
 };

