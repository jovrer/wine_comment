@ stdcall BindImage(str str str)
@ stdcall BindImageEx(long str str str ptr)
@ stdcall CheckSumMappedFile(ptr long ptr ptr)
@ stub EnumerateLoadedModules64
@ stdcall EnumerateLoadedModules(long ptr ptr) dbghelp.EnumerateLoadedModules
@ stdcall FindDebugInfoFile(str str str) dbghelp.FindDebugInfoFile
@ stdcall FindDebugInfoFileEx(str str ptr ptr ptr) dbghelp.FindDebugInfoFileEx
@ stdcall FindExecutableImage(str str str) dbghelp.FindExecutableImage
@ stub FindExecutableImageEx
@ stub FindFileInPath
@ stub FindFileInSearchPath
@ stdcall GetImageConfigInformation(ptr ptr)
@ stdcall GetImageUnusedHeaderBytes(ptr ptr)
@ stdcall GetTimestampForLoadedLibrary(long) dbghelp.GetTimestampForLoadedLibrary
@ stdcall ImageAddCertificate(long ptr ptr)
@ stdcall ImageDirectoryEntryToData(ptr long long ptr) ntdll.RtlImageDirectoryEntryToData
@ stub ImageDirectoryEntryToDataEx
@ stdcall ImageEnumerateCertificates(long long ptr ptr long)
@ stdcall ImageGetCertificateData(long long ptr ptr)
@ stdcall ImageGetCertificateHeader(long long ptr)
@ stdcall ImageGetDigestStream(long long ptr long)
@ stdcall ImageLoad(str str)
@ stdcall ImageNtHeader(ptr) ntdll.RtlImageNtHeader
@ stdcall ImageRemoveCertificate(long long)
@ stdcall ImageRvaToSection(ptr ptr long) ntdll.RtlImageRvaToSection
@ stdcall ImageRvaToVa(ptr ptr long ptr) ntdll.RtlImageRvaToVa
@ stdcall ImageUnload(ptr)
@ stdcall ImagehlpApiVersion() dbghelp.ImagehlpApiVersion
@ stdcall ImagehlpApiVersionEx(ptr) dbghelp.ImagehlpApiVersionEx
@ stdcall MakeSureDirectoryPathExists(str) dbghelp.MakeSureDirectoryPathExists
@ stdcall MapAndLoad(str str ptr long long)
@ stdcall MapDebugInformation(long str str long) dbghelp.MapDebugInformation
@ stdcall MapFileAndCheckSumA(str ptr ptr)
@ stdcall MapFileAndCheckSumW(wstr ptr ptr)
@ stub  MarkImageAsRunFromSwap
@ stub ReBaseImage64
@ stdcall ReBaseImage(str str long long long long ptr ptr ptr ptr long)
@ stdcall RemovePrivateCvSymbolic(ptr ptr ptr)
@ stub RemovePrivateCvSymbolicEx
@ stdcall RemoveRelocations(ptr)
@ stdcall SearchTreeForFile(str str str) dbghelp.SearchTreeForFile
@ stdcall SetImageConfigInformation(ptr ptr)
@ stdcall SplitSymbols(str str str long)
@ stub StackWalk64
@ stdcall StackWalk(long long long ptr ptr ptr ptr ptr ptr) dbghelp.StackWalk
@ stdcall SymCleanup(long) dbghelp.SymCleanup
@ stdcall SymEnumSourceFiles(long long str ptr ptr) dbghelp.SymEnumSourceFiles
@ stub SymEnumSym
@ stdcall SymEnumSymbols(long long str ptr ptr) dbghelp.SymEnumSymbols
@ stdcall SymEnumTypes(long long ptr ptr) dbghelp.SymEnumTypes
@ stub SymEnumerateModules64
@ stdcall SymEnumerateModules(long ptr ptr) dbghelp.SymEnumerateModules
@ stub SymEnumerateSymbols64
@ stdcall SymEnumerateSymbols(long long ptr ptr) dbghelp.SymEnumerateSymbols
@ stub SymEnumerateSymbolsW64
@ stub SymEnumerateSymbolsW
@ stub SymFindFileInPath
@ stdcall SymFromAddr(long long ptr ptr) dbghelp.SymFromAddr
@ stdcall SymFromName(long str ptr) dbghelp.SymFromName
@ stub SymFunctionTableAccess64
@ stdcall SymFunctionTableAccess(long long) dbghelp.SymFunctionTableAccess
@ stub SymGetLineFromAddr64
@ stdcall SymGetLineFromAddr(long long ptr ptr) dbghelp.SymGetLineFromAddr
@ stub SymGetLineFromName64
@ stub SymGetLineFromName
@ stub SymGetLineNext64
@ stdcall SymGetLineNext(long ptr) dbghelp.SymGetLineNext
@ stub SymGetLinePrev64
@ stdcall SymGetLinePrev(long ptr) dbghelp.SymGetLinePrev
@ stub SymGetModuleBase64
@ stdcall SymGetModuleBase(long long) dbghelp.SymGetModuleBase
@ stub SymGetModuleInfo64
@ stdcall SymGetModuleInfo(long long ptr) dbghelp.SymGetModuleInfo
@ stub SymGetModuleInfoW64
@ stub SymGetModuleInfoW
@ stdcall SymGetOptions() dbghelp.SymGetOptions
@ stdcall SymGetSearchPath(long str long) dbghelp.SymGetSearchPath
@ stub SymGetSymFromAddr64
@ stdcall SymGetSymFromAddr(long long ptr ptr) dbghelp.SymGetSymFromAddr
@ stub SymGetSymFromName64
@ stdcall SymGetSymFromName(long str ptr) dbghelp.SymGetSymFromName
@ stub SymGetSymNext64
@ stdcall SymGetSymNext(long ptr) dbghelp.SymGetSymNext
@ stub SymGetSymPrev64
@ stdcall SymGetSymPrev(long ptr) dbghelp.SymGetSymPrev
@ stdcall SymGetTypeFromName(long long str ptr) dbghelp.SymGetTypeFromName
@ stdcall SymGetTypeInfo(long long long long ptr) dbghelp.SymGetTypeInfo
@ stdcall SymInitialize(long str long) dbghelp.SymInitialize
@ stub SymLoadModule64
@ stdcall SymLoadModule(long long str str long long) dbghelp.SymLoadModule
@ stub SymMatchFileName
@ stub SymMatchString
@ stub SymRegisterCallback64
@ stdcall SymRegisterCallback(long ptr ptr) dbghelp.SymRegisterCallback
@ stub SymRegisterFunctionEntryCallback64
@ stub SymRegisterFunctionEntryCallback
@ stdcall SymSetContext(long ptr ptr) dbghelp.SymSetContext
@ stdcall SymSetOptions(long) dbghelp.SymGetOptions
@ stdcall SymSetSearchPath(long str) dbghelp.SymSetSearchPath
@ stub SymUnDName64
@ stdcall SymUnDName(ptr str long) dbghelp.SymUnDName
@ stub SymUnloadModule64
@ stdcall SymUnloadModule(long long) dbghelp.SymUnloadModule
@ stdcall TouchFileTimes(long ptr)
@ stdcall UnDecorateSymbolName(str str long long) dbghelp.UnDecorateSymbolName
@ stdcall UnMapAndLoad(ptr)
@ stdcall UnmapDebugInformation(ptr) dbghelp.UnmapDebugInformation
@ stdcall UpdateDebugInfoFile(str str str ptr)
@ stdcall UpdateDebugInfoFileEx(str str str ptr long)
