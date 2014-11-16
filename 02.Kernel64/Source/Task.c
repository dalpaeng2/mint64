#include "Task.h"
#include "Descriptor.h"

// 스케쥴러 관련 자료구조
static SCHEDULER gs_stScheduler;
static TCBPOOLMANAGER gs_stTCBPoolManager;

////////////////////////////////////////////////////////////////////////////////
// 태스크 풀과 태스크 관련
////////////////////////////////////////////////////////////////////////////////
void kInitializeTCBPool( void )
{
  int i;
  kMemSet( &(gs_stTCBPoolManager ), 0, sizeof( gs_stTCBPoolManager ) );

  gs_stTCBPoolManager.pstStartAddress = ( TCB * ) TASK_TCBPOOLADDRESS;
  kMemSet( TASK_TCBPOOLADDRESS, 0, sizeof( TCB ) * TASK_MAXCOUNT );

  for( i = 0; i < TASK_MAXCOUNT; i++ )
  {
    gs_stTCBPoolManager.pstStartAddress[i].stLink.qwID = i;
  }

  gs_stTCBPoolManager.iMaxCount = TASK_MAXCOUNT;
  gs_stTCBPoolManager.iAllocatedCount = 1;
}

TCB * kAllocateTCB( void )
{
  TCB * pstEmptyTCB;
  int i;

  if( gs_stTCBPoolManager.iUseCount == gs_stTCBPoolManager.iMaxCount )
  {
    return NULL;
  }

  for( i = 0; i < gs_stTCBPoolManager.iMaxCount; i++ )
  {
    if( ( gs_stTCBPoolManager.pstStartAddress[i].stLink.qwID >> 32 ) == 0 )
    {
      pstEmptyTCB = &( gs_stTCBPoolManager.pstStartAddress[i] );
      break;
    }
  }

  // 상위 32비트를 0이 아닌 값으로 설정해서 할당된 TCB로 설정
  pstEmptyTCB->stLink.qwID = ( ( QWORD ) gs_stTCBPoolManager.iAllocatedCount << 32 ) | i;
  gs_stTCBPoolManager.iUseCount++;
  gs_stTCBPoolManager.iAllocatedCount++;
  if( gs_stTCBPoolManager.iAllocatedCount == 0 )
  {
    gs_stTCBPoolManager.iAllocatedCount = 1;
  }
  return pstEmptyTCB;
}

void kFreeTCB( QWORD qwID )
{
  int i;

  // 태스크 ID의 하위 32비트가 인텍스 열할을 함
  i = qwID & 0xFFFFFFFF;

  kMemSet( &( gs_stTCBPoolManager.pstStartAddress[i].stContext ), 0, sizeof( CONTEXT ) );
  gs_stTCBPoolManager.pstStartAddress[i].stLink.qwID = i;

  gs_stTCBPoolManager.iUseCount--;
}

TCB * kCreateTask( QWORD qwFlags, QWORD qwEntryPointAddress )
{
  TCB * pstTask;
  void * pvStackAddress;

  pstTask = kAllocateTCB();
  if( pstTask == NULL )
  {
    return NULL;
  }

  // 태스크 ID로 스택 어드레스 계산, 하위 32비트가 스택 풀의 오프셋 역할 수행
  pvStackAddress = ( void * ) ( TASK_STACKPOOLADDRESS +
                    ( TASK_STACKSIZE * ( pstTask->stLink.qwID & 0xFFFFFFFF ) ) );

  // TCB를 설정한 후 준비 리스트에 삽입하여 스케줄링될 수 있도록 함
  kSetUpTask( pstTask, qwFlags, qwEntryPointAddress, pvStackAddress,
              TASK_STACKSIZE );
  kAddTaskToReadyList( pstTask );

  return pstTask;
}

void kSetUpTask( TCB * pstTCB, QWORD qwFlags, QWORD qwEntryPointAddress,
                  void * pvStackAddress, QWORD qwStackSize )
{
  kMemSet( pstTCB->stContext.vqRegister, 0, sizeof( pstTCB->stContext.vqRegister ) );

  pstTCB->stContext.vqRegister[ TASK_RSPOFFSET ] = ( QWORD ) pvStackAddress + qwStackSize;
  pstTCB->stContext.vqRegister[ TASK_RBPOFFSET ] = ( QWORD ) pvStackAddress + qwStackSize;

  pstTCB->stContext.vqRegister[ TASK_CSOFFSET ] = GDT_KERNELCODESEGMENT;
  pstTCB->stContext.vqRegister[ TASK_DSOFFSET ] = GDT_KERNELDATASEGMENT;
  pstTCB->stContext.vqRegister[ TASK_ESOFFSET ] = GDT_KERNELDATASEGMENT;
  pstTCB->stContext.vqRegister[ TASK_FSOFFSET ] = GDT_KERNELDATASEGMENT;
  pstTCB->stContext.vqRegister[ TASK_GSOFFSET ] = GDT_KERNELDATASEGMENT;
  pstTCB->stContext.vqRegister[ TASK_SSOFFSET ] = GDT_KERNELDATASEGMENT;

  pstTCB->stContext.vqRegister[ TASK_RIPOFFSET ] = qwEntryPointAddress;

  pstTCB->stContext.vqRegister[ TASK_RFLAGSOFFSET ] |= 0x0200;

  pstTCB->pvStackAddress = pvStackAddress;
  pstTCB->qwStackSize = qwStackSize;
  pstTCB->qwFlags = qwFlags;
}

////////////////////////////////////////////////////////////////////////////////
// 스케줄러 관련
////////////////////////////////////////////////////////////////////////////////
void kInitializeScheduler( void )
{
  kInitializeTCBPool();

  kInitializeList( &( gs_stScheduler.stReadyList ) );

  // TCB를 할당받아 실행 중인 태스크로 설정하여, 부팅을 수행한 태스크를 저장할 TCB를 준비
  gs_stScheduler.pstRunningTask = kAllocateTCB();
}

void kSetRunningTask( TCB * pstTask )
{
  gs_stScheduler.pstRunningTask = pstTask;
}

TCB * kGetRunningTask( void )
{
  return gs_stScheduler.pstRunningTask;
}

TCB * kGetNextTaskToRun( void )
{
  if( kGetListCount( &( gs_stScheduler.stReadyList ) ) == 0 )
  {
    return NULL;
  }

  return ( TCB * ) kRemoveListFromHeader( &( gs_stScheduler.stReadyList ) );
}

void kAddTaskToReadyList( TCB * pstTask )
{
  kAddListToTail( &( gs_stScheduler.stReadyList ), pstTask );
}


// 다른 태스크를 찾아서 전환
//   인터럽트나 예외가 발생했을 때 호출하면 안됨
void kSchedule( void )
{
  TCB * pstRunningTask, * pstNextTask;
  BOOL bPreviousFlag;

  if( kGetListCount( &( gs_stScheduler.stReadyList ) ) == 0 )
  {
    return ;
  }

  // 태스크 전환 중 인터럽트가 발생하지 않도록 함
  bPreviousFlag = kSetInterruptFlag( FALSE );

  pstNextTask = kGetNextTaskToRun();
  if( pstNextTask == NULL )
  {
    kSetInterruptFlag( bPreviousFlag );
    return;
  }

  pstRunningTask = gs_stScheduler.pstRunningTask;
  kAddTaskToReadyList( pstRunningTask );

  // 다음 태스크를 현재 수행 중인 태스크로 설정한 후 콘텍스트 전환
  gs_stScheduler.pstRunningTask = pstNextTask;
  kSwitchContext( &( pstRunningTask->stContext ), &( pstNextTask->stContext ) );

  gs_stScheduler.iProcessorTime = TASK_PROCESSORTIME;

  kSetInterruptFlag( bPreviousFlag );
}

// 인터럽트가 발생했을 때 다른 태스크를 찾아 전환
//   반드시 인터럽트나 예외가 발생했을 때 호출해야 함
BOOL kScheduleInInterrupt( void )
{
  TCB * pstRunningTask, * pstNextTask;
  char * pcContextAddress;

  pstNextTask = kGetNextTaskToRun();
  if( pstNextTask == NULL )
  {
    return FALSE;
  }

  //////////////////////////////////////////////////////////////////////////////
  // 태스크 전환 처리
  //   인터럽트 핸들러에서 저장한 콘텍스트를 다른 콘텍스트로 덮어쓰는 방법으로 처리
  //////////////////////////////////////////////////////////////////////////////
  pcContextAddress = ( char * ) IST_STARTADDRESS + IST_SIZE - sizeof( CONTEXT );

  // 현재 태스크를 얻어서 IST에 있는 콘텍스트를 복사하고, 현재 태스크를 준비 리스트로 옮김
  pstRunningTask = gs_stScheduler.pstRunningTask;
  kMemCpy( &( pstRunningTask->stContext ), pcContextAddress, sizeof( CONTEXT ) );
  kAddTaskToReadyList( pstRunningTask );

  // 전환해서 실행할 태스크를 Running Task로 설정하고 콘텍스트를 IST에 복사해서
  //   자동으로 태스크 전환이 일어나도록 함
  gs_stScheduler.pstRunningTask = pstNextTask;
  kMemCpy( pcContextAddress, &( pstNextTask->stContext ), sizeof( CONTEXT ) );

  gs_stScheduler.iProcessorTime = TASK_PROCESSORTIME;
  return TRUE;
}

void kDecreaseProcessorTime( void )
{
  if( gs_stScheduler.iProcessorTime > 0 )
  {
    gs_stScheduler.iProcessorTime--;
  }
}

BOOL kIsProcessorTimeExpired( void )
{
  if( gs_stScheduler.iProcessorTime <= 0 )
  {
    return TRUE;
  }
  return FALSE;
}
