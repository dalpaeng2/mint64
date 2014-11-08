#ifndef __UTILITY_H__
#define __UTILITY_H__

#include "Types.h"

void kReadCPUID( DWORD dwEAX, DWORD * pdwEAX, DWORD * pdwEBX, DWORD * pdwECX, DWORD * pdwEDX );
void kSwitchAndExecute64bitKernal( void );

#endif /* __UTILITY_H__ */
