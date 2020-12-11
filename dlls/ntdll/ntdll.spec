#note that the Zw... functions are alternate names for the
#Nt... functions.  (see www.sysinternals.com for details)
#if you change a Nt.. function DON'T FORGET to change the
#Zw one too.

@ stub CsrAllocateCaptureBuffer
@ stub CsrAllocateCapturePointer
@ stub CsrAllocateMessagePointer
@ stub CsrCaptureMessageBuffer
# @ stub CsrCaptureMessageMultiUnicodeStringsInPlace
@ stub CsrCaptureMessageString
@ stub CsrCaptureTimeout
@ stub CsrClientCallServer
@ stub CsrClientConnectToServer
@ stub CsrClientMaxMessage
@ stub CsrClientSendMessage
@ stub CsrClientThreadConnect
@ stub CsrFreeCaptureBuffer
# @ stub CsrGetProcessId
@ stub CsrIdentifyAlertableThread
@ stub CsrNewThread
@ stub CsrProbeForRead
@ stub CsrProbeForWrite
@ stub CsrSetPriorityClass
@ stub CsrpProcessCallbackRequest
@ stdcall DbgBreakPoint()
@ varargs DbgPrint(str)
@ varargs DbgPrintEx(long long str)
# @ stub DbgPrintReturnControlC
@ stub DbgPrompt
# @ stub DbgQueryDebugFilterState
# @ stub DbgSetDebugFilterState
@ stub DbgUiConnectToDbg
@ stub DbgUiContinue
# @ stub DbgUiConvertStateChangeStructure
# @ stub DbgUiDebugActiveProcess
# @ stub DbgUiGetThreadDebugObject
# @ stub DbgUiIssueRemoteBreakin
# @ stub DbgUiRemoteBreakin
# @ stub DbgUiSetThreadDebugObject
# @ stub DbgUiStopDebugging
@ stub DbgUiWaitStateChange
@ stdcall DbgUserBreakPoint()
# @ stub KiFastSystemCall
# @ stub KiFastSystemCallRet
# @ stub KiIntSystemCall
# @ stub KiRaiseUserExceptionDispatcher
@ stub KiUserApcDispatcher
@ stub KiUserCallbackDispatcher
@ stub KiUserExceptionDispatcher
# @ stub LdrAccessOutOfProcessResource
@ stdcall LdrAccessResource(long ptr ptr ptr)
# @ stub LdrAddRefDll
# @ stub LdrAlternateResourcesEnabled
# @ stub LdrCreateOutOfProcessImage
# @ stub LdrDestroyOutOfProcessImage
@ stdcall LdrDisableThreadCalloutsForDll(long)
@ stub LdrEnumResources
# @ stub LdrEnumerateLoadedModules
# @ stub LdrFindCreateProcessManifest
@ stdcall LdrFindEntryForAddress(ptr ptr)
@ stdcall LdrFindResourceDirectory_U(long ptr long ptr)
# @ stub LdrFindResourceEx_U
@ stdcall LdrFindResource_U(long ptr long ptr)
@ stub LdrFlushAlternateResourceModules
@ stdcall LdrGetDllHandle(long long ptr ptr)
# @ stub LdrGetDllHandleEx
@ stdcall LdrGetProcedureAddress(ptr ptr long ptr)
# @ stub LdrHotPatchRoutine
@ stub LdrInitShimEngineDynamic
@ stdcall LdrInitializeThunk(long long long long)
@ stub LdrLoadAlternateResourceModule
@ stdcall LdrLoadDll(wstr long ptr ptr)
@ stdcall LdrLockLoaderLock(long ptr ptr)
@ stub LdrProcessRelocationBlock
@ stub LdrQueryImageFileExecutionOptions
@ stdcall LdrQueryProcessModuleInformation(ptr long ptr)
@ stub LdrSetAppCompatDllRedirectionCallback
@ stub LdrSetDllManifestProber
@ stdcall LdrShutdownProcess()
@ stdcall LdrShutdownThread()
@ stub LdrUnloadAlternateResourceModule
@ stdcall LdrUnloadDll(ptr)
@ stdcall LdrUnlockLoaderLock(long long)
@ stub LdrVerifyImageMatchesChecksum
@ extern NlsAnsiCodePage
@ extern NlsMbCodePageTag
@ extern NlsMbOemCodePageTag
@ stdcall NtAcceptConnectPort(ptr long ptr long long ptr)
@ stdcall NtAccessCheck(ptr long long ptr ptr ptr ptr ptr)
@ stdcall NtAccessCheckAndAuditAlarm(ptr long ptr ptr ptr long ptr long ptr ptr ptr)
# @ stub NtAccessCheckByType
# @ stub NtAccessCheckByTypeAndAuditAlarm
# @ stub NtAccessCheckByTypeResultList
# @ stub NtAccessCheckByTypeResultListAndAuditAlarm
# @ stub NtAccessCheckByTypeResultListAndAuditAlarmByHandle
@ stdcall NtAddAtom(ptr long ptr)
# @ stub NtAddBootEntry
@ stdcall NtAdjustGroupsToken(long long ptr long ptr ptr)
@ stdcall NtAdjustPrivilegesToken(long long long long long long)
@ stdcall NtAlertResumeThread(long ptr)
@ stdcall NtAlertThread(long)
@ stdcall NtAllocateLocallyUniqueId(ptr)
# @ stub NtAllocateUserPhysicalPages
@ stdcall NtAllocateUuids(ptr ptr ptr)
@ stdcall NtAllocateVirtualMemory(long ptr ptr ptr long long)
# @ stub NtAreMappedFilesTheSame
# @ stub NtAssignProcessToJobObject
@ stub NtCallbackReturn
# @ stub NtCancelDeviceWakeupRequest
@ stdcall NtCancelIoFile(long ptr)
@ stdcall NtCancelTimer(long ptr)
@ stdcall NtClearEvent(long)
@ stdcall NtClose(long)
@ stub NtCloseObjectAuditAlarm
# @ stub NtCompactKeys
# @ stub NtCompareTokens
@ stdcall NtCompleteConnectPort(ptr)
# @ stub NtCompressKey
@ stdcall NtConnectPort(ptr ptr ptr ptr ptr ptr ptr ptr)
@ stub NtContinue
# @ stub NtCreateDebugObject
@ stdcall NtCreateDirectoryObject(long long long)
@ stdcall NtCreateEvent(long long long long long)
@ stub NtCreateEventPair
@ stdcall NtCreateFile(ptr long ptr ptr long long long ptr long long ptr)
@ stub NtCreateIoCompletion
# @ stub NtCreateJobObject
# @ stub NtCreateJobSet
@ stdcall NtCreateKey(ptr long ptr long ptr long long)
# @ stub NtCreateKeyedEvent
@ stdcall NtCreateMailslotFile(long long long long long long long long)
@ stdcall NtCreateMutant(ptr long ptr long)
@ stdcall NtCreateNamedPipeFile(ptr long ptr ptr long long long long long long long long long ptr)
@ stdcall NtCreatePagingFile(long long long long)
@ stdcall NtCreatePort(ptr ptr long long ptr)
@ stub NtCreateProcess
# @ stub NtCreateProcessEx
@ stub NtCreateProfile
@ stdcall NtCreateSection(ptr long ptr ptr long long long)
@ stdcall NtCreateSemaphore(ptr long ptr long long)
@ stdcall NtCreateSymbolicLinkObject(ptr long ptr ptr)
@ stub NtCreateThread
@ stdcall NtCreateTimer(ptr long ptr long)
@ stub NtCreateToken
# @ stub NtCreateWaitablePort
@ stdcall NtCurrentTeb()
# @ stub NtDebugActiveProcess
# @ stub NtDebugContinue
@ stdcall NtDelayExecution(long ptr)
@ stdcall NtDeleteAtom(long)
# @ stub NtDeleteBootEntry
@ stdcall NtDeleteFile(ptr)
@ stdcall NtDeleteKey(long)
# @ stub NtDeleteObjectAuditAlarm
@ stdcall NtDeleteValueKey(long ptr)
@ stdcall NtDeviceIoControlFile(long long long long long long long long long long)
@ stdcall NtDisplayString(ptr)
@ stdcall NtDuplicateObject(long long long ptr long long long)
@ stdcall NtDuplicateToken(long long long long long long)
# @ stub NtEnumerateBootEntries
@ stub NtEnumerateBus
@ stdcall NtEnumerateKey (long long long long long long)
# @ stub NtEnumerateSystemEnvironmentValuesEx
@ stdcall NtEnumerateValueKey (long long long long long long)
@ stub NtExtendSection
# @ stub NtFilterToken
@ stdcall NtFindAtom(ptr long ptr)
@ stdcall NtFlushBuffersFile(long ptr)
@ stdcall NtFlushInstructionCache(long ptr long)
@ stdcall NtFlushKey(long)
@ stdcall NtFlushVirtualMemory(long ptr ptr long)
@ stub NtFlushWriteBuffer
# @ stub NtFreeUserPhysicalPages
@ stdcall NtFreeVirtualMemory(long ptr ptr long)
@ stdcall NtFsControlFile(long long long long long long long long long long)
@ stdcall NtGetContextThread(long ptr)
# @ stub NtGetDevicePowerState
@ stub NtGetPlugPlayEvent
@ stub NtGetTickCount
# @ stub NtGetWriteWatch
@ stub NtImpersonateAnonymousToken
@ stub NtImpersonateClientOfPort
@ stub NtImpersonateThread
@ stub NtInitializeRegistry
@ stdcall NtInitiatePowerAction (long long long long)
# @ stub NtIsProcessInJob
# @ stub NtIsSystemResumeAutomatic
@ stdcall NtListenPort(ptr ptr)
@ stdcall NtLoadDriver(ptr)
# @ stub NtLoadKey2
@ stdcall NtLoadKey(ptr ptr)
@ stdcall NtLockFile(long long ptr ptr ptr ptr ptr ptr long long)
# @ stub NtLockProductActivationKeys
# @ stub NtLockRegistryKey
@ stdcall NtLockVirtualMemory(long ptr ptr long)
# @ stub NtMakePermanentObject
@ stub NtMakeTemporaryObject
# @ stub NtMapUserPhysicalPages
# @ stub NtMapUserPhysicalPagesScatter
@ stdcall NtMapViewOfSection(long long ptr long long ptr ptr long long long)
# @ stub NtModifyBootEntry
@ stub NtNotifyChangeDirectoryFile
@ stdcall NtNotifyChangeKey(long long ptr ptr ptr long long ptr long long)
# @ stub NtNotifyChangeMultipleKeys
@ stdcall NtOpenDirectoryObject(long long long)
@ stdcall NtOpenEvent(long long long)
@ stub NtOpenEventPair
@ stdcall NtOpenFile(ptr long ptr ptr long long)
@ stub NtOpenIoCompletion
# @ stub NtOpenJobObject
@ stdcall NtOpenKey(ptr long ptr)
# @ stub NtOpenKeyedEvent
@ stdcall NtOpenMutant(ptr long ptr)
@ stub NtOpenObjectAuditAlarm
@ stdcall NtOpenProcess(ptr long ptr ptr)
@ stdcall NtOpenProcessToken(long long long)
# @ stub NtOpenProcessTokenEx
@ stdcall NtOpenSection(ptr long ptr)
@ stdcall NtOpenSemaphore(long long ptr)
@ stdcall NtOpenSymbolicLinkObject (ptr long ptr)
@ stdcall NtOpenThread(ptr long ptr ptr)
@ stdcall NtOpenThreadToken(long long long long)
# @ stub NtOpenThreadTokenEx
@ stdcall NtOpenTimer(ptr long ptr)
@ stub NtPlugPlayControl
@ stdcall NtPowerInformation(long ptr long ptr long)
@ stdcall NtPrivilegeCheck(ptr ptr ptr)
@ stub NtPrivilegeObjectAuditAlarm
@ stub NtPrivilegedServiceAuditAlarm
@ stdcall NtProtectVirtualMemory(long ptr ptr long ptr)
@ stdcall NtPulseEvent(long ptr)
@ stdcall NtQueryAttributesFile(ptr ptr)
# @ stub NtQueryBootEntryOrder
# @ stub NtQueryBootOptions
# @ stub NtQueryDebugFilterState
@ stdcall NtQueryDefaultLocale(long ptr)
@ stdcall NtQueryDefaultUILanguage(ptr)
@ stdcall NtQueryDirectoryFile(long long ptr ptr ptr ptr long long long ptr long)
@ stdcall NtQueryDirectoryObject(long ptr long long long ptr ptr)
@ stub NtQueryEaFile
@ stdcall NtQueryEvent(long long ptr long ptr)
@ stdcall NtQueryFullAttributesFile(ptr ptr)
@ stdcall NtQueryInformationAtom(long long ptr long ptr)
@ stdcall NtQueryInformationFile(long ptr ptr long long)
# @ stub NtQueryInformationJobObject
@ stub NtQueryInformationPort
@ stdcall NtQueryInformationProcess(long long ptr long ptr)
@ stdcall NtQueryInformationThread(long long ptr long ptr)
@ stdcall NtQueryInformationToken(long long ptr long ptr)
@ stdcall NtQueryInstallUILanguage(ptr)
@ stub NtQueryIntervalProfile
@ stub NtQueryIoCompletion
@ stdcall NtQueryKey (long long ptr long ptr)
# @ stub NtQueryMultipleValueKey
@ stdcall NtQueryMutant(long long ptr long ptr)
@ stdcall NtQueryObject(long long long long long)
@ stub NtQueryOpenSubKeys
@ stdcall NtQueryPerformanceCounter(ptr ptr)
# @ stub NtQueryPortInformationProcess
# @ stub NtQueryQuotaInformationFile
@ stdcall NtQuerySection (long long long long long)
@ stdcall NtQuerySecurityObject (long long long long long)
@ stdcall NtQuerySemaphore (long long ptr long ptr)
@ stdcall NtQuerySymbolicLinkObject(long ptr ptr)
@ stub NtQuerySystemEnvironmentValue
# @ stub NtQuerySystemEnvironmentValueEx
@ stdcall NtQuerySystemInformation(long long long long)
@ stdcall NtQuerySystemTime(ptr)
@ stdcall NtQueryTimer(ptr long ptr long ptr)
@ stdcall NtQueryTimerResolution(long long long)
@ stdcall NtQueryValueKey(long long long long long long)
@ stdcall NtQueryVirtualMemory(long ptr long ptr long ptr)
@ stdcall NtQueryVolumeInformationFile(long ptr ptr long long)
@ stdcall NtQueueApcThread(long ptr long long long)
@ stdcall NtRaiseException(ptr ptr long)
@ stub NtRaiseHardError
@ stdcall NtReadFile(long long long long long long long long long)
@ stub NtReadFileScatter
@ stub NtReadRequestData
@ stdcall NtReadVirtualMemory(long ptr ptr long ptr)
@ stub NtRegisterNewDevice
@ stdcall NtRegisterThreadTerminatePort(ptr)
# @ stub NtReleaseKeyedEvent
@ stdcall NtReleaseMutant(long ptr)
@ stub NtReleaseProcessMutant
@ stdcall NtReleaseSemaphore(long long ptr)
@ stub NtRemoveIoCompletion
# @ stub NtRemoveProcessDebug
# @ stub NtRenameKey
@ stdcall NtReplaceKey(ptr long ptr)
@ stub NtReplyPort
@ stdcall NtReplyWaitReceivePort(ptr ptr ptr ptr)
@ stub NtReplyWaitReceivePortEx
@ stub NtReplyWaitReplyPort
# @ stub NtRequestDeviceWakeup
@ stub NtRequestPort
@ stdcall NtRequestWaitReplyPort(ptr ptr ptr)
# @ stub NtRequestWakeupLatency
@ stdcall NtResetEvent(long ptr)
# @ stub NtResetWriteWatch
@ stdcall NtRestoreKey(long long long)
# @ stub NtResumeProcess
@ stdcall NtResumeThread(long long)
@ stdcall NtSaveKey(long long)
# @ stub NtSaveKeyEx
# @ stub NtSaveMergedKeys
@ stub NtSecureConnectPort
# @ stub NtSetBootEntryOrder
# @ stub NtSetBootOptions
@ stdcall NtSetContextThread(long ptr)
# @ stub NtSetDebugFilterState
@ stub NtSetDefaultHardErrorPort
@ stdcall NtSetDefaultLocale(long long)
@ stdcall NtSetDefaultUILanguage(long)
@ stub NtSetEaFile
@ stdcall NtSetEvent(long long)
# @ stub NtSetEventBoostPriority
@ stub NtSetHighEventPair
@ stub NtSetHighWaitLowEventPair
@ stub NtSetHighWaitLowThread
# @ stub NtSetInformationDebugObject
@ stdcall NtSetInformationFile(long long long long long)
# @ stub NtSetInformationJobObject
@ stdcall NtSetInformationKey(long long ptr long)
@ stdcall NtSetInformationObject(long long ptr long)
@ stdcall NtSetInformationProcess(long long long long)
@ stdcall NtSetInformationThread(long long ptr long)
@ stdcall NtSetInformationToken(long long ptr long)
@ stdcall NtSetIntervalProfile(long long)
@ stub NtSetIoCompletion
@ stub NtSetLdtEntries
@ stub NtSetLowEventPair
@ stub NtSetLowWaitHighEventPair
@ stub NtSetLowWaitHighThread
# @ stub NtSetQuotaInformationFile
@ stdcall NtSetSecurityObject(long long ptr)
@ stub NtSetSystemEnvironmentValue
# @ stub NtSetSystemEnvironmentValueEx
@ stub NtSetSystemInformation
@ stub NtSetSystemPowerState
@ stdcall NtSetSystemTime(ptr ptr)
# @ stub NtSetThreadExecutionState
@ stdcall NtSetTimer(long ptr ptr ptr long long ptr)
@ stdcall NtSetTimerResolution(long long ptr)
# @ stub NtSetUuidSeed
@ stdcall NtSetValueKey(long long long long long long)
@ stdcall NtSetVolumeInformationFile(long ptr ptr long long)
@ stdcall NtShutdownSystem(long)
@ stdcall NtSignalAndWaitForSingleObject(long long long ptr)
@ stub NtStartProfile
@ stub NtStopProfile
# @ stub NtSuspendProcess
@ stdcall NtSuspendThread(long ptr)
@ stub NtSystemDebugControl
# @ stub NtTerminateJobObject
@ stdcall NtTerminateProcess(long long)
@ stdcall NtTerminateThread(long long)
@ stub NtTestAlert
# @ stub NtTraceEvent
# @ stub NtTranslateFilePath
@ stdcall NtUnloadDriver(ptr)
@ stdcall NtUnloadKey(long)
@ stub NtUnloadKeyEx
@ stdcall NtUnlockFile(long ptr ptr ptr ptr)
@ stdcall NtUnlockVirtualMemory(long ptr ptr long)
@ stdcall NtUnmapViewOfSection(long ptr)
@ stub NtVdmControl
@ stub NtW32Call
# @ stub NtWaitForDebugEvent
# @ stub NtWaitForKeyedEvent
@ stdcall NtWaitForMultipleObjects(long ptr long long ptr)
@ stub NtWaitForProcessMutant
@ stdcall NtWaitForSingleObject(long long long)
@ stub NtWaitHighEventPair
@ stub NtWaitLowEventPair
@ stdcall NtWriteFile(long long ptr ptr ptr ptr long ptr ptr)
@ stub NtWriteFileGather
@ stub NtWriteRequestData
@ stdcall NtWriteVirtualMemory(long ptr ptr long ptr)
@ stdcall NtYieldExecution()
@ stub PfxFindPrefix
@ stub PfxInitialize
@ stub PfxInsertPrefix
@ stub PfxRemovePrefix
# @ stub PropertyLengthAsVariant
@ stub RtlAbortRXact
@ stdcall RtlAbsoluteToSelfRelativeSD(ptr ptr ptr)
@ stdcall RtlAcquirePebLock()
@ stdcall RtlAcquireResourceExclusive(ptr long)
@ stdcall RtlAcquireResourceShared(ptr long)
@ stub RtlActivateActivationContext
@ stub RtlActivateActivationContextEx
@ stub RtlActivateActivationContextUnsafeFast
@ stdcall RtlAddAccessAllowedAce(ptr long long ptr)
@ stdcall RtlAddAccessAllowedAceEx(ptr long long long ptr)
# @ stub RtlAddAccessAllowedObjectAce
@ stdcall RtlAddAccessDeniedAce(ptr long long ptr)
@ stdcall RtlAddAccessDeniedAceEx(ptr long long long ptr)
# @ stub RtlAddAccessDeniedObjectAce
@ stdcall RtlAddAce(ptr long long ptr long)
@ stub RtlAddActionToRXact
@ stdcall RtlAddAtomToAtomTable(ptr wstr ptr)
@ stub RtlAddAttributeActionToRXact
@ stdcall RtlAddAuditAccessAce(ptr long long ptr long long) 
# @ stub RtlAddAuditAccessAceEx
# @ stub RtlAddAuditAccessObjectAce
# @ stub RtlAddCompoundAce
# @ stub RtlAddRange
# @ stub RtlAddRefActivationContext
# @ stub RtlAddRefMemoryStream
@ stdcall RtlAddVectoredExceptionHandler(long ptr)
# @ stub RtlAddressInSectionTable
@ stdcall RtlAdjustPrivilege(long long long ptr)
@ stdcall RtlAllocateAndInitializeSid (ptr long long long long long long long long long ptr)
@ stdcall RtlAllocateHandle(ptr ptr)
@ stdcall RtlAllocateHeap(long long long)
@ stdcall RtlAnsiCharToUnicodeChar(ptr)
@ stdcall RtlAnsiStringToUnicodeSize(ptr)
@ stdcall RtlAnsiStringToUnicodeString(ptr ptr long)
@ stdcall RtlAppendAsciizToString(ptr str)
# @ stub RtlAppendPathElement
@ stdcall RtlAppendStringToString(ptr ptr)
@ stdcall RtlAppendUnicodeStringToString(ptr ptr)
@ stdcall RtlAppendUnicodeToString(ptr wstr)
# @ stub RtlApplicationVerifierStop
@ stub RtlApplyRXact
@ stub RtlApplyRXactNoFlush
@ stdcall RtlAreAllAccessesGranted(long long)
@ stdcall RtlAreAnyAccessesGranted(long long)
@ stdcall RtlAreBitsClear(ptr long long)
@ stdcall RtlAreBitsSet(ptr long long)
# @ stub RtlAssert2
@ stdcall RtlAssert(ptr ptr long long)
# @ stub RtlCancelTimer
# @ stub RtlCaptureContext
@ stub RtlCaptureStackBackTrace
# @ stub RtlCaptureStackContext
@ stdcall RtlCharToInteger(ptr long ptr)
# @ stub RtlCheckForOrphanedCriticalSections
# @ stub RtlCheckProcessParameters
@ stdcall RtlCheckRegistryKey(long ptr)
@ stdcall RtlClearAllBits(ptr)
@ stdcall RtlClearBits(ptr long long)
# @ stub RtlCloneMemoryStream
@ stub RtlClosePropertySet
# @ stub RtlCommitMemoryStream
@ stdcall RtlCompactHeap(long long)
@ stdcall RtlCompareMemory(ptr ptr long)
@ stdcall RtlCompareMemoryUlong(ptr long long)
@ stdcall RtlCompareString(ptr ptr long)
@ stdcall RtlCompareUnicodeString (ptr ptr long)
@ stub RtlCompressBuffer
@ stdcall RtlComputeCrc32(long ptr long)
# @ stub RtlComputeImportTableHash
# @ stub RtlComputePrivatizedDllName_U
@ stub RtlConsoleMultiByteToUnicodeN
@ stub RtlConvertExclusiveToShared
@ stdcall -ret64 RtlConvertLongToLargeInteger(long)
# @ stub RtlConvertPropertyToVariant
@ stub RtlConvertSharedToExclusive
@ stdcall RtlConvertSidToUnicodeString(ptr ptr long)
# @ stub RtlConvertToAutoInheritSecurityObject
@ stub RtlConvertUiListToApiList
@ stdcall -ret64 RtlConvertUlongToLargeInteger(long)
# @ stub RtlConvertVariantToProperty
@ stdcall RtlCopyLuid(ptr ptr)
@ stdcall RtlCopyLuidAndAttributesArray(long ptr ptr)
# @ stub RtlCopyMemoryStreamTo
# @ stub RtlCopyOutOfProcessMemoryStreamTo
# @ stub RtlCopyRangeList
@ stdcall RtlCopySecurityDescriptor(ptr ptr)
@ stdcall RtlCopySid(long ptr ptr)
@ stub RtlCopySidAndAttributesArray
@ stdcall RtlCopyString(ptr ptr)
@ stdcall RtlCopyUnicodeString(ptr ptr)
@ stdcall RtlCreateAcl(ptr long long)
# @ stub RtlCreateActivationContext
@ stub RtlCreateAndSetSD
@ stdcall RtlCreateAtomTable(long ptr)
# @ stub RtlCreateBootStatusDataFile
@ stdcall RtlCreateEnvironment(long ptr)
@ stdcall RtlCreateHeap(long ptr long long ptr ptr)
@ stdcall RtlCreateProcessParameters(ptr ptr ptr ptr ptr ptr ptr ptr ptr ptr)
@ stub RtlCreatePropertySet
@ stdcall RtlCreateQueryDebugBuffer(long long)
@ stub RtlCreateRegistryKey
@ stdcall RtlCreateSecurityDescriptor(ptr long)
# @ stub RtlCreateSystemVolumeInformationFolder
@ stub RtlCreateTagHeap
# @ stub RtlCreateTimer
# @ stub RtlCreateTimerQueue
@ stdcall RtlCreateUnicodeString(ptr wstr)
@ stdcall RtlCreateUnicodeStringFromAsciiz(ptr str)
@ stub RtlCreateUserProcess
@ stub RtlCreateUserSecurityObject
@ stdcall RtlCreateUserThread(long ptr long ptr long long ptr ptr ptr ptr)
@ stub RtlCustomCPToUnicodeN
@ stub RtlCutoverTimeToSystemTime
@ stdcall RtlDeNormalizeProcessParams(ptr)
@ stub RtlDeactivateActivationContext
@ stub RtlDeactivateActivationContextUnsafeFast
@ stub RtlDebugPrintTimes
# @ stub RtlDecodePointer
# @ stub RtlDecodeSystemPointer
@ stub RtlDecompressBuffer
@ stub RtlDecompressFragment
@ stub RtlDefaultNpAcl
@ stub RtlDelete
@ stdcall RtlDeleteAce(ptr long)
@ stdcall RtlDeleteAtomFromAtomTable(ptr long)
@ stdcall RtlDeleteCriticalSection(ptr)
@ stub RtlDeleteElementGenericTable
@ stub RtlDeleteElementGenericTableAvl
@ stub RtlDeleteNoSplay
@ stub RtlDeleteOwnersRanges
@ stub RtlDeleteRange
@ stdcall RtlDeleteRegistryValue(long ptr ptr)
@ stdcall RtlDeleteResource(ptr)
@ stdcall RtlDeleteSecurityObject(long)
# @ stub RtlDeleteTimer
# @ stub RtlDeleteTimerQueue
# @ stub RtlDeleteTimerQueueEx
# @ stub RtlDeregisterWait
# @ stub RtlDeregisterWaitEx
@ stdcall RtlDestroyAtomTable(ptr)
@ stdcall RtlDestroyEnvironment(ptr)
@ stdcall RtlDestroyHandleTable(ptr)
@ stdcall RtlDestroyHeap(long)
@ stdcall RtlDestroyProcessParameters(ptr)
@ stdcall RtlDestroyQueryDebugBuffer(ptr)
@ stdcall RtlDetermineDosPathNameType_U(wstr)
# @ stub RtlDllShutdownInProgress
# @ stub RtlDnsHostNameToComputerName
@ stdcall RtlDoesFileExists_U(wstr)
# @ stub RtlDosApplyFileIsolationRedirection_Ustr
@ stdcall RtlDosPathNameToNtPathName_U(wstr ptr ptr ptr)
@ stdcall RtlDosSearchPath_U(wstr wstr wstr long ptr ptr)
# @ stub RtlDosSearchPath_Ustr
@ stdcall RtlDowncaseUnicodeChar(long)
@ stdcall RtlDowncaseUnicodeString(ptr ptr long)
@ stdcall RtlDumpResource(ptr)
@ stdcall RtlDuplicateUnicodeString(long ptr ptr)
@ stdcall RtlEmptyAtomTable(ptr long)
# @ stub RtlEnableEarlyCriticalSectionEventCreation
# @ stub RtlEncodePointer
# @ stub RtlEncodeSystemPointer
@ stdcall -ret64 RtlEnlargedIntegerMultiply(long long)
@ stdcall RtlEnlargedUnsignedDivide(double long ptr)
@ stdcall -ret64 RtlEnlargedUnsignedMultiply(long long)
@ stdcall RtlEnterCriticalSection(ptr)
@ stub RtlEnumProcessHeaps
@ stub RtlEnumerateGenericTable
# @ stub RtlEnumerateGenericTableAvl
# @ stub RtlEnumerateGenericTableLikeADirectory
@ stub RtlEnumerateGenericTableWithoutSplaying
# @ stub RtlEnumerateGenericTableWithoutSplayingAvl
@ stub RtlEnumerateProperties
@ stdcall RtlEqualComputerName(ptr ptr)
@ stdcall RtlEqualDomainName(ptr ptr)
@ stdcall RtlEqualLuid(ptr ptr)
@ stdcall RtlEqualPrefixSid(ptr ptr)
@ stdcall RtlEqualSid(long long)
@ stdcall RtlEqualString(ptr ptr long)
@ stdcall RtlEqualUnicodeString(ptr ptr long)
@ stdcall RtlEraseUnicodeString(ptr)
@ stdcall RtlExitUserThread(long)
@ stdcall RtlExpandEnvironmentStrings_U(ptr ptr ptr ptr)
@ stub RtlExtendHeap
@ stdcall -ret64 RtlExtendedIntegerMultiply(double long)
@ stdcall -ret64 RtlExtendedLargeIntegerDivide(double long ptr)
@ stdcall -ret64 RtlExtendedMagicDivide(double double long)
@ stdcall RtlFillMemory(ptr long long)
@ stdcall RtlFillMemoryUlong(ptr long long)
@ stub RtlFinalReleaseOutOfProcessMemoryStream
@ stub RtlFindActivationContextSectionGuid
@ stub RtlFindActivationContextSectionString
@ stdcall RtlFindCharInUnicodeString(long ptr ptr ptr)
@ stdcall RtlFindClearBits(ptr long long)
@ stdcall RtlFindClearBitsAndSet(ptr long long)
@ stdcall RtlFindClearRuns(ptr ptr long long)
@ stdcall RtlFindLastBackwardRunClear(ptr long ptr)
@ stdcall RtlFindLastBackwardRunSet(ptr long ptr)
@ stdcall RtlFindLeastSignificantBit(double)
@ stdcall RtlFindLongestRunClear(ptr long)
@ stdcall RtlFindLongestRunSet(ptr long)
@ stdcall RtlFindMessage(long long long long ptr)
@ stdcall RtlFindMostSignificantBit(double)
@ stdcall RtlFindNextForwardRunClear(ptr long ptr)
@ stdcall RtlFindNextForwardRunSet(ptr long ptr)
@ stub RtlFindRange
@ stdcall RtlFindSetBits(ptr long long)
@ stdcall RtlFindSetBitsAndClear(ptr long long)
@ stdcall RtlFindSetRuns(ptr ptr long long)
@ stub RtlFirstEntrySList
@ stdcall RtlFirstFreeAce(ptr ptr)
@ stub RtlFlushPropertySet
# @ stub RtlFlushSecureMemoryCache
@ stdcall RtlFormatCurrentUserKeyPath(ptr)
@ stdcall RtlFormatMessage(ptr long long long long ptr ptr long)
@ stdcall RtlFreeAnsiString(long)
@ stdcall RtlFreeHandle(ptr ptr)
@ stdcall RtlFreeHeap(long long long)
@ stdcall RtlFreeOemString(ptr)
# @ stub RtlFreeRangeList
@ stdcall RtlFreeSid (long)
# @ stub RtlFreeThreadActivationContextStack
@ stdcall RtlFreeUnicodeString(ptr)
@ stub RtlFreeUserThreadStack
@ stdcall RtlGUIDFromString(ptr ptr)
@ stub RtlGenerate8dot3Name
@ stdcall RtlGetAce(ptr long ptr)
# @ stub RtlGetActiveActivationContext
@ stub RtlGetCallersAddress
@ stub RtlGetCompressionWorkSpaceSize
@ stdcall RtlGetControlSecurityDescriptor(ptr ptr ptr)
@ stdcall RtlGetCurrentDirectory_U(long ptr)
@ stdcall RtlGetCurrentPeb()
@ stdcall RtlGetDaclSecurityDescriptor(ptr ptr ptr ptr)
@ stub RtlGetElementGenericTable
# @ stub RtlGetElementGenericTableAvl
# @ stub RtlGetFirstRange
# @ stub RtlGetFrame
@ stdcall RtlGetFullPathName_U(wstr long ptr ptr)
@ stdcall RtlGetGroupSecurityDescriptor(ptr ptr ptr)
@ stdcall RtlGetLastNtStatus()
@ stdcall RtlGetLastWin32Error()
# @ stub RtlGetLengthWithoutLastFullDosOrNtPathElement
# @ stub RtlGetLengthWithoutTrailingPathSeperators
@ stdcall RtlGetLongestNtPathLength()
# @ stub RtlGetNativeSystemInformation
# @ stub RtlGetNextRange
@ stub RtlGetNtGlobalFlags
@ stdcall RtlGetNtProductType(ptr)
@ stdcall RtlGetNtVersionNumbers(ptr ptr ptr)
@ stdcall RtlGetOwnerSecurityDescriptor(ptr ptr ptr)
@ stdcall RtlGetProcessHeaps(long ptr)
@ stdcall RtlGetSaclSecurityDescriptor(ptr ptr ptr ptr)
# @ stub RtlGetSecurityDescriptorRMControl
# @ stub RtlGetSetBootStatusData
# @ stub RtlGetUnloadEventTrace
@ stub RtlGetUserInfoHeap
@ stdcall RtlGetVersion(ptr)
@ stub RtlGuidToPropertySetName
# @ stub RtlHashUnicodeString
@ stdcall RtlIdentifierAuthoritySid(ptr)
@ stdcall RtlImageDirectoryEntryToData(long long long ptr)
@ stdcall RtlImageNtHeader(long)
@ stdcall RtlImageRvaToSection(ptr long long)
@ stdcall RtlImageRvaToVa(ptr long long ptr)
@ stdcall RtlImpersonateSelf(long)
@ stdcall RtlInitAnsiString(ptr str)
@ stdcall RtlInitAnsiStringEx(ptr str)
@ stub RtlInitCodePageTable
# @ stub RtlInitMemoryStream
@ stub RtlInitNlsTables
# @ stub RtlInitOutOfProcessMemoryStream
@ stdcall RtlInitString(ptr str)
@ stdcall RtlInitUnicodeString(ptr wstr)
@ stdcall RtlInitUnicodeStringEx(ptr wstr)
# @ stub RtlInitializeAtomPackage
@ stdcall RtlInitializeBitMap(ptr long long)
@ stub RtlInitializeContext
@ stdcall RtlInitializeCriticalSection(ptr)
@ stdcall RtlInitializeCriticalSectionAndSpinCount(ptr long)
@ stdcall RtlInitializeGenericTable(ptr ptr ptr ptr ptr)
# @ stub RtlInitializeGenericTableAvl
@ stdcall RtlInitializeHandleTable(long long ptr)
@ stub RtlInitializeRXact
# @ stub RtlInitializeRangeList
@ stdcall RtlInitializeResource(ptr)
# @ stub RtlInitializeSListHead
@ stdcall RtlInitializeSid(ptr ptr long)
# @ stub RtlInitializeStackTraceDataBase
@ stub RtlInsertElementGenericTable
# @ stub RtlInsertElementGenericTableAvl
@ stdcall RtlInt64ToUnicodeString(double long ptr)
@ stdcall RtlIntegerToChar(long long long ptr)
@ stdcall RtlIntegerToUnicodeString(long long ptr)
# @ stub RtlInterlockedFlushSList
# @ stub RtlInterlockedPopEntrySList
# @ stub RtlInterlockedPushEntrySList
# @ stub RtlInterlockedPushListSList
# @ stub RtlInvertRangeList
# @ stub RtlIpv4AddressToStringA
# @ stub RtlIpv4AddressToStringExA
# @ stub RtlIpv4AddressToStringExW
# @ stub RtlIpv4AddressToStringW
# @ stub RtlIpv4StringToAddressA
# @ stub RtlIpv4StringToAddressExA
# @ stub RtlIpv4StringToAddressExW
# @ stub RtlIpv4StringToAddressW
# @ stub RtlIpv6AddressToStringA
# @ stub RtlIpv6AddressToStringExA
# @ stub RtlIpv6AddressToStringExW
# @ stub RtlIpv6AddressToStringW
# @ stub RtlIpv6StringToAddressA
# @ stub RtlIpv6StringToAddressExA
# @ stub RtlIpv6StringToAddressExW
# @ stub RtlIpv6StringToAddressW
# @ stub RtlIsActivationContextActive
@ stdcall RtlIsDosDeviceName_U(wstr)
@ stub RtlIsGenericTableEmpty
# @ stub RtlIsGenericTableEmptyAvl
@ stdcall RtlIsNameLegalDOS8Dot3(ptr ptr ptr)
# @ stub RtlIsRangeAvailable
@ stdcall RtlIsTextUnicode(ptr long ptr)
# @ stub RtlIsThreadWithinLoaderCallout
@ stdcall RtlIsValidHandle(ptr ptr)
@ stdcall RtlIsValidIndexHandle(ptr long ptr)
@ stdcall -ret64 RtlLargeIntegerAdd(double double)
@ stdcall -ret64 RtlLargeIntegerArithmeticShift(double long)
@ stdcall -ret64 RtlLargeIntegerDivide(double double ptr)
@ stdcall -ret64 RtlLargeIntegerNegate(double)
@ stdcall -ret64 RtlLargeIntegerShiftLeft(double long)
@ stdcall -ret64 RtlLargeIntegerShiftRight(double long)
@ stdcall -ret64 RtlLargeIntegerSubtract(double double)
@ stdcall RtlLargeIntegerToChar(ptr long long ptr)
@ stdcall RtlLeaveCriticalSection(ptr)
@ stdcall RtlLengthRequiredSid(long)
@ stdcall RtlLengthSecurityDescriptor(ptr)
@ stdcall RtlLengthSid(ptr)
@ stdcall RtlLocalTimeToSystemTime(ptr ptr)
# @ stub RtlLockBootStatusData
@ stdcall RtlLockHeap(long)
# @ stub RtlLockMemoryStreamRegion
# @ stub RtlLogStackBackTrace
@ stdcall RtlLookupAtomInAtomTable(ptr wstr ptr)
@ stub RtlLookupElementGenericTable
# @ stub RtlLookupElementGenericTableAvl
@ stdcall RtlMakeSelfRelativeSD(ptr ptr ptr)
@ stdcall RtlMapGenericMask(long ptr)
# @ stub RtlMapSecurityErrorToNtStatus
# @ stub RtlMergeRangeLists
@ stdcall RtlMoveMemory(ptr ptr long)
# @ stub RtlMultiAppendUnicodeStringBuffer
@ stdcall RtlMultiByteToUnicodeN(ptr long ptr ptr long)
@ stdcall RtlMultiByteToUnicodeSize(ptr str long)
@ stub RtlNewInstanceSecurityObject
@ stub RtlNewSecurityGrantedAccess
@ stdcall RtlNewSecurityObject(long long long long long long)
# @ stub RtlNewSecurityObjectEx
# @ stub RtlNewSecurityObjectWithMultipleInheritance
@ stdcall RtlNormalizeProcessParams(ptr)
# @ stub RtlNtPathNameToDosPathName
@ stdcall RtlNtStatusToDosError(long)
@ stdcall RtlNtStatusToDosErrorNoTeb(long)
@ stub RtlNumberGenericTableElements
# @ stub RtlNumberGenericTableElementsAvl
@ stdcall RtlNumberOfClearBits(ptr)
@ stdcall RtlNumberOfSetBits(ptr)
@ stdcall RtlOemStringToUnicodeSize(ptr)
@ stdcall RtlOemStringToUnicodeString(ptr ptr long)
@ stdcall RtlOemToUnicodeN(ptr long ptr ptr long)
@ stdcall RtlOpenCurrentUser(long ptr)
@ stub RtlPcToFileHeader
@ stdcall RtlPinAtomInAtomTable(ptr long)
# @ stub RtlPopFrame
@ stdcall RtlPrefixString(ptr ptr long)
@ stdcall RtlPrefixUnicodeString(ptr ptr long)
@ stub RtlPropertySetNameToGuid
@ stub RtlProtectHeap
# @ stub RtlPushFrame
@ stdcall RtlQueryAtomInAtomTable(ptr long ptr ptr ptr ptr)
@ stub RtlQueryDepthSList
@ stdcall RtlQueryEnvironmentVariable_U(ptr ptr ptr)
@ stub RtlQueryHeapInformation
@ stdcall RtlQueryInformationAcl(ptr ptr long long)
@ stub RtlQueryInformationActivationContext
@ stub RtlQueryInformationActiveActivationContext
@ stub RtlQueryInterfaceMemoryStream
@ stub RtlQueryProcessBackTraceInformation
@ stdcall RtlQueryProcessDebugInformation(long long ptr)
@ stub RtlQueryProcessHeapInformation
@ stub RtlQueryProcessLockInformation
@ stub RtlQueryProperties
@ stub RtlQueryPropertyNames
@ stub RtlQueryPropertySet
@ stdcall RtlQueryRegistryValues(long ptr ptr ptr ptr)
@ stub RtlQuerySecurityObject
@ stub RtlQueryTagHeap
@ stdcall RtlQueryTimeZoneInformation(ptr)
@ stub RtlQueueApcWow64Thread
@ stub RtlQueueWorkItem
@ stdcall RtlRaiseException(ptr)
@ stdcall RtlRaiseStatus(long)
@ stdcall RtlRandom(ptr)
@ stub RtlRandomEx
@ stdcall RtlReAllocateHeap(long long ptr long)
@ stub RtlReadMemoryStream
@ stub RtlReadOutOfProcessMemoryStream
@ stub RtlRealPredecessor
@ stub RtlRealSuccessor
@ stub RtlRegisterSecureMemoryCacheCallback
@ stub RtlRegisterWait
@ stub RtlReleaseActivationContext
@ stub RtlReleaseMemoryStream
@ stdcall RtlReleasePebLock()
@ stdcall RtlReleaseResource(ptr)
@ stub RtlRemoteCall
@ stdcall RtlRemoveVectoredExceptionHandler(ptr)
@ stub RtlResetRtlTranslations
@ stdcall RtlRestoreLastWin32Error(long) RtlSetLastWin32Error
@ stub RtlRevertMemoryStream
@ stub RtlRunDecodeUnicodeString
@ stub RtlRunEncodeUnicodeString
@ stdcall RtlSecondsSince1970ToTime(long ptr)
@ stdcall RtlSecondsSince1980ToTime(long ptr)
# @ stub RtlSeekMemoryStream
# @ stub RtlSelfRelativeToAbsoluteSD2
@ stdcall RtlSelfRelativeToAbsoluteSD(ptr ptr ptr ptr ptr ptr ptr ptr ptr ptr ptr)
@ stdcall RtlSetAllBits(ptr)
# @ stub RtlSetAttributesSecurityDescriptor
@ stdcall RtlSetBits(ptr long long)
# @ stub RtlSetControlSecurityDescriptor
@ stdcall RtlSetCriticalSectionSpinCount(ptr long)
@ stdcall RtlSetCurrentDirectory_U(ptr)
@ stdcall RtlSetCurrentEnvironment(wstr ptr)
@ stdcall RtlSetDaclSecurityDescriptor(ptr long ptr long)
@ stdcall RtlSetEnvironmentVariable(ptr ptr ptr)
@ stdcall RtlSetGroupSecurityDescriptor(ptr ptr long)
# @ stub RtlSetHeapInformation
@ stub RtlSetInformationAcl
# @ stub RtlSetIoCompletionCallback
@ stdcall RtlSetLastWin32Error(long)
@ stdcall RtlSetLastWin32ErrorAndNtStatusFromNtStatus(long)
# @ stub RtlSetMemoryStreamSize
@ stdcall RtlSetOwnerSecurityDescriptor(ptr ptr long)
# @ stub RtlSetProcessIsCritical
@ stub RtlSetProperties
@ stub RtlSetPropertyClassId
@ stub RtlSetPropertyNames
@ stub RtlSetPropertySetClassId
@ stdcall RtlSetSaclSecurityDescriptor(ptr long ptr long)
# @ stub RtlSetSecurityDescriptorRMControl
@ stub RtlSetSecurityObject
# @ stub RtlSetSecurityObjectEx
# @ stub RtlSetThreadIsCritical
# @ stub RtlSetThreadPoolStartFunc
@ stdcall RtlSetTimeZoneInformation(ptr)
# @ stub RtlSetTimer
@ stub RtlSetUnicodeCallouts
@ stub RtlSetUserFlagsHeap
@ stub RtlSetUserValueHeap
@ stdcall RtlSizeHeap(long long ptr)
@ stub RtlSplay
@ stub RtlStartRXact
# @ stub RtlStatMemoryStream
@ stdcall RtlStringFromGUID(ptr ptr)
@ stdcall RtlSubAuthorityCountSid(ptr)
@ stdcall RtlSubAuthoritySid(ptr long)
@ stub RtlSubtreePredecessor
@ stub RtlSubtreeSuccessor
@ stdcall RtlSystemTimeToLocalTime(ptr ptr)
@ stdcall RtlTimeFieldsToTime(ptr ptr)
@ stdcall RtlTimeToElapsedTimeFields(long long)
@ stdcall RtlTimeToSecondsSince1970(ptr ptr)
@ stdcall RtlTimeToSecondsSince1980(ptr ptr)
@ stdcall RtlTimeToTimeFields (long long)
# @ stub RtlTraceDatabaseAdd
# @ stub RtlTraceDatabaseCreate
# @ stub RtlTraceDatabaseDestroy
# @ stub RtlTraceDatabaseEnumerate
# @ stub RtlTraceDatabaseFind
# @ stub RtlTraceDatabaseLock
# @ stub RtlTraceDatabaseUnlock
# @ stub RtlTraceDatabaseValidate
@ stdcall RtlTryEnterCriticalSection(ptr)
@ cdecl -i386 -norelay RtlUlongByteSwap() NTDLL_RtlUlongByteSwap
@ cdecl -ret64 RtlUlonglongByteSwap(double)
# @ stub RtlUnhandledExceptionFilter2
# @ stub RtlUnhandledExceptionFilter
@ stdcall RtlUnicodeStringToAnsiSize(ptr)
@ stdcall RtlUnicodeStringToAnsiString(ptr ptr long)
@ stub RtlUnicodeStringToCountedOemString
@ stdcall RtlUnicodeStringToInteger(ptr long ptr)
@ stdcall RtlUnicodeStringToOemSize(ptr)
@ stdcall RtlUnicodeStringToOemString(ptr ptr long)
@ stub RtlUnicodeToCustomCPN
@ stdcall RtlUnicodeToMultiByteN(ptr long ptr ptr long)
@ stdcall RtlUnicodeToMultiByteSize(ptr ptr long)
@ stdcall RtlUnicodeToOemN(ptr long ptr ptr long)
@ stdcall RtlUniform(ptr)
# @ stub RtlUnlockBootStatusData
@ stdcall RtlUnlockHeap(long)
# @ stub RtlUnlockMemoryStreamRegion
@ stdcall RtlUnwind(ptr ptr ptr long)
@ stdcall RtlUpcaseUnicodeChar(long)
@ stdcall RtlUpcaseUnicodeString(ptr ptr long)
@ stdcall RtlUpcaseUnicodeStringToAnsiString(ptr ptr long)
@ stdcall RtlUpcaseUnicodeStringToCountedOemString(ptr ptr long)
@ stdcall RtlUpcaseUnicodeStringToOemString(ptr ptr long)
@ stub RtlUpcaseUnicodeToCustomCPN
@ stdcall RtlUpcaseUnicodeToMultiByteN(ptr long ptr ptr long)
@ stdcall RtlUpcaseUnicodeToOemN(ptr long ptr ptr long)
# @ stub RtlUpdateTimer
@ stdcall RtlUpperChar(long)
@ stdcall RtlUpperString(ptr ptr)
@ stub RtlUsageHeap
@ cdecl -i386 -norelay RtlUshortByteSwap() NTDLL_RtlUshortByteSwap
@ stdcall RtlValidAcl(ptr)
# @ stub RtlValidRelativeSecurityDescriptor
@ stdcall RtlValidSecurityDescriptor(ptr)
@ stdcall RtlValidSid(ptr)
@ stdcall RtlValidateHeap(long long ptr)
@ stub RtlValidateProcessHeaps
# @ stub RtlValidateUnicodeString
@ stdcall RtlVerifyVersionInfo(ptr long double)
@ stub RtlWalkFrameChain
@ stdcall RtlWalkHeap(long ptr)
@ stub RtlWriteMemoryStream
@ stub RtlWriteRegistryValue
@ stub RtlZeroHeap
@ stdcall RtlZeroMemory(ptr long)
# @ stub RtlZombifyActivationContext
# @ stub RtlpApplyLengthFunction
# @ stub RtlpEnsureBufferSize
# @ stub RtlpNotOwnerCriticalSection
@ stdcall RtlpNtCreateKey(ptr long ptr long ptr long long)
@ stdcall RtlpNtEnumerateSubKey(ptr ptr long)
@ stdcall RtlpNtMakeTemporaryKey(ptr)
@ stdcall RtlpNtOpenKey(ptr long ptr)
@ stdcall RtlpNtQueryValueKey(long ptr ptr ptr)
@ stdcall RtlpNtSetValueKey(ptr long ptr long)
@ stdcall RtlpUnWaitCriticalSection(ptr)
@ stdcall RtlpWaitForCriticalSection(ptr)
@ stdcall RtlxAnsiStringToUnicodeSize(ptr) RtlAnsiStringToUnicodeSize
@ stdcall RtlxOemStringToUnicodeSize(ptr) RtlOemStringToUnicodeSize
@ stdcall RtlxUnicodeStringToAnsiSize(ptr) RtlUnicodeStringToAnsiSize
@ stdcall RtlxUnicodeStringToOemSize(ptr) RtlUnicodeStringToOemSize
@ stdcall -ret64 VerSetConditionMask(double long long)
@ stdcall ZwAcceptConnectPort(ptr long ptr long long ptr) NtAcceptConnectPort
@ stdcall ZwAccessCheck(ptr long long ptr ptr ptr ptr ptr) NtAccessCheck
@ stdcall ZwAccessCheckAndAuditAlarm(ptr long ptr ptr ptr long ptr long ptr ptr ptr) NtAccessCheckAndAuditAlarm
# @ stub ZwAccessCheckByType
# @ stub ZwAccessCheckByTypeAndAuditAlarm
# @ stub ZwAccessCheckByTypeResultList
# @ stub ZwAccessCheckByTypeResultListAndAuditAlarm
# @ stub ZwAccessCheckByTypeResultListAndAuditAlarmByHandle
@ stdcall ZwAddAtom(ptr long ptr) NtAddAtom
# @ stub ZwAddBootEntry
@ stdcall ZwAdjustGroupsToken(long long long long long long) NtAdjustGroupsToken
@ stdcall ZwAdjustPrivilegesToken(long long long long long long) NtAdjustPrivilegesToken
@ stdcall ZwAlertResumeThread(long ptr) NtAlertResumeThread
@ stdcall ZwAlertThread(long) NtAlertThread
@ stdcall ZwAllocateLocallyUniqueId(ptr) NtAllocateLocallyUniqueId
# @ stub ZwAllocateUserPhysicalPages
@ stdcall ZwAllocateUuids(ptr ptr ptr) NtAllocateUuids
@ stdcall ZwAllocateVirtualMemory(long ptr ptr ptr long long) NtAllocateVirtualMemory
# @ stub ZwAreMappedFilesTheSame
# @ stub ZwAssignProcessToJobObject
@ stub ZwCallbackReturn
# @ stub ZwCancelDeviceWakeupRequest
@ stdcall ZwCancelIoFile(long ptr) NtCancelIoFile
@ stdcall ZwCancelTimer(long ptr) NtCancelTimer
@ stdcall ZwClearEvent(long) NtClearEvent
@ stdcall ZwClose(long) NtClose
@ stub ZwCloseObjectAuditAlarm
# @ stub ZwCompactKeys
# @ stub ZwCompareTokens
@ stdcall ZwCompleteConnectPort(ptr) NtCompleteConnectPort
# @ stub ZwCompressKey
@ stdcall ZwConnectPort(ptr ptr ptr ptr ptr ptr ptr ptr) NtConnectPort
@ stub ZwContinue
# @ stub ZwCreateDebugObject
@ stdcall ZwCreateDirectoryObject(long long long) NtCreateDirectoryObject
@ stdcall ZwCreateEvent(long long long long long) NtCreateEvent
@ stub ZwCreateEventPair
@ stdcall ZwCreateFile(ptr long ptr ptr long long long ptr long long ptr) NtCreateFile
@ stub ZwCreateIoCompletion
# @ stub ZwCreateJobObject
# @ stub ZwCreateJobSet
@ stdcall ZwCreateKey(ptr long ptr long ptr long long) NtCreateKey
# @ stub ZwCreateKeyedEvent
@ stdcall ZwCreateMailslotFile(long long long long long long long long) NtCreateMailslotFile
@ stdcall ZwCreateMutant(ptr long ptr long) NtCreateMutant
@ stdcall ZwCreateNamedPipeFile(ptr long ptr ptr long long long long long long long long long ptr) NtCreateNamedPipeFile
@ stdcall ZwCreatePagingFile(long long long long) NtCreatePagingFile
@ stdcall ZwCreatePort(ptr ptr long long long) NtCreatePort
@ stub ZwCreateProcess
# @ stub ZwCreateProcessEx
@ stub ZwCreateProfile
@ stdcall ZwCreateSection(ptr long ptr ptr long long long) NtCreateSection
@ stdcall ZwCreateSemaphore(ptr long ptr long long) NtCreateSemaphore
@ stdcall ZwCreateSymbolicLinkObject(ptr long ptr ptr) NtCreateSymbolicLinkObject
@ stub ZwCreateThread
@ stdcall ZwCreateTimer(ptr long ptr long) NtCreateTimer
@ stub ZwCreateToken
# @ stub ZwCreateWaitablePort
# @ stub ZwDebugActiveProcess
# @ stub ZwDebugContinue
@ stdcall ZwDelayExecution(long ptr) NtDelayExecution
@ stdcall ZwDeleteAtom(long) NtDeleteAtom
# @ stub ZwDeleteBootEntry
@ stdcall ZwDeleteFile(ptr) NtDeleteFile
@ stdcall ZwDeleteKey(long) NtDeleteKey
# @ stub ZwDeleteObjectAuditAlarm
@ stdcall ZwDeleteValueKey(long ptr) NtDeleteValueKey
@ stdcall ZwDeviceIoControlFile(long long long long long long long long long long) NtDeviceIoControlFile
@ stdcall ZwDisplayString(ptr) NtDisplayString
@ stdcall ZwDuplicateObject(long long long ptr long long long) NtDuplicateObject
@ stdcall ZwDuplicateToken(long long long long long long) NtDuplicateToken
# @ stub ZwEnumerateBootEntries
@ stub ZwEnumerateBus
@ stdcall ZwEnumerateKey(long long long ptr long ptr) NtEnumerateKey
# @ stub ZwEnumerateSystemEnvironmentValuesEx
@ stdcall ZwEnumerateValueKey(long long long ptr long ptr) NtEnumerateValueKey
@ stub ZwExtendSection
# @ stub ZwFilterToken
@ stdcall ZwFindAtom(ptr long ptr) NtFindAtom
@ stdcall ZwFlushBuffersFile(long ptr) NtFlushBuffersFile
@ stdcall ZwFlushInstructionCache(long ptr long) NtFlushInstructionCache
@ stdcall ZwFlushKey(long) NtFlushKey
@ stdcall ZwFlushVirtualMemory(long ptr ptr long) NtFlushVirtualMemory
@ stub ZwFlushWriteBuffer
# @ stub ZwFreeUserPhysicalPages
@ stdcall ZwFreeVirtualMemory(long ptr ptr long) NtFreeVirtualMemory
@ stdcall ZwFsControlFile(long long long long long long long long long long) NtFsControlFile
@ stdcall ZwGetContextThread(long ptr) NtGetContextThread
# @ stub ZwGetDevicePowerState
@ stub ZwGetPlugPlayEvent
@ stub ZwGetTickCount
# @ stub ZwGetWriteWatch
# @ stub ZwImpersonateAnonymousToken
@ stub ZwImpersonateClientOfPort
@ stub ZwImpersonateThread
@ stub ZwInitializeRegistry
# @ stub ZwInitiatePowerAction
# @ stub ZwIsProcessInJob
# @ stub ZwIsSystemResumeAutomatic
@ stdcall ZwListenPort(ptr ptr) NtListenPort
@ stdcall ZwLoadDriver(ptr) NtLoadDriver
# @ stub ZwLoadKey2
@ stdcall ZwLoadKey(ptr ptr) NtLoadKey
@ stdcall ZwLockFile(long long ptr ptr ptr ptr ptr ptr long long) NtLockFile
# @ stub ZwLockProductActivationKeys
# @ stub ZwLockRegistryKey
@ stdcall ZwLockVirtualMemory(long ptr ptr long) NtLockVirtualMemory
# @ stub ZwMakePermanentObject
@ stub ZwMakeTemporaryObject
# @ stub ZwMapUserPhysicalPages
# @ stub ZwMapUserPhysicalPagesScatter
@ stdcall ZwMapViewOfSection(long long ptr long long ptr ptr long long long) NtMapViewOfSection
# @ stub ZwModifyBootEntry
@ stub ZwNotifyChangeDirectoryFile
@ stdcall ZwNotifyChangeKey(long long ptr ptr ptr long long ptr long long) NtNotifyChangeKey
# @ stub ZwNotifyChangeMultipleKeys
@ stdcall ZwOpenDirectoryObject(long long long) NtOpenDirectoryObject
@ stdcall ZwOpenEvent(long long long) NtOpenEvent
@ stub ZwOpenEventPair
@ stdcall ZwOpenFile(ptr long ptr ptr long long) NtOpenFile
@ stub ZwOpenIoCompletion
# @ stub ZwOpenJobObject
@ stdcall ZwOpenKey(ptr long ptr) NtOpenKey
# @ stub ZwOpenKeyedEvent
@ stdcall ZwOpenMutant(ptr long ptr) NtOpenMutant
@ stub ZwOpenObjectAuditAlarm
@ stdcall ZwOpenProcess(ptr long ptr ptr) NtOpenProcess
@ stdcall ZwOpenProcessToken(long long long) NtOpenProcessToken
# @ stub ZwOpenProcessTokenEx
@ stdcall ZwOpenSection(ptr long ptr) NtOpenSection
@ stdcall ZwOpenSemaphore(long long ptr) NtOpenSemaphore
@ stdcall ZwOpenSymbolicLinkObject (ptr long ptr) NtOpenSymbolicLinkObject
@ stdcall ZwOpenThread(ptr long ptr ptr) NtOpenThread
@ stdcall ZwOpenThreadToken(long long long long) NtOpenThreadToken
# @ stub ZwOpenThreadTokenEx
@ stdcall ZwOpenTimer(ptr long ptr) NtOpenTimer
@ stub ZwPlugPlayControl
# @ stub ZwPowerInformation
@ stdcall ZwPrivilegeCheck(ptr ptr ptr) NtPrivilegeCheck
@ stub ZwPrivilegeObjectAuditAlarm
@ stub ZwPrivilegedServiceAuditAlarm
@ stdcall ZwProtectVirtualMemory(long ptr ptr long ptr) NtProtectVirtualMemory
@ stdcall ZwPulseEvent(long ptr) NtPulseEvent
@ stdcall ZwQueryAttributesFile(ptr ptr) NtQueryAttributesFile
# @ stub ZwQueryBootEntryOrder
# @ stub ZwQueryBootOptions
# @ stub ZwQueryDebugFilterState
@ stdcall ZwQueryDefaultLocale(long ptr) NtQueryDefaultLocale
@ stdcall ZwQueryDefaultUILanguage(ptr) NtQueryDefaultUILanguage
@ stdcall ZwQueryDirectoryFile(long long ptr ptr ptr ptr long long long ptr long) NtQueryDirectoryFile
@ stdcall ZwQueryDirectoryObject(long ptr long long long ptr ptr) NtQueryDirectoryObject
@ stub ZwQueryEaFile
@ stdcall ZwQueryEvent(long long ptr long ptr) NtQueryEvent
# @ stub ZwQueryFullAttributesFile
@ stdcall ZwQueryInformationAtom(long long ptr long ptr) NtQueryInformationAtom
@ stdcall ZwQueryInformationFile(long ptr ptr long long) NtQueryInformationFile
# @ stub ZwQueryInformationJobObject
@ stub ZwQueryInformationPort
@ stdcall ZwQueryInformationProcess(long long ptr long ptr) NtQueryInformationProcess
@ stdcall ZwQueryInformationThread(long long ptr long ptr) NtQueryInformationThread
@ stdcall ZwQueryInformationToken(long long ptr long ptr) NtQueryInformationToken
@ stdcall ZwQueryInstallUILanguage(ptr) NtQueryInstallUILanguage
@ stub ZwQueryIntervalProfile
@ stub ZwQueryIoCompletion
@ stdcall ZwQueryKey(long long ptr long ptr) NtQueryKey
# @ stub ZwQueryMultipleValueKey
@ stdcall ZwQueryMutant(long long ptr long ptr) NtQueryMutant
@ stdcall ZwQueryObject(long long long long long) NtQueryObject
@ stub ZwQueryOpenSubKeys
@ stdcall ZwQueryPerformanceCounter (long long) NtQueryPerformanceCounter
# @ stub ZwQueryPortInformationProcess
# @ stub ZwQueryQuotaInformationFile
@ stdcall ZwQuerySection (long long long long long) NtQuerySection
@ stdcall ZwQuerySecurityObject (long long long long long) NtQuerySecurityObject
@ stdcall ZwQuerySemaphore (long long long long long) NtQuerySemaphore
@ stdcall ZwQuerySymbolicLinkObject(long ptr ptr) NtQuerySymbolicLinkObject
@ stub ZwQuerySystemEnvironmentValue
# @ stub ZwQuerySystemEnvironmentValueEx
@ stdcall ZwQuerySystemInformation(long long long long) NtQuerySystemInformation
@ stdcall ZwQuerySystemTime(ptr) NtQuerySystemTime
@ stdcall ZwQueryTimer(ptr long ptr long ptr) NtQueryTimer
@ stdcall ZwQueryTimerResolution(long long long) NtQueryTimerResolution
@ stdcall ZwQueryValueKey(long ptr long ptr long ptr) NtQueryValueKey
@ stdcall ZwQueryVirtualMemory(long ptr long ptr long ptr) NtQueryVirtualMemory
@ stdcall ZwQueryVolumeInformationFile(long ptr ptr long long) NtQueryVolumeInformationFile
# @ stub ZwQueueApcThread
@ stdcall ZwRaiseException(ptr ptr long) NtRaiseException
@ stub ZwRaiseHardError
@ stdcall ZwReadFile(long long long long long long long long long) NtReadFile
# @ stub ZwReadFileScatter
@ stub ZwReadRequestData
@ stdcall ZwReadVirtualMemory(long ptr ptr long ptr) NtReadVirtualMemory
@ stub ZwRegisterNewDevice
@ stdcall ZwRegisterThreadTerminatePort(ptr) NtRegisterThreadTerminatePort
# @ stub ZwReleaseKeyedEvent
@ stdcall ZwReleaseMutant(long ptr) NtReleaseMutant
@ stub ZwReleaseProcessMutant
@ stdcall ZwReleaseSemaphore(long long ptr) NtReleaseSemaphore
@ stub ZwRemoveIoCompletion
# @ stub ZwRemoveProcessDebug
# @ stub ZwRenameKey
@ stdcall ZwReplaceKey(ptr long ptr) NtReplaceKey
@ stub ZwReplyPort
@ stdcall ZwReplyWaitReceivePort(ptr ptr ptr ptr) NtReplyWaitReceivePort
# @ stub ZwReplyWaitReceivePortEx
@ stub ZwReplyWaitReplyPort
# @ stub ZwRequestDeviceWakeup
@ stub ZwRequestPort
@ stdcall ZwRequestWaitReplyPort(ptr ptr ptr) NtRequestWaitReplyPort
# @ stub ZwRequestWakeupLatency
@ stdcall ZwResetEvent(long ptr) NtResetEvent
# @ stub ZwResetWriteWatch
@ stdcall ZwRestoreKey(long long long) NtRestoreKey
# @ stub ZwResumeProcess
@ stdcall ZwResumeThread(long long) NtResumeThread
@ stdcall ZwSaveKey(long long) NtSaveKey
# @ stub ZwSaveKeyEx
# @ stub ZwSaveMergedKeys
# @ stub ZwSecureConnectPort
# @ stub ZwSetBootEntryOrder
# @ stub ZwSetBootOptions
@ stdcall ZwSetContextThread(long ptr) NtSetContextThread
# @ stub ZwSetDebugFilterState
@ stub ZwSetDefaultHardErrorPort
@ stdcall ZwSetDefaultLocale(long long) NtSetDefaultLocale
@ stdcall ZwSetDefaultUILanguage(long) NtSetDefaultUILanguage
@ stub ZwSetEaFile
@ stdcall ZwSetEvent(long long) NtSetEvent
# @ stub ZwSetEventBoostPriority
@ stub ZwSetHighEventPair
@ stub ZwSetHighWaitLowEventPair
@ stub ZwSetHighWaitLowThread
# @ stub ZwSetInformationDebugObject
@ stdcall ZwSetInformationFile(long long long long long) NtSetInformationFile
# @ stub ZwSetInformationJobObject
@ stdcall ZwSetInformationKey(long long ptr long) NtSetInformationKey
@ stdcall ZwSetInformationObject(long long ptr long) NtSetInformationObject
@ stdcall ZwSetInformationProcess(long long long long) NtSetInformationProcess
@ stdcall ZwSetInformationThread(long long ptr long) NtSetInformationThread
@ stdcall ZwSetInformationToken(long long ptr long) NtSetInformationToken
@ stdcall ZwSetIntervalProfile(long long) NtSetIntervalProfile
@ stub ZwSetIoCompletion
@ stub ZwSetLdtEntries
@ stub ZwSetLowEventPair
@ stub ZwSetLowWaitHighEventPair
@ stub ZwSetLowWaitHighThread
# @ stub ZwSetQuotaInformationFile
@ stdcall ZwSetSecurityObject(long long ptr) NtSetSecurityObject
@ stub ZwSetSystemEnvironmentValue
# @ stub ZwSetSystemEnvironmentValueEx
@ stub ZwSetSystemInformation
@ stub ZwSetSystemPowerState
@ stdcall ZwSetSystemTime(ptr ptr) NtSetSystemTime
# @ stub ZwSetThreadExecutionState
@ stdcall ZwSetTimer(long ptr ptr ptr long long ptr) NtSetTimer
@ stdcall ZwSetTimerResolution(long long ptr) NtSetTimerResolution
# @ stub ZwSetUuidSeed
@ stdcall ZwSetValueKey(long long long long long long) NtSetValueKey
@ stdcall ZwSetVolumeInformationFile(long ptr ptr long long) NtSetVolumeInformationFile
@ stdcall ZwShutdownSystem(long) NtShutdownSystem
# @ stub ZwSignalAndWaitForSingleObject
@ stub ZwStartProfile
@ stub ZwStopProfile
# @ stub ZwSuspendProcess
@ stdcall ZwSuspendThread(long ptr) NtSuspendThread
@ stub ZwSystemDebugControl
# @ stub ZwTerminateJobObject
@ stdcall ZwTerminateProcess(long long) NtTerminateProcess
@ stdcall ZwTerminateThread(long long) NtTerminateThread
@ stub ZwTestAlert
# @ stub ZwTraceEvent
# @ stub ZwTranslateFilePath
@ stdcall ZwUnloadDriver(ptr) NtUnloadDriver
@ stdcall ZwUnloadKey(long) NtUnloadKey
@ stub ZwUnloadKeyEx
@ stdcall ZwUnlockFile(long ptr ptr ptr ptr) NtUnlockFile
@ stdcall ZwUnlockVirtualMemory(long ptr ptr long) NtUnlockVirtualMemory
@ stdcall ZwUnmapViewOfSection(long ptr) NtUnmapViewOfSection
@ stub ZwVdmControl
@ stub ZwW32Call
# @ stub ZwWaitForDebugEvent
# @ stub ZwWaitForKeyedEvent
@ stdcall ZwWaitForMultipleObjects(long ptr long long ptr) NtWaitForMultipleObjects
@ stub ZwWaitForProcessMutant
@ stdcall ZwWaitForSingleObject(long long long) NtWaitForSingleObject
@ stub ZwWaitHighEventPair
@ stub ZwWaitLowEventPair
@ stdcall ZwWriteFile(long long ptr ptr ptr ptr long ptr ptr) NtWriteFile
# @ stub ZwWriteFileGather
@ stub ZwWriteRequestData
@ stdcall ZwWriteVirtualMemory(long ptr ptr long ptr) NtWriteVirtualMemory
@ stdcall ZwYieldExecution() NtYieldExecution
# @ stub _CIcos
# @ stub _CIlog
@ cdecl _CIpow() NTDLL__CIpow
# @ stub _CIsin
# @ stub _CIsqrt
# @ stub __isascii
# @ stub __iscsym
# @ stub __iscsymf
# @ stub __toascii
@ stdcall -ret64 _alldiv(double double)
# @ stub _alldvrm
@ stdcall -ret64 _allmul(double double)
@ stdcall -i386 _alloca_probe()
@ stdcall -ret64 _allrem(double double)
# @ stub _allshl
# @ stub _allshr
@ cdecl -ret64 _atoi64(str)
@ stdcall -ret64 _aulldiv(double double)
# @ stub _aulldvrm
@ stdcall -ret64 _aullrem(double double)
# @ stub _aullshr
@ stdcall -i386 _chkstk()
@ stub _fltused
@ cdecl -ret64 _ftol() NTDLL__ftol
@ cdecl _i64toa(double ptr long)
@ cdecl _i64tow(double ptr long)
@ cdecl _itoa(long ptr long)
@ cdecl _itow(long ptr long)
@ cdecl _lfind(ptr ptr ptr long ptr) lfind
@ cdecl _ltoa(long ptr long)
@ cdecl _ltow(long ptr long)
@ cdecl _memccpy(ptr ptr long long) memccpy
@ cdecl _memicmp(str str long) NTDLL__memicmp
@ varargs _snprintf(ptr long ptr) snprintf
@ varargs _snwprintf(wstr long wstr)
@ cdecl _splitpath(str ptr ptr ptr ptr)
@ cdecl _strcmpi(str str) strcasecmp
@ cdecl _stricmp(str str) strcasecmp
@ cdecl _strlwr(str)
@ cdecl _strnicmp(str str long) strncasecmp
@ cdecl _strupr(str)
# @ stub _tolower
# @ stub _toupper
@ cdecl _ui64toa(double ptr long)
@ cdecl _ui64tow(double ptr long)
@ cdecl _ultoa(long ptr long)
@ cdecl _ultow(long ptr long)
@ cdecl _vsnprintf(ptr long str ptr) vsnprintf
@ cdecl _vsnwprintf(ptr long wstr ptr) vsnprintfW
@ cdecl _wcsicmp(wstr wstr) NTDLL__wcsicmp
@ cdecl _wcslwr(wstr) NTDLL__wcslwr
@ cdecl _wcsnicmp(wstr wstr long) NTDLL__wcsnicmp
@ cdecl _wcsupr(wstr) NTDLL__wcsupr
@ cdecl _wtoi(wstr)
@ cdecl _wtoi64(wstr)
@ cdecl _wtol(wstr)
@ cdecl abs(long)
@ cdecl atan(double)
@ cdecl atoi(str)
@ cdecl atol(str)
@ cdecl bsearch(ptr ptr long long ptr)
@ cdecl ceil(double)
@ cdecl cos(double)
@ cdecl fabs(double)
@ cdecl floor(double)
@ cdecl isalnum(long)
@ cdecl isalpha(long)
@ cdecl iscntrl(long)
@ cdecl isdigit(long)
@ cdecl isgraph(long)
@ cdecl islower(long)
@ cdecl isprint(long)
@ cdecl ispunct(long)
@ cdecl isspace(long)
@ cdecl isupper(long)
@ cdecl iswalpha(long) NTDLL_iswalpha
@ cdecl iswctype(long long) NTDLL_iswctype
@ cdecl iswdigit(long) NTDLL_iswdigit
@ cdecl iswlower(long) NTDLL_iswlower
@ cdecl iswspace(long) NTDLL_iswspace
@ cdecl iswxdigit(long) NTDLL_iswxdigit
@ cdecl isxdigit(long)
@ cdecl labs(long)
@ cdecl log(double)
@ cdecl mbstowcs(ptr str long) NTDLL_mbstowcs
@ cdecl memchr(ptr long long)
@ cdecl memcmp(ptr ptr long)
@ cdecl memcpy(ptr ptr long)
@ cdecl memmove(ptr ptr long)
@ cdecl memset(ptr long long)
@ cdecl pow(double double)
@ cdecl qsort(ptr long long ptr)
@ cdecl sin(double)
@ varargs sprintf(str str)
@ cdecl sqrt(double)
@ varargs sscanf(str str)
@ cdecl strcat(str str)
@ cdecl strchr(str long)
@ cdecl strcmp(str str)
@ cdecl strcpy(ptr str)
@ cdecl strcspn(str str)
@ cdecl strlen(str)
@ cdecl strncat(str str long)
@ cdecl strncmp(str str long)
@ cdecl strncpy(ptr str long)
@ cdecl strpbrk(str str)
@ cdecl strrchr(str long)
@ cdecl strspn(str str)
@ cdecl strstr(str str)
@ cdecl strtol(str ptr long)
@ cdecl strtoul(str ptr long)
@ varargs swprintf(wstr wstr) NTDLL_swprintf
@ cdecl tan(double)
@ cdecl tolower(long)
@ cdecl toupper(long)
@ cdecl towlower(long) NTDLL_towlower
@ cdecl towupper(long) NTDLL_towupper
@ stdcall vDbgPrintEx(long long str ptr)
@ stdcall vDbgPrintExWithPrefix(str long long str ptr)
@ cdecl vsprintf(ptr str ptr)
@ cdecl wcscat(wstr wstr) NTDLL_wcscat
@ cdecl wcschr(wstr long) NTDLL_wcschr
@ cdecl wcscmp(wstr wstr) NTDLL_wcscmp
@ cdecl wcscpy(ptr wstr) NTDLL_wcscpy
@ cdecl wcscspn(wstr wstr) NTDLL_wcscspn
@ cdecl wcslen(wstr) NTDLL_wcslen
@ cdecl wcsncat(wstr wstr long) NTDLL_wcsncat
@ cdecl wcsncmp(wstr wstr long) NTDLL_wcsncmp
@ cdecl wcsncpy(ptr wstr long) NTDLL_wcsncpy
@ cdecl wcspbrk(wstr wstr) NTDLL_wcspbrk
@ cdecl wcsrchr(wstr long) NTDLL_wcsrchr
@ cdecl wcsspn(wstr wstr) NTDLL_wcsspn
@ cdecl wcsstr(wstr wstr) NTDLL_wcsstr
@ cdecl wcstok(wstr wstr) NTDLL_wcstok
@ cdecl wcstol(wstr ptr long) NTDLL_wcstol
@ cdecl wcstombs(ptr ptr long) NTDLL_wcstombs
@ cdecl wcstoul(wstr ptr long) NTDLL_wcstoul

##################
# Wine extensions
#
# All functions must be prefixed with '__wine_' (for internal functions)
# or 'wine_' (for user-visible functions) to avoid namespace conflicts.

# Exception handling
@ cdecl -norelay __wine_exception_handler(ptr ptr ptr ptr)
@ cdecl -norelay __wine_finally_handler(ptr ptr ptr ptr)

# Relays
@ cdecl -norelay -i386 __wine_call_from_32_regs()
@ cdecl -i386 __wine_enter_vm86(ptr)

# Server interface
@ cdecl -norelay wine_server_call(ptr)
@ cdecl wine_server_fd_to_handle(long long long ptr)
@ cdecl wine_server_handle_to_fd(long long ptr ptr)
@ cdecl wine_server_release_fd(long long)
@ cdecl wine_server_send_fd(long)

# Codepages
@ cdecl __wine_init_codepages(ptr ptr ptr)

# signal handling
@ cdecl __wine_set_signal_handler(long ptr)

# Filesystem
@ cdecl wine_nt_to_unix_file_name(ptr ptr long long)
@ cdecl wine_unix_to_nt_file_name(ptr ptr)
@ cdecl __wine_init_windows_dir(wstr wstr)

################################################################
# Wine dll separation hacks, these will go away, don't use them
#
@ cdecl MODULE_DllThreadAttach(ptr)
@ cdecl MODULE_GetLoadOrderW(ptr wstr wstr)
