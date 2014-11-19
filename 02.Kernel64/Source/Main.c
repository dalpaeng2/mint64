#include "Types.h"
#include "Keyboard.h"
#include "Descriptor.h"
#include "PIC.h"
#include "Console.h"
#include "ConsoleShell.h"
#include "Task.h"
#include "PIT.h"

void Main( void )
{
  int iCursorX, iCursorY;
  char vcTemp[2] = {0, };
  BYTE bFlags;
  BYTE bTemp;
  int i = 0;
  KEYDATA stData;

  kInitializeConsole( 0, 10 );
  kPrintf( "Switch To IA-32e Mode Success~!!\n" );
  kPrintf( "IA-32e C Language Kernel Start..............[Pass]\n" );
  kPrintf( "Initialize Console..........................[Pass]\n" );

  // 부팅 상황을 화면에 출력
  kGetCursor( &iCursorX, &iCursorY );
  kPrintf( "GDT Initialize And Switch For IA-32e Mode...[    ]" );
  kInitializeGDTTableAndTSS();
  kLoadGDTR( GDTR_STARTADDRESS );
  kSetCursor( 45, iCursorY++ );
  kPrintf( "Pass\n" );

  kPrintf( "TSS Segment Load............................[    ]" );
  kLoadTR( GDT_TSSSEGMENT );
  kSetCursor( 45, iCursorY++ );
  kPrintf( "Pass\n" );

  kPrintf( "IDT Initialize..............................[    ]" );
  kInitializeIDTTables();
  kLoadIDTR( IDTR_STARTADDRESS );
  kSetCursor( 45, iCursorY++ );
  kPrintf( "Pass\n" );

  kPrintf( "Total RAM Size Check........................[    ]" );
  kCheckTotalRAMSize();
  kSetCursor( 45, iCursorY++ );
  kPrintf( "Pass], Size = %d MB\n", kGetTotalRAMSize() );

  kPrintf( "TCB Pool And Scheduler Initialize...........[Pass]\n" );
  iCursorY++;
  kInitializeScheduler();
  // 1ms당 한 번씩 인터럽트가 발생하도록 설정
  kInitializePIT( MSTOCOUNT( 1 ), 1 );

  kPrintf( "Keyboard Activate And Queue Initialize......[    ]" );
  // 키보드 활성화
  if( kInitializeKeyboard() == TRUE )
  {
    kSetCursor( 45, iCursorY++ );
    kPrintf( "Pass\n" );
    kChangeKeyboardLED( FALSE, FALSE, FALSE );
  }
  else
  {
    kSetCursor( 45, iCursorY++ );
    kPrintf( "Fail\n" );
    while( 1 );
  }

  kPrintf( "PIC Controller And Interrupt Initialized....[    ]" );
  kInitializePIC();
  kMaskPICInterrupt( 0 );
  kEnableInterrupt();
  kSetCursor( 45, iCursorY++ );
  kPrintf( "Pass\n" );

  // 유휴 태스크를 생성하고 shell을 시작
  kCreateTask( TASK_FLAGS_LOWEST | TASK_FLAGS_THREAD | TASK_FLAGS_SYSTEM | TASK_FLAGS_IDLE, 0, 0, ( QWORD ) kIdleTask );
  kStartConsoleShell();
}
