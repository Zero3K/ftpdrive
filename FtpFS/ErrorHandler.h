namespace ErrorHandler
 {
 #define ERR_MODE_RETRY       0x01
 #define ERR_MODE_IGNORE      0x02
// #define ERR_MODE_DISCARD     0x04
 #define ERR_MODE_MASK_ANSW   0x3f
 #define ERR_MODE_MASK_AUTO   0x80

 #define OPCODE_OPEN          0x00
 #define OPCODE_ENUM          0x01
 #define OPCODE_READ          0x02
 #define OPCODE_WRITE         0x03
 #define OPCODE_GETINFO       0x04
 #define OPCODE_SETINFO       0x05

 #define OPCODE_MAXOPCODE     0x05

 bool CheckRetry(unsigned int OpCode, unsigned int StatusCode,DWORD pid,const std::wstring &FileName,unsigned int Pos = 0xffffffff,unsigned int BlockSize = 0xffffffff);
 void ClearAutoActions(unsigned int pid = 0xffffffff);
 }