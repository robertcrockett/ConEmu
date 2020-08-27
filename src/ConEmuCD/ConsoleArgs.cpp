﻿/*
Copyright (c) 2015-present Maximus5
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the authors may not be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ''AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#define HIDE_USE_EXCEPTION_INFO
#define SHOWDEBUGSTR
#define DEBUGSTRCMD(x) DEBUGSTR(x)

#ifdef _DEBUG
//#define SHOW_ATTACH_MSGBOX
//#define SHOW_LOADCFGFILE_MSGBOX
//#define SHOW_SERVER_STARTED_MSGBOX
//#define SHOW_COMSPEC_STARTED_MSGBOX
#endif

#include "ConsoleArgs.h"

#include "Actions.h"
#include "ConsoleMain.h"
#include "ExitCodes.h"

#include "../common/CmdLine.h"
#include "../common/MStrDup.h"
#include "../common/RConStartArgs.h"

#include <tuple>

#include "ConsoleState.h"


ConsoleArgs* gpConsoleArgs = nullptr;


Switch::Switch(const SwitchType aType)
	: switchType(aType)
{
}

Switch::~Switch()
{
	Clear();
}

// Helpers

void Switch::Clear()
{
	switchType = SwitchType::None;
	exists = false;
}

SwitchBool::SwitchBool(const bool defaultValue /*= false*/)
	: Switch(SwitchType::Simple)
	, value(defaultValue)
{
}

SwitchBool::operator bool() const
{
	return GetBool();
}

SwitchInt::SwitchInt()
	: Switch(SwitchType::Int)
{
}

SwitchBool& SwitchBool::operator=(const bool newVal)
{
	SetBool(newVal);
	return *this;
}

SwitchInt& SwitchInt::operator=(const ValueType_t newVal)
{
	SetInt(newVal);
	return *this;
}

SwitchStr::SwitchStr()
	: Switch(SwitchType::Str)
{
}

SwitchStr::~SwitchStr()
{
	Clear();
}

SwitchStr& SwitchStr::operator=(const wchar_t* const newVal)
{
	SetStr(newVal);
	return *this;
}

void SwitchBool::Clear()
{
	value = false;
	Switch::Clear();
}

bool SwitchBool::GetBool() const
{
	_ASSERTE(switchType == SwitchType::Simple || (switchType == SwitchType::None && !exists));
	return (exists && value);
}

void SwitchBool::SetBool(const bool newVal)
{
	value = newVal;
	exists = true;
	if (switchType != SwitchType::Simple)
	{
		_ASSERTE(switchType == SwitchType::Simple || switchType == SwitchType::None);
		switchType = SwitchType::Simple;
	}
}

void SwitchInt::Clear()
{
	value = 0;
	Switch::Clear();
}

bool SwitchInt::IsValid() const
{
	return exists && switchType == SwitchType::Int;
}

SwitchInt::ValueType_t SwitchInt::GetInt() const
{
	_ASSERTE(switchType==SwitchType::Int || (switchType==SwitchType::None && !exists));
	return (exists && (switchType == SwitchType::Int)) ? value : 0;
}

SwitchInt::ValueType_t SwitchInt::SetInt(const ValueType_t newVal)
{
	value = newVal;
	exists = true;
	if (switchType != SwitchType::Int)
	{
		_ASSERTE(switchType == SwitchType::Int || switchType == SwitchType::None);
		switchType = SwitchType::Int;
	}
	return value;
}

SwitchInt::ValueType_t SwitchInt::SetInt(const wchar_t* const newVal, const int radix /*= 10*/)
{
	if (!newVal || !*newVal)
	{
		Clear();
		exists = true;
	}
	else
	{
		wchar_t* endPtr = nullptr;
		const auto iVal = wcstoll(newVal, &endPtr, radix);
		SetInt(iVal);
	}
	return value;
}

void SwitchStr::Clear()
{
	if (value)
		free(value);
	Switch::Clear();
}

bool SwitchStr::IsEmpty() const
{
	if (!exists)
		return true;
	if (!value || !*value)
		return true;
	return false;
}

LPCWSTR SwitchStr::GetStr() const
{
	if (!exists || (switchType != SwitchType::Str))
		return nullptr;
	if (!value || !*value)
		return nullptr;
	return value;
}

void SwitchStr::SetStr(const wchar_t* const newVal)
{
	if (value != newVal)
	{
		if (value)
			free(value);
		value = (newVal && *newVal) ? lstrdup(newVal) : nullptr;
	}
	exists = true;
	switchType = SwitchType::Str;
}

void SwitchStr::SetStr(wchar_t*&& newVal)
{
	if (value != newVal && value != nullptr)
		free(value);
	value = newVal;
	exists = true;
	switchType = SwitchType::Str;
}


// ********************************************
ConsoleArgs::ConsoleArgs() // NOLINT(modernize-use-equals-default)
{
}

ConsoleArgs::~ConsoleArgs() // NOLINT(modernize-use-equals-default)
{
}

bool ConsoleArgs::GetCfgParam(LPCWSTR& cmdLineRest, SwitchStr& Val)
{
	Val.Clear();

	if (!cmdLineRest || !*cmdLineRest)
	{
		_ASSERTE(cmdLineRest && *cmdLineRest);
		return false;
	}

	CmdArg newArg;
	if (!((cmdLineRest = NextArg(cmdLineRest, newArg))))
	{
		return false;
	}

	Val.SetStr(newArg.Detach());
	_ASSERTE(Val.switchType == SwitchType::Str);
	_ASSERTE(Val.exists == (Val.value && *Val.value));
	Val.exists = (Val.value && *Val.value);

	return true;
}

/// Debug helper, show message box if SHOW_ATTACH_MSGBOX is defined and "/ADMIN" switch is specified
// ReSharper disable once CppMemberFunctionMayBeStatic
void ConsoleArgs::ShowAttachMsgBox(const CEStr& szArg) const
{
#ifdef SHOW_ATTACH_MSGBOX
	if (!IsDebuggerPresent())
	{
		wchar_t szTitle[100]; swprintf_c(szTitle, L"%s PID=%u %s", gsModuleName, gnSelfPID, szArg.c_str(L""));
		MessageBox(NULL, fullCmdLine_.c_str(L""), szTitle, MB_SYSTEMMODAL);
	}
#endif
}

void ConsoleArgs::ShowConfigMsgBox(const CEStr& szArg, LPCWSTR cmdLineRest)
{
#ifdef SHOW_LOADCFGFILE_MSGBOX
	MessageBox(NULL, lsCmdLine, szArg.c_str(L""), MB_SYSTEMMODAL);
#endif
}

void ConsoleArgs::ShowServerStartedMsgBox(LPCWSTR asCmdLine)
{
#ifdef SHOW_SERVER_STARTED_MSGBOX
	wchar_t szTitle[100]; swprintf_c(szTitle, L"ConEmuC [Server] started (PID=%i)", gnSelfPID);
	const wchar_t* pszCmdLine = asCmdLine;
	MessageBox(NULL,pszCmdLine,szTitle,0);
#endif
}

void ConsoleArgs::ShowComspecStartedMsgBox(LPCWSTR asCmdLine)
{
#ifdef SHOW_COMSPEC_STARTED_MSGBOX
	wchar_t szTitle[100]; swprintf_c(szTitle, L"ConEmuC [ComSpec] started (PID=%i)", gnSelfPID);
	const wchar_t* pszCmdLine = asCmdLine;
	MessageBox(NULL,pszCmdLine,szTitle,0);
#endif
}

void ConsoleArgs::AddConEmuArg(LPCWSTR asSwitch, LPCWSTR asValue)
{
	lstrmerge(&conemuAddArgs_.ms_Val, asSwitch);
	if (asValue && *asValue)
	{
		const bool needQuot = IsQuotationNeeded(asValue);
		lstrmerge(&conemuAddArgs_.ms_Val,
			needQuot ? L" \"" : L" ",
			asValue,
			needQuot ? L"\"" : NULL);
	}
	SetEnvironmentVariable(ENV_CONEMU_EXEARGS_W, conemuAddArgs_);
}

/// AutoAttach is not allowed in some cases
bool ConsoleArgs::IsAutoAttachAllowed() const
{
	if (!gpState->realConWnd_)
	{
		LogString("IsAutoAttachAllowed is not allowed, gpState->realConWnd is not null");
		return false;
	}

	if (gpState->attachMode_ & am_Admin)
	{
		LogString("IsAutoAttachAllowed is allowed due to am_Admin");
		return true;
	}

	if (!IsWindowVisible(gpState->realConWnd_))
	{
		if (defTermCall_ || attachFromFar_)
		{
			LogString(
				defTermCall_
					? "IsAutoAttachAllowed is allowed due to gbDefTermCall"
					: "IsAutoAttachAllowed is allowed due to gbAttachFromFar");
			return true;
		}
		return false;
	}

	if (IsIconic(gpState->realConWnd_))
	{
		LogString("IsAutoAttachAllowed is not allowed, gpState->realConWnd is iconic");
		return false;
	}

	LogString("IsAutoAttachAllowed is allowed");
	return true;
}

bool ConsoleArgs::IsConsoleModeFlags() const
{
	if (!consoleModeFlags_.exists)
		return true; // set defaults
	// {old comment}: if 0 - set (ENABLE_QUICK_EDIT_MODE|ENABLE_EXTENDED_FLAGS|ENABLE_INSERT_MODE)
	return consoleModeFlags_.GetInt() != 0;
}

/// Are we sure to hide RealConsole window
bool ConsoleArgs::IsForceHideConWnd() const
{
	// В принципе, консоль может действительно запуститься видимой. В этом случае ее скрывать не нужно
	// Но скорее всего, консоль запущенная под Админом в Win7 будет отображена ошибочно
	// 110807 - Если gpStatus->attachMode_, тоже консоль нужно спрятать
	return forceHideConWnd_ || (gpState->attachMode_ && (gpState->attachMode_ != am_Admin));
}

/// <summary>
/// Parse command line into member variables
/// </summary>
/// <param name="pszCmdLine">Full command line of ConEmuC.exe or argument passes to ConsoleMain3</param>
/// <returns>0 on success or error code otherwise</returns>
int ConsoleArgs::ParseCommandLine(LPCWSTR pszCmdLine)
{
	_ASSERTE(pszCmdLine!=nullptr);
	fullCmdLine_.Set(pszCmdLine ? pszCmdLine : L"");

	// pszCmdLine *may* or *may not* start with our executable or full path to our executable
	const auto* cmdLineRest = SkipNonPrintable(fullCmdLine_);
	CmdArg szArg;

	// Check the first argument in the command line (most probably it will be our executable path/name)
	{
		LPCWSTR pszTemp = fullCmdLine_;
		if (!((pszTemp = NextArg(pszTemp, szArg))))
		{
			_ASSERTE(FALSE && "GetCommandLine() is empty");
			return 0;
		}
		if (!szArg.IsPossibleSwitch())
		{
			_ASSERTE(IsFilePath(szArg));
			cmdLineRest = SkipNonPrintable(pszTemp);
		}
	}

	int iResult = 0;
	LPCWSTR pszArgStart;

	// Must be empty at the moment
	_ASSERTE(command_.IsEmpty());

	// Let parse the reset
	szArg.Empty();

	// Processing loop begin
	while ((cmdLineRest = NextArg(cmdLineRest, szArg, &pszArgStart)))
	{
		if (!szArg.IsPossibleSwitch())
		{
			_ASSERTE(FALSE && "Unknown switch!");
			// Show error on unknown switch
			unknownSwitch_.SetStr(pszArgStart);
			break;
		}


		// Main processing cycle
		params_++;

		if (szArg.OneOfSwitches(L"-?", L"-h", L"-help"))
		{
			iResult = CERR_HELPREQUESTED;
			break;
		}
#ifdef _DEBUG
		else if (szArg.IsSwitch(L"/DebugTrap"))
		{
			// ReSharper disable once CppJoinDeclarationAndAssignment
			int i, j = 1;
			j--;
			// ReSharper disable once CppJoinDeclarationAndAssignment
			i = 1 / j;
			std::ignore = i;
		}
#endif
			// **** Unit tests ****
		else if (szArg.OneOfSwitches(L"/Args", L"/ParseArgs"))
		{
			eExecAction_ = ConEmuExecAction::ParseArgs;
			command_.Set(cmdLineRest);
			break;
		}
		else if (szArg.IsSwitch(L"/ConInfo"))
		{
			eExecAction_ = ConEmuExecAction::PrintConsoleInfo;
			break;
		}
		else if (szArg.IsSwitch(L"/CheckUnicode"))
		{
			eExecAction_ = ConEmuExecAction::CheckUnicodeFont;
			break;
		}
		else if (szArg.IsSwitch(L"/TestUnicode"))
		{
			eExecAction_ = ConEmuExecAction::TestUnicodeCvt;
			break;
		}
		else if (szArg.IsSwitch(L"/OsVerInfo"))
		{
			eExecAction_ = ConEmuExecAction::OsVerInfo;
			break;
		}
		else if (szArg.IsSwitch(L"/ErrorLevel"))
		{
			eExecAction_ = ConEmuExecAction::ErrorLevel;
			command_.Set(cmdLineRest);
			break;
		}
		else if (szArg.IsSwitch(L"/Result"))
		{
			printRetErrLevel_.SetBool(true);
		}
		else if (szArg.OneOfSwitches(L"/echo", L"/e"))
		{
			eExecAction_ = ConEmuExecAction::OutEcho;
			break;
		}
		else if (szArg.OneOfSwitches(L"/type", L"/t"))
		{
			eExecAction_ = ConEmuExecAction::OutType;
			break;
		}
			// **** Regular use ****
		else if (szArg.IsSwitch(L"/RegConFont="))
		{
			eExecAction_ = ConEmuExecAction::RegConFont;
			command_.Set(szArg.GetExtra());
			break;
		}
		else if (szArg.IsSwitch(L"/SetHooks="))
		{
			eExecAction_ = ConEmuExecAction::InjectHooks;
			command_.Set(szArg.GetExtra());
			break;
		}
		else if (szArg.IsSwitch(L"/INJECT="))
		{
			eExecAction_ = ConEmuExecAction::InjectRemote;
			command_.Set(szArg.GetExtra());
			break;
		}
		else if (szArg.IsSwitch(L"/DEFTRM="))
		{
			eExecAction_ = ConEmuExecAction::InjectDefTrm;
			command_.Set(szArg.GetExtra());
			break;
		}
		else if (szArg.OneOfSwitches(L"/STRUCT", L"/DumpStruct"))
		{
			eExecAction_ = ConEmuExecAction::DumpStruct;
			command_.Set(cmdLineRest);
			break;
		}
		else if (szArg.IsSwitch(L"/StoreCWD"))
		{
			eExecAction_ = ConEmuExecAction::StoreCWD;
			command_.Set(cmdLineRest);
			break;
		}
			// /GUIMACRO[:PID|0xHWND][:T<tab>][:S<split>] <Macro string>
		else if (szArg.OneOfSwitches(L"/GuiMacro", L"/GuiMacro="))
		{
			// Execute rest of the command line as GuiMacro
			guiMacro_.SetStr(szArg);
			eExecAction_ = ConEmuExecAction::GuiMacro;
			command_.Set(cmdLineRest);
			break;
		}
		else if (szArg.IsSwitch(L"/UseExport"))
		{
			macroExportResult_.SetBool(true);
		}
		else if (szArg.IsSwitch(L"/SILENT"))
		{
			preferSilentMode_.SetBool(true);
		}
		else if (szArg.OneOfSwitches(L"/EXPORT=ALL", L"/ExportAll"))
		{
			eExecAction_ = ConEmuExecAction::ExportAll;
			//_ASSERTE(FALSE && "Continue to export");
			command_.Set(cmdLineRest);
			break;
		}
		else if (szArg.OneOfSwitches(L"/EXPORT=CON", L"/ExportCon"))
		{
			eExecAction_ = ConEmuExecAction::ExportCon;
			//_ASSERTE(FALSE && "Continue to export");
			command_.Set(cmdLineRest);
			break;
		}
		else if (szArg.OneOfSwitches(L"/EXPORT=GUI", L"/ExportGui"))
		{
			eExecAction_ = ConEmuExecAction::ExportGui;
			//_ASSERTE(FALSE && "Continue to export");
			command_.Set(cmdLineRest);
			break;
		}
		else if (szArg.IsSwitch(L"/EXPORT"))
		{
			eExecAction_ = ConEmuExecAction::ExportTab;
			//_ASSERTE(FALSE && "Continue to export");
			command_.Set(cmdLineRest);
			break;
		}
		else if (szArg.IsSwitch(L"/IsConEmu"))
		{
			eStateCheck_ = ConEmuStateCheck::IsConEmu;
			break;
		}
		else if (szArg.IsSwitch(L"/IsTerm"))
		{
			eStateCheck_ = ConEmuStateCheck::IsTerm;
			break;
		}
		else if (szArg.IsSwitch(L"/IsAnsi"))
		{
			eStateCheck_ = ConEmuStateCheck::IsAnsi;
			break;
		}
		else if (szArg.IsSwitch(L"/IsAdmin"))
		{
			eStateCheck_ = ConEmuStateCheck::IsAdmin;
			break;
		}
		else if (szArg.IsSwitch(L"/IsRedirect"))
		{
			eStateCheck_ = ConEmuStateCheck::IsRedirect;
			break;
		}
		else if (szArg.OneOfSwitches(L"/CONFIRM", L"/ConfHalt", L"/EConfirm"))
		{
			const auto* name = szArg.Mid(1); // Skip the "/", it could be "-"
			confirmExitParm_ = (lstrcmpi(name, L"CONFIRM") == 0)
								   ? RConStartArgs::eConfAlways
								   : (lstrcmpi(name, L"ConfHalt") == 0)
								   ? RConStartArgs::eConfHalt
								   : RConStartArgs::eConfEmpty;
			gpState->alwaysConfirmExit_ = true;
			gpState->autoDisableConfirmExit_ = false;
		}
		else if (szArg.IsSwitch(L"/NoConfirm"))
		{
			confirmExitParm_ = RConStartArgs::eConfNever;
			gpState->alwaysConfirmExit_ = false;
			gpState->autoDisableConfirmExit_ = false;
		}
		else if (szArg.IsSwitch(L"/OmitHooksWarn"))
		{
			skipHookSettersCheck_.SetBool(true);
		}
		else if (szArg.IsSwitch(L"/ADMIN"))
		{
			ShowAttachMsgBox(szArg);
			gpState->attachMode_ |= am_Admin;
			gpState->runMode_ = RunMode::Server;
		}
		else if (szArg.IsSwitch(L"/ATTACH"))
		{
			ShowAttachMsgBox(szArg);
			if (!(gpState->attachMode_ & am_Modes))
				gpState->attachMode_ |= am_Simple;
			gpState->runMode_ = RunMode::Server;
		}
		else if (szArg.OneOfSwitches(L"/AutoAttach", L"/AttachDefTerm"))
		{
			ShowAttachMsgBox(szArg);
			gpState->attachMode_ |= am_Auto;
			gpState->alienMode_ = true;
			gpState->noCreateProcess_ = true;
			if (szArg.IsSwitch(L"/AutoAttach"))
			{
				gpState->runMode_ = RunMode::AutoAttach;
				gpState->attachMode_ |= am_Async;
			}
			if (szArg.IsSwitch(L"/AttachDefTerm"))
			{
				gpState->runMode_ = RunMode::Server;
				gpState->attachMode_ |= am_DefTerm;
			}

			/*
			#TODO process am_Auto flag

			// Below is also "/GHWND=NEW". There would be "requestNewGuiWnd_" set to "true"

			//ConEmu autorun (c) Maximus5
			//Starting "%ConEmuPath%" in "Attach" mode (NewWnd=%FORCE_NEW_WND%)

			if (!IsAutoAttachAllowed())
			{
				if (gpState->realConWnd_ && IsWindowVisible(gpState->realConWnd_))
				{
					_printf("AutoAttach was requested, but skipped\n");
				}
				gpState->DisableAutoConfirmExit();
				//_ASSERTE(FALSE && "AutoAttach was called while Update process is in progress?");
				return CERR_AUTOATTACH_NOT_ALLOWED;
			}
			*/
		}
		else if (szArg.IsSwitch(L"/GuiAttach="))
		{
			ShowAttachMsgBox(szArg);

			gpState->runMode_ = RunMode::Server;
			if (!(gpState->attachMode_ & am_Modes))
				gpState->attachMode_ |= am_Simple;
			attachGuiAppWnd_.SetInt(szArg.GetExtra(), 16);

			/*
			#TODO process in worker
			if (!attachGuiAppWnd_.IsValid())
			{
				LogString(L"CERR_CARGUMENT: Invalid Child HWND was specified in /GuiAttach arg");
				_printf("Invalid Child HWND specified: ");
				_wprintf(szArg);
				_printf("\n" "Command line:\n");
				_wprintf(GetCommandLineW());
				_printf("\n");
				_ASSERTE(FALSE && "Invalid window was specified in /GuiAttach arg");
				return CERR_CARGUMENT;
			}
			HWND2 hAppWnd{ attachGuiApp_.value };
			if (IsWindow(hAppWnd))
				gpWorker->SetRootProcessGui(hAppWnd);
			*/
		}
		else if (szArg.IsSwitch(L"/NoCmd"))
		{
			gpState->runMode_ = RunMode::Server;
			gpState->noCreateProcess_ = true;
			gpState->alienMode_ = true;
		}
		else if (szArg.IsSwitch(L"/ParentFarPid="))
		{
			parentFarPid_.SetInt(szArg.GetExtra(), 10);
		}
		else if (szArg.IsSwitch(L"/CreateCon"))
		{
			creatingHiddenConsole_.SetBool(true);
			//_ASSERTE(FALSE && "Continue to create con");
		}
		else if (szArg.IsSwitch(L"/ROOTEXE"))
		{
			if ((cmdLineRest = NextArg(cmdLineRest, szArg, &pszArgStart)))
				rootExe_.SetStr(lstrmerge(L"\"", szArg, L"\""));
		}
		else if (szArg.OneOfSwitches(L"/PID=", L"/TRMPID=", L"/FARPID=", L"/CONPID="))
		{
			gpState->runMode_ = RunMode::Server;
			gpState->noCreateProcess_ = true; // process already started
			gpState->alienMode_ = true;       // console was not created by us

			if (szArg.IsSwitch(L"/TRMPID="))
			{
				// This is called from *.vshost.exe when "AllocConsole" just created
				defTermCall_.SetBool(true);
				doNotInjectConEmuHk_.SetBool(true);
			}
			else if (szArg.IsSwitch(L"/FARPID="))
			{
				attachFromFar_.SetBool(true);
				gpState->rootIsCmdExe_ = false;
			}
			else if (szArg.IsSwitch(L"/CONPID="))
			{
				//_ASSERTE(FALSE && "Continue to alternative attach mode");
				alternativeAttach_.SetBool(true);
				gpState->rootIsCmdExe_ = false;
			}
			else if (szArg.IsSwitch(L"/CONPID="))
			{
				// Nothing to set here
			}
			else
			{
				_ASSERTE(FALSE && "Should not get here");
			}

			rootPid_.SetInt(szArg.GetExtra(), 10);

			/*
			#TODO process in worker
			gpWorker->SetRootProcessId(wcstoul(pszStart, &pszEnd, 10));

			if ((gpWorker->RootProcessId() == 0) && gpConsoleArgs->creatingHiddenConsole_)
			{
				gpWorker->SetRootProcessId(WaitForRootConsoleProcess(30000));
			}

			if (gbAlternativeAttach && gpWorker->RootProcessId())
			{
				// if process was started "with console window"
				if (gpState->realConWnd)
				{
					DEBUGTEST(SafeCloseHandle(ghFarInExecuteEvent));
				}

				BOOL bAttach = StdCon::AttachParentConsole(gpWorker->RootProcessId());
				if (!bAttach)
				{
					gbInShutdown = TRUE;
					gpState->alwaysConfirmExit_ = FALSE;
					LogString(L"CERR_CARGUMENT: (gbAlternativeAttach && gpWorker->RootProcessId())");
					return CERR_CARGUMENT;
				}

				// Need to be set, because of new console === new handler
				SetConsoleCtrlHandler((PHANDLER_ROUTINE)HandlerRoutine, true);

				_ASSERTE(ghFarInExecuteEvent == NULL);
				_ASSERTE(gpState->realConWnd != NULL);
			}
			else if (gpWorker->RootProcessId() == 0)
			{
				LogString("CERR_CARGUMENT: Attach to GUI was requested, but invalid PID specified");
				_printf("Attach to GUI was requested, but invalid PID specified:\n");
				_wprintf(GetCommandLineW());
				_printf("\n");
				_ASSERTE(FALSE && "Attach to GUI was requested, but invalid PID specified");
				return CERR_CARGUMENT;
			}
			*/
		}
		else if (szArg.IsSwitch(L"/CInMode="))
		{
			consoleModeFlags_.SetInt(szArg.GetExtra(), 16);
		}
		else if (szArg.IsSwitch(L"/HIDE"))
		{
			forceHideConWnd_.SetBool(true);
		}
		else if (szArg.OneOfSwitches(L"/BW=", L"/BH=", L"/BZ=", L"/BX="))
		{
			if (szArg.IsSwitch(L"/BW="))
			{
				if (visibleWidth_.SetInt(szArg.GetExtra(), 10) > 0)
				{
					gcrVisibleSize.X = static_cast<decltype(gcrVisibleSize.X)>(visibleWidth_.GetInt());
					gbParmVisibleSize = true;
				}
			}
			else if (szArg.IsSwitch(L"/BH="))
			{
				if (visibleHeight_.SetInt(szArg.GetExtra(), 10) > 0)
				{
					gcrVisibleSize.Y = static_cast<decltype(gcrVisibleSize.Y)>(visibleHeight_.GetInt());
					gbParmVisibleSize = true;
				}
			}
			else if (szArg.IsSwitch(L"/BZ="))
			{
				if (bufferHeight_.SetInt(szArg.GetExtra(), 10) > 0)
				{
					gnBufferHeight = static_cast<decltype(gnBufferHeight)>(bufferHeight_.GetInt());
					gbParmVisibleSize = true;
				}
			}
			else if (szArg.IsSwitch(L"/BX="))
			{
				bufferWidth_.SetInt(szArg.GetExtra(), 10);
				// #TODO BufferWidth
			}
			else
			{
				_ASSERTE(FALSE && "Should not get here");
			}
		}
		else if (szArg.OneOfSwitches(L"/FN=", L"/FW=", L"/FH="))
		{
			if (szArg.IsSwitch(L"/FN="))
			{
				consoleFontName_.SetStr(szArg.GetExtra());
				_ASSERTE(!consoleFontName_.IsEmpty() && wcslen(consoleFontName_.GetStr()) < LF_FACESIZE);
			}
			else if (szArg.IsSwitch(L"/FW="))
			{
				consoleFontWidth_.SetInt(szArg.GetExtra(), 10);
			}
			else if (szArg.IsSwitch(L"/FH="))
			{
				consoleFontHeight_.SetInt(szArg.GetExtra(), 10);
			}
		}
		else if (szArg.OneOfSwitches(L"/LOG", L"/LOG0", L"/LOG1", L"/LOG2", L"/LOG3", L"/LOG4"))
		{
			// we use here lstrcmpni, because different logging levels are possible (e.g. /LOG1)
			// for now, levels are ignored, so just /LOG
			isLogging_.SetBool(true);
			/*
			#TODO
			CreateLogSizeFile(0);
			*/
		}
		else if (szArg.IsSwitch(L"/GID="))
		{
			gpState->runMode_ = RunMode::Server;
			conemuPid_.SetInt(szArg.GetExtra(), 10);
			/*
			#TODO Init and validate gpState->conemuPid_
			wchar_t* pszEnd = nullptr;
			gpState->conemuPid_ = wcstoul(szArg.GetExtra(), &pszEnd, 10);

			if (gpState->conemuPid_ == 0)
			{
				LogString(L"CERR_CARGUMENT: Invalid GUI PID specified");
				_printf("Invalid GUI PID specified:\n");
				_wprintf(GetCommandLineW());
				_printf("\n");
				_ASSERTE(FALSE);
				return CERR_CARGUMENT;
			}
			*/
		}
		else if (szArg.IsSwitch(L"/AID="))
		{
			wchar_t* pszEnd = nullptr;
			gpState->dwGuiAID = wcstoul(szArg.Mid(5), &pszEnd, 10);
		}
		else if (szArg.IsSwitch(L"/GHWND="))
		{
			if (gpState->runMode_ == RunMode::Undefined)
			{
				gpState->runMode_ = RunMode::Server;
			}
			else
			{
				_ASSERTE(
					gpState->runMode_ == RunMode::AutoAttach
					|| gpState->runMode_ == RunMode::Server || gpState->runMode_ == RunMode::AltServer);
			}

			if (lstrcmpi(szArg.GetExtra(), L"NEW") == 0)
			{
				requestNewGuiWnd_.SetBool(true);
				/*
				#TODO reset gpState->conemuPid_ & gpState->hGuiWnd
				gpState->hGuiWnd = nullptr;
				_ASSERTE(gpState->conemuPid_ == 0);
				gpState->conemuPid_ = 0;
				*/
			}
			else
			{
				LPCWSTR pszDescriptor = szArg.GetExtra();
				if (pszDescriptor[0] == L'0' && (pszDescriptor[1] == L'x' || pszDescriptor[1] == L'X'))
					pszDescriptor += 2; // That may be useful for calling from batch files
				conemuHwnd_.SetInt(pszDescriptor, 16);
				requestNewGuiWnd_.SetBool(false);


				/*
				#TODO apply gpState->hGuiWnd=conemuHwnd_ and check it
				wchar_t szLog[120];
				const bool isWnd = gpState->hGuiWnd ? IsWindow(gpState->hGuiWnd) : FALSE;
				const DWORD nErr = gpState->hGuiWnd ? GetLastError() : 0;

				swprintf_c(
					szLog, L"GUI HWND=0x%08X, %s, ErrCode=%u",
					LODWORD(gpState->hGuiWnd), isWnd ? L"Valid" : L"Invalid", nErr);
				LogString(szLog);

				if (!isWnd)
				{
					LogString(L"CERR_CARGUMENT: Invalid GUI HWND was specified in /GHWND arg");
					_printf("Invalid GUI HWND specified: ");
					_wprintf(szArg);
					_printf("\n" "Command line:\n");
					_wprintf(GetCommandLineW());
					_printf("\n");
					_ASSERTE(FALSE && "Invalid window was specified in /GHWND arg");
					return CERR_CARGUMENT;
				}

				DWORD nPID = 0;
				GetWindowThreadProcessId(gpState->hGuiWnd, &nPID);
				_ASSERTE(gpState->conemuPid_ == 0 || gpState->conemuPid_ == nPID);
				gpState->conemuPid_ = nPID;
				*/
			}
		}
		else if (szArg.IsSwitch(L"/TA="))
		{
			consoleColorIndexes_.SetInt(szArg.GetExtra(), 16);
			/*
			#TODO process nColors = DWORD(consoleColorIndexes_) and apply them
			if (nColors)
			{
				DWORD nTextIdx = (nColors & 0xFF);
				DWORD nBackIdx = ((nColors >> 8) & 0xFF);
				DWORD nPopTextIdx = ((nColors >> 16) & 0xFF);
				DWORD nPopBackIdx = ((nColors >> 24) & 0xFF);

				if ((nTextIdx <= 15) && (nBackIdx <= 15) && (nTextIdx != nBackIdx))
					gnDefTextColors = MAKECONCOLOR(nTextIdx, nBackIdx);

				if ((nPopTextIdx <= 15) && (nPopBackIdx <= 15) && (nPopTextIdx != nPopBackIdx))
					gnDefPopupColors = MAKECONCOLOR(nPopTextIdx, nPopBackIdx);

				HANDLE hConOut = ghConOut;
				CONSOLE_SCREEN_BUFFER_INFO csbi5 = {};
				GetConsoleScreenBufferInfo(hConOut, &csbi5);

				if (gnDefTextColors || gnDefPopupColors)
				{
					BOOL bPassed = FALSE;

					if (gnDefPopupColors && (gnOsVer >= 0x600))
					{
						MY_CONSOLE_SCREEN_BUFFER_INFOEX csbi = {sizeof(csbi)};
						if (apiGetConsoleScreenBufferInfoEx(hConOut, &csbi))
						{
							// Microsoft bug? When console is started elevated - it does NOT show
							// required attributes, BUT GetConsoleScreenBufferInfoEx returns them.
							if (!(gpState->attachMode_ & am_Admin)
								&& (!gnDefTextColors || (csbi.wAttributes = gnDefTextColors))
								&& (!gnDefPopupColors || (csbi.wPopupAttributes = gnDefPopupColors)))
							{
								bPassed = TRUE; // Менять не нужно, консоль соответствует
							}
							else
							{
								if (gnDefTextColors)
									csbi.wAttributes = gnDefTextColors;
								if (gnDefPopupColors)
									csbi.wPopupAttributes = gnDefPopupColors;

								_ASSERTE(FALSE && "Continue to SetConsoleScreenBufferInfoEx");

								// Vista/Win7. _SetConsoleScreenBufferInfoEx unexpectedly SHOWS console window
								//if (gnOsVer == 0x0601)
								//{
								//	RECT rcGui = {};
								//	if (gpState->hGuiWnd)
								//		GetWindowRect(gpState->hGuiWnd, &rcGui);
								//	//SetWindowPos(gpState->realConWnd, HWND_BOTTOM, rcGui.left+3, rcGui.top+3, 0,0, SWP_NOSIZE|SWP_SHOWWINDOW|SWP_NOZORDER);
								//	SetWindowPos(gpState->realConWnd, NULL, -30000, -30000, 0,0, SWP_NOSIZE|SWP_SHOWWINDOW|SWP_NOZORDER);
								//	apiShowWindow(gpState->realConWnd, SW_SHOWMINNOACTIVE);
								//	#ifdef _DEBUG
								//	apiShowWindow(gpState->realConWnd, SW_SHOWNORMAL);
								//	apiShowWindow(gpState->realConWnd, SW_HIDE);
								//	#endif
								//}

								bPassed = apiSetConsoleScreenBufferInfoEx(hConOut, &csbi);

								// Что-то Win7 хулиганит
								if (!gbVisibleOnStartup)
								{
									apiShowWindow(gpState->realConWnd_, SW_HIDE);
								}
							}
						}
					}


					if (!bPassed && gnDefTextColors)
					{
						SetConsoleTextAttribute(hConOut, gnDefTextColors);
						RefillConsoleAttributes(csbi5, csbi5.wAttributes, gnDefTextColors);
					}
				}
			}
			*/
		}
		else if (szArg.IsSwitch(L"/DEBUGPID="))
		{
			//gpStatus->runMode_ = RunMode::RM_SERVER; -- don't set, the RM_UNDEFINED is flag for debugger only
			debugPid_.SetInt(szArg.GetExtra(), 10);
			/*
			#TODO init debugger
			const auto dbgRc = gpWorker->SetDebuggingPid(szArg.Mid(10));
			if (dbgRc != 0)
				return dbgRc;
			*/
		}
		else if (szArg.OneOfSwitches(L"/DebugExe", L"/DebugTree"))
		{
			//gpStatus->runMode_ = RunMode::RM_SERVER; -- don't set, the RM_UNDEFINED is flag for debugger only
			if (szArg.IsSwitch(L"/DebugExe"))
				debugExe_.SetBool(true);
			else
				debugTree_.SetBool(true);
			command_.Set(cmdLineRest);
			/*
			#TODO init debugger
			const bool debugTree = (lstrcmpi(szArg, L"/DEBUGTREE") == 0);
			const auto dbgRc = gpWorker->SetDebuggingExe(lsCmdLine, debugTree);
			if (dbgRc != 0)
				return dbgRc;
			*/
			// STOP processing rest of command line, it goes to debugger
			break;
		}
		else if (szArg.IsSwitch(L"/DUMP"))
		{
			debugDump_.SetBool(true);
			/*
			#TODO init debugger
			gpWorker->SetDebugDumpType(DumpProcessType::AskUser);
			*/
		}
		else if (szArg.OneOfSwitches(L"/MINIDUMP", L"/MINI"))
		{
			debugDump_.SetBool(true);
			debugMiniDump_.SetBool(true);
			/*
			#TODO init debugger
			gpWorker->SetDebugDumpType(DumpProcessType::MiniDump);
			*/
		}
		else if (szArg.OneOfSwitches(L"/FULLDUMP", L"/FULL"))
		{
			debugDump_.SetBool(true);
			debugFullDump_.SetBool(true);
			/*
			#TODO init debugger
			gpWorker->SetDebugDumpType(DumpProcessType::FullDump);
			*/
		}
		else if (szArg.IsSwitch(L"/AutoMini"))
		{
			debugDump_.SetBool(true);
			//_ASSERTE(FALSE && "Continue to /AUTOMINI");
			const wchar_t* interval = nullptr;
			if (cmdLineRest && *cmdLineRest && isDigit(cmdLineRest[0]))
			{
				if ((cmdLineRest = NextArg(cmdLineRest, szArg, &pszArgStart)))
					interval = szArg.ms_Val;
			}
			debugAutoMini_.SetStr(interval);

			/*
			#TODO init debugger
			gpWorker->SetDebugAutoDump(interval);
			*/
		}
		else if (szArg.IsSwitch(L"/ProfileCd"))
		{
			needCdToProfileDir_.SetBool(true);
		}
		else if (szArg.IsSwitch(L"/CONFIG"))
		{
			ShowConfigMsgBox(szArg, cmdLineRest);
			if (!((cmdLineRest = NextArg(cmdLineRest, szArg, &pszArgStart))))
			{
				iResult = CERR_CMDLINEEMPTY;
				_ASSERTE(FALSE && "Config name was not specified!");
				_wprintf(L"Config name was not specified!\r\n");
				break;
			}
			configName_.SetStr(szArg);
			// Reuse config if starting "ConEmu.exe" from console server!
			SetEnvironmentVariable(ENV_CONEMU_CONFIG_W, szArg);
			AddConEmuArg(L"-config", szArg);
		}
		else if (szArg.IsSwitch(L"/LoadCfgFile"))
		{
			ShowConfigMsgBox(szArg, cmdLineRest);
			// Reuse specified xml file if starting "ConEmu.exe" from console server!
			if (!((cmdLineRest = NextArg(cmdLineRest, szArg, &pszArgStart))))
			{
				iResult = CERR_CMDLINEEMPTY;
				_ASSERTE(FALSE && "Xml file name was not specified!");
				_wprintf(L"Xml file name was not specified!\r\n");
				break;
			}
			configFile_.SetStr(szArg);
			AddConEmuArg(L"-LoadCfgFile", szArg);
		}
		else if (szArg.OneOfSwitches(L"/ASYNC", L"/FORK"))
		{
			asyncRun_.SetBool(true);
		}
		else if (szArg.IsSwitch(L"/NoInject"))
		{
			doNotInjectConEmuHk_.SetBool(true);
		}
		else if (szArg.IsSwitch(L"/DOSBOX"))
		{
			gpState->dosbox_.use_ = true;
		}
		else if (szArg.IsSwitch(L"-STD"))
		{
			gpState->reopenStdHandles_ = true;
		}
			// После этих аргументов - идет то, что передается в CreateProcess!
		else if (szArg.IsSwitch(L"/ROOT"))
		{
			ShowServerStartedMsgBox(cmdLineRest);
			gpState->runMode_ = RunMode::Server;
			gpState->noCreateProcess_ = FALSE;
			gpConsoleArgs->asyncRun_ = FALSE;
			command_.Set(cmdLineRest);
			/*
			#TODO init variables
			SetWorkEnvVar();
			*/
			break;
		}
			// После этих аргументов - идет то, что передается в COMSPEC (CreateProcess)!
			//if (wcscmp(szArg, L"/C")==0 || wcscmp(szArg, L"/c")==0 || wcscmp(szArg, L"/K")==0 || wcscmp(szArg, L"/k")==0) {
		else if (szArg.OneOfSwitches(L"/C", L"/K"))
		{
			ShowComspecStartedMsgBox(cmdLineRest);
			gpState->noCreateProcess_ = FALSE;

			//_ASSERTE(FALSE && "ConEmuC -c ...");

			if (szArg.GetLen() == 2) // "/c" or "/k"
				gpState->runMode_ = RunMode::Comspec;

			if (gpState->runMode_ == RunMode::Undefined && szArg[4] == 0
				&& ((szArg[2] & ~0x20) == L'M') && ((szArg[3] & ~0x20) == L'D'))
			{
				_ASSERTE(FALSE && "'/cmd' obsolete switch. use /c, /k, /root");
				gpState->runMode_ = RunMode::Server;
			}

			if (gpState->runMode_ == RunMode::Undefined)
			{
				gpState->runMode_ = RunMode::Comspec;
				// Support weird legacy "cmd /cecho xxx"
				command_.Set(SkipNonPrintable(pszArgStart + 2));
			}
			else
			{
				command_.Set(cmdLineRest);
			}

			if (gpState->runMode_ == RunMode::Comspec)
			{
				cmdK_.SetBool(szArg.IsSwitch(L"/K"));
				/*
				#TODO init worker
				gpWorker->SetCmdK((szArg[1] & ~0x20) == L'K');
				*/
			}

			/*
			#TODO expand ConEmu {Task}
			if (lsCmdLine && (lsCmdLine[0] == TaskBracketLeft) && wcschr(lsCmdLine, TaskBracketRight))
			{
				// Allow smth like: ConEmuC -c {Far} /e text.txt
				gpszTaskCmd = ExpandTaskCmd(lsCmdLine);
				if (gpszTaskCmd && *gpszTaskCmd)
					lsCmdLine = gpszTaskCmd;
			}
			*/

			break;
		}
		else
		{
			_ASSERTE(FALSE && "Unknown switch!");
			// Show error on unknown switch
			unknownSwitch_.SetStr(szArg);
			//_wprintf(L"Unknown switch: ");
			//_wprintf(szArg);
			//_wprintf(L"\r\n");
		}

		// Avoid assertions in NextArg
		szArg.Empty();
	} // while (NextArg(&cmdLineRest, szArg, &pszArgStart) == 0)
	// Processing loop end

	return iResult;
}
