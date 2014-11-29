#include "InterruptHandler.h"
#include "PIC.h"
#include "Keyboard.h"
#include "Console.h"
#include "Utility.h"
#include "Task.h"
#include "Descriptor.h"
#include "AssemblyUtility.h"
#include "HardDisk.h"

void kCommonExceptionHandler( int iVectorNumber, QWORD qwErrorCode )
{
  char vcBuffer[3] = {0, };

  vcBuffer[0] = '0' + iVectorNumber / 10;
  vcBuffer[1] = '0' + iVectorNumber % 10;

  kPrintStringXY( 0, 0, "=========================================================" );
  kPrintStringXY( 0, 1, "                Exception Occur~!!!                      " );
  kPrintStringXY( 0, 2, "                   Vector:                               " );
  kPrintStringXY( 27, 2, vcBuffer );
  kPrintStringXY( 0, 3, "=========================================================" );

  while( 1 );
}

void kCommonInterruptHandler( int iVectorNumber )
{
  char vcBuffer[] = "[INT:  , ]";
  static int g_iCommonInterruptCount = 0;

  vcBuffer[5] = '0' + iVectorNumber / 10;
  vcBuffer[6] = '0' + iVectorNumber % 10;

  vcBuffer[8] = '0' + g_iCommonInterruptCount;
  g_iCommonInterruptCount = ( g_iCommonInterruptCount + 1 ) % 10;
  kPrintStringXY( 70, 0, vcBuffer );

  // EOI 전송
  kSendEOIToPIC( iVectorNumber - PIC_IRQSTARTVECTOR );
}

void kKeyboardHandler( int iVectorNumber )
{
  char vcBuffer[] = "[INT:  , ]";
  static int g_iKeyboardInterruptCount = 0;
  BYTE bTemp;

  vcBuffer[5] = '0' + iVectorNumber / 10;
  vcBuffer[6] = '0' + iVectorNumber % 10;

  vcBuffer[8] = '0' + g_iKeyboardInterruptCount;
  g_iKeyboardInterruptCount = ( g_iKeyboardInterruptCount + 1 ) % 10;
  kPrintStringXY( 0, 0, vcBuffer );

  if( kIsOutputBufferFull() == TRUE )
  {
    bTemp = kGetKeyboardScanCode();
    kConvertScanCodeAndPutQueue( bTemp );
  }

  kSendEOIToPIC( iVectorNumber - PIC_IRQSTARTVECTOR );
}

void kTimerHandler( int iVectorNumber )
{
  char vcBuffer[] = "[INT:  , ]";
  static int g_iTimerInterruptCount = 0;

  vcBuffer[5] = '0' + iVectorNumber / 10;
  vcBuffer[6] = '0' + iVectorNumber % 10;

  vcBuffer[8] = '0' + g_iTimerInterruptCount;
  g_iTimerInterruptCount = ( g_iTimerInterruptCount + 1 ) % 10;
  kPrintStringXY( 70, 0, vcBuffer );

  // EOI 전송
  kSendEOIToPIC( iVectorNumber - PIC_IRQSTARTVECTOR );

  // 타이머 발생 횟수를 증가
  g_qwTickCount++;

  // 태스크가 사용한 프로세서의 시간을 줄임
  kDecreaseProcessorTime();
  // 프로세서가 사용할 수 있는 시간을 다 썼다면 태스크 전환 수행
  if( kIsProcessorTimeExpired() == TRUE )
  {
    kScheduleInInterrupt();
  }
}

void kDeviceNotAvailableHandler( int iVectorNumber )
{
  TCB * pstFPUTask, * pstCurrentTask;
  QWORD qwLastFPUTaskID;

  char vcBuffer[] = "[EXC:  , ]";
  static int g_iFPUInterruptCount = 0;

  vcBuffer[5] = '0' + iVectorNumber / 10;
  vcBuffer[6] = '0' + iVectorNumber % 10;

  vcBuffer[8] = '0' + g_iFPUInterruptCount;
  g_iFPUInterruptCount = ( g_iFPUInterruptCount + 1 ) % 10;
  kPrintStringXY( 0, 0, vcBuffer );

  // CR0 컨트롤 레지스터의 TS 비트를 0으로 설정
  kClearTS();

  // 이전에 FPU를 사용한 태스크가 있는지 학인해 있다면 FPU의 상태를 태스크에 저장
  qwLastFPUTaskID = kGetLastFPUUsedTaskID();
  pstCurrentTask = kGetRunningTask();

  // 이전에 FPU를 사용한 것이 자신이면 아무것도 안 함
  if( qwLastFPUTaskID == pstCurrentTask->stLink.qwID )
  {
    return;
  }
  // FPU를 사용한 태스크가 있으면 FPU상태를 저장
  else if( qwLastFPUTaskID != TASK_INVALIDID )
  {
    pstFPUTask = kGetTCBInTCBPool( GETTCBOFFSET( qwLastFPUTaskID ) );
    if( ( pstFPUTask != NULL ) && ( pstFPUTask->stLink.qwID == qwLastFPUTaskID ) )
    {
      kSaveFPUContext( pstFPUTask->vqwFPUContext );
    }
  }

  // 현재 태스크가 FPU를 사용한 적이 있는 지 확인해 FPU를 사용한 적이 없다면 초기화하고,
  // 사용한 적이 있다면 저장된 FPU 콘텍스트를 복원
  if( pstCurrentTask->bFPUUsed == FALSE )
  {
    kInitializeFPU();
    pstCurrentTask->bFPUUsed = TRUE;
  }
  else
  {
    kLoadFPUContext( pstCurrentTask->vqwFPUContext );
  }

  // FPU를 사용한 태스크 ID를 현재 태스크로 변경
  kSetLastFPUUsedTaskID( pstCurrentTask->stLink.qwID );
}

void kHDDHandler( int iVectorNumber )
{
  char vcBuffer[] = "[INT:  , ]";
  static int g_iHDDInterruptCount = 0;

  vcBuffer[5] = '0' + iVectorNumber / 10;
  vcBuffer[6] = '0' + iVectorNumber % 10;

  vcBuffer[8] = '0' + g_iHDDInterruptCount;
  g_iHDDInterruptCount = ( g_iHDDInterruptCount + 1 ) % 10;
  kPrintStringXY( 10, 0, vcBuffer );

  if( iVectorNumber - PIC_IRQSTARTVECTOR == 14 )
  {
    kSetHDDInterruptFlag( TRUE, TRUE );
  }
  else
  {
    kSetHDDInterruptFlag( FALSE, TRUE );
  }

  kSendEOIToPIC( iVectorNumber - PIC_IRQSTARTVECTOR );
}
