/*
 * Copyright 2004-2005 Ivan Leo Puoti
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _WDMDDK_
#define _WDMDDK_
#define _NTDDK_

#include <ntstatus.h>

#ifdef _WIN64
#define POINTER_ALIGNMENT DECLSPEC_ALIGN(8)
#else
#define POINTER_ALIGNMENT
#endif

typedef ULONG_PTR KSPIN_LOCK, *PKSPIN_LOCK;

struct _KDPC;
struct _KAPC;

typedef VOID (WINAPI *PKDEFERRED_ROUTINE)(struct _KDPC *, PVOID, PVOID, PVOID);

typedef struct _DISPATCHER_HEADER {
  UCHAR  Type;
  UCHAR  Absolute;
  UCHAR  Size;
  UCHAR  Inserted;
  LONG  SignalState;
  LIST_ENTRY  WaitListHead;
} DISPATCHER_HEADER, *PDISPATCHER_HEADER;

typedef struct _KEVENT {
  DISPATCHER_HEADER  Header;
} KEVENT, *PKEVENT, *RESTRICTED_POINTER PRKEVENT;

typedef struct _KDPC {
  CSHORT  Type;
  UCHAR  Number;
  UCHAR  Importance;
  LIST_ENTRY  DpcListEntry;
  PKDEFERRED_ROUTINE  DeferredRoutine;
  PVOID  DeferredContext;
  PVOID  SystemArgument1;
  PVOID  SystemArgument2;
  PULONG_PTR  Lock;
} KDPC, *PKDPC, *RESTRICTED_POINTER PRKDPC;

typedef struct _KDEVICE_QUEUE_ENTRY {
  LIST_ENTRY  DeviceListEntry;
  ULONG  SortKey;
  BOOLEAN  Inserted;
} KDEVICE_QUEUE_ENTRY, *PKDEVICE_QUEUE_ENTRY,
*RESTRICTED_POINTER PRKDEVICE_QUEUE_ENTRY;

typedef struct _KDEVICE_QUEUE {
  CSHORT  Type;
  CSHORT  Size;
  LIST_ENTRY  DeviceListHead;
  KSPIN_LOCK  Lock;
  BOOLEAN  Busy;
} KDEVICE_QUEUE, *PKDEVICE_QUEUE, *RESTRICTED_POINTER PRKDEVICE_QUEUE;

typedef struct _IO_TIMER *PIO_TIMER;
typedef struct _ETHREAD *PETHREAD;

#define MAXIMUM_VOLUME_LABEL_LENGTH       (32 * sizeof(WCHAR))

typedef struct _VPB {
  CSHORT  Type;
  CSHORT  Size;
  USHORT  Flags;
  USHORT  VolumeLabelLength;
  struct _DEVICE_OBJECT  *DeviceObject;
  struct _DEVICE_OBJECT  *RealDevice;
  ULONG  SerialNumber;
  ULONG  ReferenceCount;
  WCHAR  VolumeLabel[MAXIMUM_VOLUME_LABEL_LENGTH / sizeof(WCHAR)];
} VPB, *PVPB;


typedef struct _WAIT_CONTEXT_BLOCK {
  KDEVICE_QUEUE_ENTRY  WaitQueueEntry;
  struct _DRIVER_CONTROL  *DeviceRoutine;
  PVOID  DeviceContext;
  ULONG  NumberOfMapRegisters;
  PVOID  DeviceObject;
  PVOID  CurrentIrp;
  PKDPC  BufferChainingDpc;
} WAIT_CONTEXT_BLOCK, *PWAIT_CONTEXT_BLOCK;

#ifndef DEVICE_TYPE
#define DEVICE_TYPE ULONG
#endif
#define IRP_MJ_MAXIMUM_FUNCTION           0x1b
#define IRP_MJ_DEVICE_CONTROL             0x0e

typedef struct _DEVICE_OBJECT {
  CSHORT  Type;
  USHORT  Size;
  LONG  ReferenceCount;
  struct _DRIVER_OBJECT  *DriverObject;
  struct _DEVICE_OBJECT  *NextDevice;
  struct _DEVICE_OBJECT  *AttachedDevice;
  struct _IRP  *CurrentIrp;
  PIO_TIMER  Timer;
  ULONG  Flags;
  ULONG  Characteristics;
  PVPB  Vpb;
  PVOID  DeviceExtension;
  DEVICE_TYPE  DeviceType;
  CCHAR  StackSize;
  union {
    LIST_ENTRY  ListEntry;
    WAIT_CONTEXT_BLOCK  Wcb;
  } Queue;
  ULONG  AlignmentRequirement;
  KDEVICE_QUEUE  DeviceQueue;
  KDPC  Dpc;
  ULONG  ActiveThreadCount;
  PSECURITY_DESCRIPTOR  SecurityDescriptor;
  KEVENT  DeviceLock;
  USHORT  SectorSize;
  USHORT  Spare1;
  struct _DEVOBJ_EXTENSION  *DeviceObjectExtension;
  PVOID  Reserved;
} DEVICE_OBJECT;
typedef struct _DEVICE_OBJECT *PDEVICE_OBJECT;

typedef struct _DRIVER_EXTENSION {
  struct _DRIVER_OBJECT  *DriverObject;
  PVOID  AddDevice;
  ULONG  Count;
  UNICODE_STRING  ServiceKeyName;
} DRIVER_EXTENSION, *PDRIVER_EXTENSION;

typedef struct _DRIVER_OBJECT {
  CSHORT  Type;
  CSHORT  Size;
  PDEVICE_OBJECT  DeviceObject;
  ULONG  Flags;
  PVOID  DriverStart;
  ULONG  DriverSize;
  PVOID  DriverSection;
  PDRIVER_EXTENSION  DriverExtension;
  UNICODE_STRING  DriverName;
  PUNICODE_STRING  HardwareDatabase;
  PVOID  FastIoDispatch;
  PVOID  DriverInit;
  PVOID  DriverStartIo;
  PVOID  DriverUnload;
  PVOID  MajorFunction[IRP_MJ_MAXIMUM_FUNCTION + 1];
} DRIVER_OBJECT;
typedef struct _DRIVER_OBJECT *PDRIVER_OBJECT;

/* Irp definitions */
typedef UCHAR KIRQL, *PKIRQL;
typedef CCHAR KPROCESSOR_MODE;

typedef VOID (WINAPI *PDRIVER_CANCEL)(
  IN struct _DEVICE_OBJECT  *DeviceObject,
  IN struct _IRP  *Irp);

typedef VOID (WINAPI *PKNORMAL_ROUTINE)(
  IN PVOID  NormalContext,
  IN PVOID  SystemArgument1,
  IN PVOID  SystemArgument2);

typedef VOID (WINAPI *PKKERNEL_ROUTINE)(
  IN struct _KAPC  *Apc,
  IN OUT PKNORMAL_ROUTINE  *NormalRoutine,
  IN OUT PVOID  *NormalContext,
  IN OUT PVOID  *SystemArgument1,
  IN OUT PVOID  *SystemArgument2);

typedef VOID (WINAPI *PKRUNDOWN_ROUTINE)(
  IN struct _KAPC  *Apc);

typedef struct _KAPC {
  CSHORT  Type;
  CSHORT  Size;
  ULONG  Spare0;
  struct _KTHREAD  *Thread;
  LIST_ENTRY  ApcListEntry;
  PKKERNEL_ROUTINE  KernelRoutine;
  PKRUNDOWN_ROUTINE  RundownRoutine;
  PKNORMAL_ROUTINE  NormalRoutine;
  PVOID  NormalContext;
  PVOID  SystemArgument1;
  PVOID  SystemArgument2;
  CCHAR  ApcStateIndex;
  KPROCESSOR_MODE  ApcMode;
  BOOLEAN  Inserted;
} KAPC, *PKAPC, *RESTRICTED_POINTER PRKAPC;

#include <pshpack1.h>
typedef struct _IRP {
  CSHORT  Type;
  USHORT  Size;
  struct _MDL  *MdlAddress;
  ULONG  Flags;
  union {
    struct _IRP  *MasterIrp;
    LONG  IrpCount;
    PVOID  SystemBuffer;
  } AssociatedIrp;
  LIST_ENTRY  ThreadListEntry;
  IO_STATUS_BLOCK  IoStatus;
  KPROCESSOR_MODE  RequestorMode;
  BOOLEAN  PendingReturned;
  CHAR  StackCount;
  CHAR  CurrentLocation;
  BOOLEAN  Cancel;
  KIRQL  CancelIrql;
  CCHAR  ApcEnvironment;
  UCHAR  AllocationFlags;
  PIO_STATUS_BLOCK  UserIosb;
  PKEVENT  UserEvent;
  union {
    struct {
      PIO_APC_ROUTINE  UserApcRoutine;
      PVOID  UserApcContext;
    } AsynchronousParameters;
    LARGE_INTEGER  AllocationSize;
  } Overlay;
  PDRIVER_CANCEL  CancelRoutine;
  PVOID  UserBuffer;
  union {
    struct {
       union {
        KDEVICE_QUEUE_ENTRY  DeviceQueueEntry;
        struct {
          PVOID  DriverContext[4];
        } DUMMYSTRUCTNAME;
      } DUMMYUNIONNAME;
      PETHREAD  Thread;
      PCHAR  AuxiliaryBuffer;
      struct {
        LIST_ENTRY  ListEntry;
        union {
          struct _IO_STACK_LOCATION  *CurrentStackLocation;
          ULONG  PacketType;
        } DUMMYUNIONNAME;
      } DUMMYSTRUCTNAME;
      struct _FILE_OBJECT  *OriginalFileObject;
    } Overlay;
    KAPC  Apc;
    PVOID  CompletionKey;
  } Tail;
} IRP;
typedef struct _IRP *PIRP;
#include <poppack.h>

/* MDL definitions */

typedef VOID (WINAPI *PINTERFACE_REFERENCE)(
  PVOID  Context);

typedef VOID (WINAPI *PINTERFACE_DEREFERENCE)(
  PVOID Context);

typedef struct _INTERFACE {
  USHORT  Size;
  USHORT  Version;
  PVOID  Context;
  PINTERFACE_REFERENCE  InterfaceReference;
  PINTERFACE_DEREFERENCE  InterfaceDereference;
} INTERFACE, *PINTERFACE;

typedef struct _SECTION_OBJECT_POINTERS {
  PVOID  DataSectionObject;
  PVOID  SharedCacheMap;
  PVOID  ImageSectionObject;
} SECTION_OBJECT_POINTERS, *PSECTION_OBJECT_POINTERS;

typedef struct _IO_COMPLETION_CONTEXT {
  PVOID  Port;
  PVOID  Key;
} IO_COMPLETION_CONTEXT, *PIO_COMPLETION_CONTEXT;

typedef enum _DEVICE_RELATION_TYPE {
  BusRelations,
  EjectionRelations,
  PowerRelations,
  RemovalRelations,
  TargetDeviceRelation,
  SingleBusRelations
} DEVICE_RELATION_TYPE, *PDEVICE_RELATION_TYPE;

typedef struct _FILE_OBJECT {
  CSHORT  Type;
  CSHORT  Size;
  PDEVICE_OBJECT  DeviceObject;
  PVPB  Vpb;
  PVOID  FsContext;
  PVOID  FsContext2;
  PSECTION_OBJECT_POINTERS  SectionObjectPointer;
  PVOID  PrivateCacheMap;
  NTSTATUS  FinalStatus;
  struct _FILE_OBJECT  *RelatedFileObject;
  BOOLEAN  LockOperation;
  BOOLEAN  DeletePending;
  BOOLEAN  ReadAccess;
  BOOLEAN  WriteAccess;
  BOOLEAN  DeleteAccess;
  BOOLEAN  SharedRead;
  BOOLEAN  SharedWrite;
  BOOLEAN  SharedDelete;
  ULONG  Flags;
  UNICODE_STRING  FileName;
  LARGE_INTEGER  CurrentByteOffset;
  ULONG  Waiters;
  ULONG  Busy;
  PVOID  LastLock;
  KEVENT  Lock;
  KEVENT  Event;
  PIO_COMPLETION_CONTEXT  CompletionContext;
} FILE_OBJECT;
typedef struct _FILE_OBJECT *PFILE_OBJECT;

#define INITIAL_PRIVILEGE_COUNT           3

typedef struct _INITIAL_PRIVILEGE_SET {
  ULONG  PrivilegeCount;
  ULONG  Control;
  LUID_AND_ATTRIBUTES  Privilege[INITIAL_PRIVILEGE_COUNT];
} INITIAL_PRIVILEGE_SET, * PINITIAL_PRIVILEGE_SET;

typedef struct _SECURITY_SUBJECT_CONTEXT {
  PACCESS_TOKEN  ClientToken;
  SECURITY_IMPERSONATION_LEVEL  ImpersonationLevel;
  PACCESS_TOKEN  PrimaryToken;
  PVOID  ProcessAuditId;
} SECURITY_SUBJECT_CONTEXT, *PSECURITY_SUBJECT_CONTEXT;

typedef struct _ACCESS_STATE {
  LUID  OperationID;
  BOOLEAN  SecurityEvaluated;
  BOOLEAN  GenerateAudit;
  BOOLEAN  GenerateOnClose;
  BOOLEAN  PrivilegesAllocated;
  ULONG  Flags;
  ACCESS_MASK  RemainingDesiredAccess;
  ACCESS_MASK  PreviouslyGrantedAccess;
  ACCESS_MASK  OriginalDesiredAccess;
  SECURITY_SUBJECT_CONTEXT  SubjectSecurityContext;
  PSECURITY_DESCRIPTOR  SecurityDescriptor;
  PVOID  AuxData;
  union {
    INITIAL_PRIVILEGE_SET  InitialPrivilegeSet;
    PRIVILEGE_SET  PrivilegeSet;
  } Privileges;

  BOOLEAN  AuditPrivileges;
  UNICODE_STRING  ObjectName;
  UNICODE_STRING  ObjectTypeName;
} ACCESS_STATE, *PACCESS_STATE;

typedef struct _IO_SECURITY_CONTEXT {
  PSECURITY_QUALITY_OF_SERVICE  SecurityQos;
  PACCESS_STATE  AccessState;
  ACCESS_MASK  DesiredAccess;
  ULONG  FullCreateOptions;
} IO_SECURITY_CONTEXT, *PIO_SECURITY_CONTEXT;

typedef struct _DEVICE_CAPABILITIES {
  USHORT  Size;
  USHORT  Version;
  ULONG  DeviceD1 : 1;
  ULONG  DeviceD2 : 1;
  ULONG  LockSupported : 1;
  ULONG  EjectSupported : 1;
  ULONG  Removable : 1;
  ULONG  DockDevice : 1;
  ULONG  UniqueID : 1;
  ULONG  SilentInstall : 1;
  ULONG  RawDeviceOK : 1;
  ULONG  SurpriseRemovalOK : 1;
  ULONG  WakeFromD0 : 1;
  ULONG  WakeFromD1 : 1;
  ULONG  WakeFromD2 : 1;
  ULONG  WakeFromD3 : 1;
  ULONG  HardwareDisabled : 1;
  ULONG  NonDynamic : 1;
  ULONG  WarmEjectSupported : 1;
  ULONG  NoDisplayInUI : 1;
  ULONG  Reserved : 14;
  ULONG  Address;
  ULONG  UINumber;
  DEVICE_POWER_STATE  DeviceState[PowerSystemMaximum];
  SYSTEM_POWER_STATE  SystemWake;
  DEVICE_POWER_STATE  DeviceWake;
  ULONG  D1Latency;
  ULONG  D2Latency;
  ULONG  D3Latency;
} DEVICE_CAPABILITIES, *PDEVICE_CAPABILITIES;

typedef enum _INTERFACE_TYPE {
  InterfaceTypeUndefined = -1,
  Internal,
  Isa,
  Eisa,
  MicroChannel,
  TurboChannel,
  PCIBus,
  VMEBus,
  NuBus,
  PCMCIABus,
  CBus,
  MPIBus,
  MPSABus,
  ProcessorInternal,
  InternalPowerBus,
  PNPISABus,
  PNPBus,
  MaximumInterfaceType
} INTERFACE_TYPE, *PINTERFACE_TYPE;

typedef LARGE_INTEGER PHYSICAL_ADDRESS, *PPHYSICAL_ADDRESS;

#define IO_RESOURCE_PREFERRED             0x01
#define IO_RESOURCE_DEFAULT               0x02
#define IO_RESOURCE_ALTERNATIVE           0x08

typedef struct _IO_RESOURCE_DESCRIPTOR {
  UCHAR  Option;
  UCHAR  Type;
  UCHAR  ShareDisposition;
  UCHAR  Spare1;
  USHORT  Flags;
  USHORT  Spare2;
  union {
    struct {
      ULONG  Length;
      ULONG  Alignment;
      PHYSICAL_ADDRESS  MinimumAddress;
      PHYSICAL_ADDRESS  MaximumAddress;
    } Port;
    struct {
      ULONG  Length;
      ULONG  Alignment;
      PHYSICAL_ADDRESS  MinimumAddress;
      PHYSICAL_ADDRESS  MaximumAddress;
    } Memory;
    struct {
      ULONG  MinimumVector;
      ULONG  MaximumVector;
    } Interrupt;
    struct {
      ULONG  MinimumChannel;
      ULONG  MaximumChannel;
    } Dma;
    struct {
      ULONG  Length;
      ULONG  Alignment;
      PHYSICAL_ADDRESS  MinimumAddress;
      PHYSICAL_ADDRESS  MaximumAddress;
    } Generic;
    struct {
      ULONG  Data[3];
    } DevicePrivate;
    struct {
      ULONG  Length;
      ULONG  MinBusNumber;
      ULONG  MaxBusNumber;
      ULONG  Reserved;
    } BusNumber;
    struct {
      ULONG  Priority;
      ULONG  Reserved1;
      ULONG  Reserved2;
    } ConfigData;
  } u;
} IO_RESOURCE_DESCRIPTOR, *PIO_RESOURCE_DESCRIPTOR;

typedef struct _IO_RESOURCE_LIST {
  USHORT  Version;
  USHORT  Revision;
  ULONG  Count;
  IO_RESOURCE_DESCRIPTOR  Descriptors[1];
} IO_RESOURCE_LIST, *PIO_RESOURCE_LIST;

typedef struct _IO_RESOURCE_REQUIREMENTS_LIST {
  ULONG  ListSize;
  INTERFACE_TYPE  InterfaceType;
  ULONG  BusNumber;
  ULONG  SlotNumber;
  ULONG  Reserved[3];
  ULONG  AlternativeLists;
  IO_RESOURCE_LIST  List[1];
} IO_RESOURCE_REQUIREMENTS_LIST, *PIO_RESOURCE_REQUIREMENTS_LIST;

typedef enum _BUS_QUERY_ID_TYPE {
  BusQueryDeviceID,
  BusQueryHardwareIDs,
  BusQueryCompatibleIDs,
  BusQueryInstanceID,
  BusQueryDeviceSerialNumber
} BUS_QUERY_ID_TYPE, *PBUS_QUERY_ID_TYPE;

typedef enum _DEVICE_TEXT_TYPE {
  DeviceTextDescription,
  DeviceTextLocationInformation
} DEVICE_TEXT_TYPE, *PDEVICE_TEXT_TYPE;

typedef enum _DEVICE_USAGE_NOTIFICATION_TYPE {
  DeviceUsageTypeUndefined,
  DeviceUsageTypePaging,
  DeviceUsageTypeHibernation,
  DeviceUsageTypeDumpFile
} DEVICE_USAGE_NOTIFICATION_TYPE;

typedef struct _POWER_SEQUENCE {
  ULONG  SequenceD1;
  ULONG  SequenceD2;
  ULONG  SequenceD3;
} POWER_SEQUENCE, *PPOWER_SEQUENCE;

typedef enum _POWER_STATE_TYPE {
  SystemPowerState,
  DevicePowerState
} POWER_STATE_TYPE, *PPOWER_STATE_TYPE;

typedef union _POWER_STATE {
  SYSTEM_POWER_STATE  SystemState;
  DEVICE_POWER_STATE  DeviceState;
} POWER_STATE, *PPOWER_STATE;

typedef struct _CM_PARTIAL_RESOURCE_DESCRIPTOR {
  UCHAR Type;
  UCHAR ShareDisposition;
  USHORT Flags;
  union {
    struct {
      PHYSICAL_ADDRESS Start;
      ULONG Length;
    } Generic;
    struct {
      PHYSICAL_ADDRESS Start;
      ULONG Length;
    } Port;
    struct {
      ULONG Level;
      ULONG Vector;
      ULONG Affinity;
    } Interrupt;
    struct {
      PHYSICAL_ADDRESS Start;
      ULONG Length;
    } Memory;
    struct {
      ULONG Channel;
      ULONG Port;
      ULONG Reserved1;
    } Dma;
    struct {
      ULONG Data[3];
    } DevicePrivate;
    struct {
      ULONG Start;
      ULONG Length;
      ULONG Reserved;
    } BusNumber;
    struct {
      ULONG DataSize;
      ULONG Reserved1;
      ULONG Reserved2;
    } DeviceSpecificData;
  } u;
} CM_PARTIAL_RESOURCE_DESCRIPTOR, *PCM_PARTIAL_RESOURCE_DESCRIPTOR;

typedef struct _CM_PARTIAL_RESOURCE_LIST {
  USHORT  Version;
  USHORT  Revision;
  ULONG  Count;
  CM_PARTIAL_RESOURCE_DESCRIPTOR PartialDescriptors[1];
} CM_PARTIAL_RESOURCE_LIST, *PCM_PARTIAL_RESOURCE_LIST;

typedef struct _CM_FULL_RESOURCE_DESCRIPTOR {
  INTERFACE_TYPE  InterfaceType;
  ULONG  BusNumber;
  CM_PARTIAL_RESOURCE_LIST  PartialResourceList;
} CM_FULL_RESOURCE_DESCRIPTOR, *PCM_FULL_RESOURCE_DESCRIPTOR;

typedef struct _CM_RESOURCE_LIST {
  ULONG  Count;
  CM_FULL_RESOURCE_DESCRIPTOR  List[1];
} CM_RESOURCE_LIST, *PCM_RESOURCE_LIST;

typedef NTSTATUS (WINAPI *PIO_COMPLETION_ROUTINE)(
  IN struct _DEVICE_OBJECT  *DeviceObject,
  IN struct _IRP  *Irp,
  IN PVOID  Context);

#include <pshpack1.h>
typedef struct _IO_STACK_LOCATION {
  UCHAR  MajorFunction;
  UCHAR  MinorFunction;
  UCHAR  Flags;
  UCHAR  Control;
  union {
    struct {
      PIO_SECURITY_CONTEXT  SecurityContext;
      ULONG  Options;
      USHORT POINTER_ALIGNMENT  FileAttributes;
      USHORT  ShareAccess;
      ULONG POINTER_ALIGNMENT  EaLength;
    } Create;
    struct {
      ULONG  Length;
      ULONG POINTER_ALIGNMENT  Key;
      LARGE_INTEGER  ByteOffset;
    } Read;
    struct {
      ULONG  Length;
      ULONG POINTER_ALIGNMENT  Key;
      LARGE_INTEGER  ByteOffset;
    } Write;
    struct {
      ULONG  Length;
      FILE_INFORMATION_CLASS POINTER_ALIGNMENT  FileInformationClass;
    } QueryFile;
    struct {
      ULONG  Length;
      FILE_INFORMATION_CLASS POINTER_ALIGNMENT  FileInformationClass;
      PFILE_OBJECT  FileObject;
      union {
        struct {
          BOOLEAN  ReplaceIfExists;
          BOOLEAN  AdvanceOnly;
        } DUMMYSTRUCTNAME;
        ULONG  ClusterCount;
        HANDLE  DeleteHandle;
      } DUMMYUNIONNAME;
    } SetFile;
    struct {
      ULONG  Length;
      FS_INFORMATION_CLASS POINTER_ALIGNMENT  FsInformationClass;
    } QueryVolume;
    struct {
      ULONG  OutputBufferLength;
      ULONG POINTER_ALIGNMENT  InputBufferLength;
      ULONG POINTER_ALIGNMENT  IoControlCode;
      PVOID  Type3InputBuffer;
    } DeviceIoControl;
    struct {
      SECURITY_INFORMATION  SecurityInformation;
      ULONG POINTER_ALIGNMENT  Length;
    } QuerySecurity;
    struct {
      SECURITY_INFORMATION  SecurityInformation;
      PSECURITY_DESCRIPTOR  SecurityDescriptor;
    } SetSecurity;
    struct {
      PVPB  Vpb;
      PDEVICE_OBJECT  DeviceObject;
    } MountVolume;
    struct {
      PVPB  Vpb;
      PDEVICE_OBJECT  DeviceObject;
    } VerifyVolume;
    struct {
      struct _SCSI_REQUEST_BLOCK  *Srb;
    } Scsi;
    struct {
      DEVICE_RELATION_TYPE  Type;
    } QueryDeviceRelations;
    struct {
      CONST GUID  *InterfaceType;
      USHORT  Size;
      USHORT  Version;
      PINTERFACE  Interface;
      PVOID  InterfaceSpecificData;
    } QueryInterface;
    struct {
      PDEVICE_CAPABILITIES  Capabilities;
    } DeviceCapabilities;
    struct {
      PIO_RESOURCE_REQUIREMENTS_LIST  IoResourceRequirementList;
    } FilterResourceRequirements;
    struct {
      ULONG  WhichSpace;
      PVOID  Buffer;
      ULONG  Offset;
      ULONG POINTER_ALIGNMENT  Length;
    } ReadWriteConfig;
    struct {
      BOOLEAN  Lock;
    } SetLock;
    struct {
      BUS_QUERY_ID_TYPE  IdType;
    } QueryId;
    struct {
      DEVICE_TEXT_TYPE  DeviceTextType;
      LCID POINTER_ALIGNMENT  LocaleId;
    } QueryDeviceText;
    struct {
      BOOLEAN  InPath;
      BOOLEAN  Reserved[3];
      DEVICE_USAGE_NOTIFICATION_TYPE POINTER_ALIGNMENT  Type;
    } UsageNotification;
    struct {
      SYSTEM_POWER_STATE  PowerState;
    } WaitWake;
    struct {
      PPOWER_SEQUENCE  PowerSequence;
    } PowerSequence;
    struct {
      ULONG  SystemContext;
      POWER_STATE_TYPE POINTER_ALIGNMENT  Type;
      POWER_STATE POINTER_ALIGNMENT  State;
      POWER_ACTION POINTER_ALIGNMENT  ShutdownType;
    } Power;
    struct {
      PCM_RESOURCE_LIST  AllocatedResources;
      PCM_RESOURCE_LIST  AllocatedResourcesTranslated;
    } StartDevice;
    struct {
      ULONG_PTR  ProviderId;
      PVOID  DataPath;
      ULONG  BufferSize;
      PVOID  Buffer;
    } WMI;
    struct {
      PVOID  Argument1;
      PVOID  Argument2;
      PVOID  Argument3;
      PVOID  Argument4;
    } Others;
  } Parameters;
  PDEVICE_OBJECT  DeviceObject;
  PFILE_OBJECT  FileObject;
  PIO_COMPLETION_ROUTINE  CompletionRoutine;
  PVOID  Context;
} IO_STACK_LOCATION, *PIO_STACK_LOCATION;
#include <poppack.h>

typedef struct _MDL {
  struct _MDL  *Next;
  CSHORT  Size;
  CSHORT  MdlFlags;
  struct _EPROCESS  *Process;
  PVOID  MappedSystemVa;
  PVOID  StartVa;
  ULONG  ByteCount;
  ULONG  ByteOffset;
} MDL, *PMDL;

typedef NTSTATUS (WINAPI *PDRIVER_DISPATCH)(
  IN struct _DEVICE_OBJECT  *DeviceObject,
  IN struct _IRP  *Irp);

typedef struct _KSYSTEM_TIME {
    ULONG LowPart;
    LONG High1Time;
    LONG High2Time;
} KSYSTEM_TIME, *PKSYSTEM_TIME;

typedef enum _NT_PRODUCT_TYPE {
    NtProductWinNt = 1,
    NtProductLanManNt,
    NtProductServer
} NT_PRODUCT_TYPE, *PNT_PRODUCT_TYPE;

#define PROCESSOR_FEATURE_MAX 64

typedef enum _ALTERNATIVE_ARCHITECTURE_TYPE
{
   StandardDesign,
   NEC98x86,
   EndAlternatives
} ALTERNATIVE_ARCHITECTURE_TYPE;

typedef struct _KUSER_SHARED_DATA {
    ULONG TickCountLowDeprecated;
    ULONG TickCountMultiplier;
    volatile KSYSTEM_TIME InterruptTime;
    volatile KSYSTEM_TIME SystemTime;
    volatile KSYSTEM_TIME TimeZoneBias;
    USHORT ImageNumberLow;
    USHORT ImageNumberHigh;
    WCHAR NtSystemRoot[260];
    ULONG MaxStckTraceDepth;
    ULONG CryptoExponent;
    ULONG TimeZoneId;
    ULONG LargePageMinimum;
    ULONG Reserverd2[7];
    NT_PRODUCT_TYPE NtProductType;
    BOOLEAN ProductTypeIsValid;
    ULONG MajorNtVersion;
    ULONG MinorNtVersion;
    BOOLEAN ProcessorFeatures[PROCESSOR_FEATURE_MAX];
    ULONG Reserved1;
    ULONG Reserved3;
    volatile ULONG TimeSlip;
    ALTERNATIVE_ARCHITECTURE_TYPE AlternativeArchitecture;
    LARGE_INTEGER SystemExpirationDate;
    ULONG SuiteMask;
    BOOLEAN KdDebuggerEnabled;
    volatile ULONG ActiveConsoleId;
    volatile ULONG DismountCount;
    ULONG ComPlusPackage;
    ULONG LastSystemRITEventTickCount;
    ULONG NumberOfPhysicalPages;
    BOOLEAN SafeBootMode;
    ULONG TraceLogging;
    ULONGLONG Fill0;
    ULONGLONG SystemCall[4];
    union {
        volatile KSYSTEM_TIME TickCount;
        volatile ULONG64 TickCountQuad;
    };
} KSHARED_USER_DATA, *PKSHARED_USER_DATA;

#define IoGetCurrentIrpStackLocation(_Irp) ((_Irp)->Tail.Overlay.CurrentStackLocation)

#define KernelMode 0
#define UserMode   1

#endif
