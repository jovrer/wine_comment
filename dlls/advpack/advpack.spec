@ stub AddDelBackupEntry
@ stub AdvInstallFile
@ stub CloseINFEngine
@ stdcall DelNode(str long)
@ stdcall DelNodeRunDLL32(ptr ptr str long)
@ stdcall -private DllMain(long long ptr)
@ stdcall DoInfInstall(ptr)
@ stdcall ExecuteCab(ptr ptr ptr)
@ stdcall ExtractFiles(str str long ptr ptr long)
@ stub FileSaveMarkNotExist
@ stub FileSaveRestore
@ stub FileSaveRestoreOnINF
@ stdcall GetVersionFromFile(str ptr ptr long)
@ stdcall GetVersionFromFileEx(str ptr ptr long)
@ stdcall IsNTAdmin(long ptr)
@ stdcall LaunchINFSection(ptr ptr str long)
@ stdcall LaunchINFSectionEx(ptr ptr str long)
@ stdcall NeedReboot(long)
@ stdcall NeedRebootInit()
@ stub OpenINFEngine
@ stub RebootCheckOnInstall
@ stdcall RegInstall(ptr str ptr)
@ stub RegRestoreAll
@ stub RegSaveRestore
@ stub RegSaveRestoreOnINF
@ stdcall RegisterOCX(ptr ptr str long)
@ stdcall RunSetupCommand(long str str str str ptr long ptr)
@ stub SetPerUserSecValues
@ stdcall TranslateInfString(str str str str ptr long ptr ptr)
@ stub TranslateInfStringEx
@ stub UserInstStubWrapper
@ stub UserUnInstStubWrapper
