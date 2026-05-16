# GetVersionEx Test

A small C++ test program demonstrating the quirky behaviour of the Windows `GetVersionEx` API on modern versions of Windows.

Built as part of the **Windows Internals Day 1** course from [TrainSec](https://trainsec.net/), which is included in the **Windows Master Developer** bundle.

## What This Program Does

It calls `GetVersionEx` and prints the major version, minor version, and build number that the API reports back. On a modern Windows 10 or 11 machine, the API reports **6.2** (Windows 8) rather than the real version of the operating system.

## A Bit of History

In the early days of Windows, `GetVersion` and later `GetVersionEx` were the standard way for an application to ask the OS what version it was running on. Developers used these calls to gate features, work around bugs in specific Windows releases, or display version info to the user.

The problem was that a huge number of applications used these APIs *badly*. Common patterns included:

- Exact equality checks (`if (major == 6 && minor == 1)` — works only on Windows 7)
- "Greater than" checks that assumed the major version would never change
- Refusing to run if the version wasn't recognised, even on newer releases

This meant that every time Microsoft shipped a new version of Windows, a wave of perfectly capable applications would refuse to launch or behave incorrectly — not because of any real incompatibility, but because of lazy version checks. Users blamed Microsoft, even though the bugs lived in third-party code.

## The Fix — and the New Confusion

Starting with **Windows 8.1**, Microsoft deprecated `GetVersionEx` and changed its behaviour. The API now returns a version number based on what your application's **manifest** declares it supports, not the actual OS version.

In short:

- No manifest (or only old GUIDs) → API returns **6.2.9200** (Windows 8), no matter what you're actually running on
- Manifest declares Windows 8.1 support → returns 6.3
- Manifest declares Windows 10 support → returns the real 10.0.x

This was Microsoft's way of freezing the version number for legacy apps so their bad version checks wouldn't break, while letting modern, manifest-aware apps see the truth.

## The Issues It Caused

The fix solved one problem but created plenty of fresh confusion:

- Developers writing new code see `6.2` returned on Windows 10 or 11 and assume the API is broken
- Tutorials and Stack Overflow answers became outdated overnight
- Software that genuinely needs the real version has to use workarounds like `RtlGetVersion` (from `ntdll.dll`), reading the file version of `kernel32.dll`, or checking the registry directly
- Even Windows 11 reports as `10.0` internally — there is no `11.0` — so detecting Windows 11 requires looking at the build number (≥ 22000), not the major version

## Seeing the Fix in Action

I've added an `app.manifest` file to the repo that declares support for Windows 8.1, Windows 10, and Windows 11. To see `GetVersionEx` report the real OS version, just uncomment the following line in the `CMakeLists.txt`:

```cmake
#app.manifest
```

Rebuild and run the program — `GetVersionEx` will now report `10.0.x` on Windows 10 / 11 instead of `6.2.9200`.

## Reverse Engineering

I started this session by loading up `KernelBase.dll` into IDA and loading the PDB for it. This let me locate and view `GetVersionEx`. On first view I found the function was quite small. It contains multiple calls into `ntdll`, as well as a size check on the passed struct.

```c
BOOL __stdcall GetVersionExW(_OSVERSIONINFOEXW *lpVersionInformation)
{
  DWORD dwOSVersionInfoSize; // ecx
  __int64 (*GetManifestedOSCompatibilityLevel)(void); // rax
  unsigned int v4; // eax
                                                // sizeof(_OSVERSIONINFOW) = 276
                                                // _OSVERSIONINFOEXW // sizeof=0x11C
  dwOSVersionInfoSize = lpVersionInformation->dwOSVersionInfoSize;
  if ( ((dwOSVersionInfoSize - 276) & 0xFFFFFFE7) != 0 || dwOSVersionInfoSize == 292 )
  {
    RtlSetLastWin32Error(122u);                 // ERROR_INSUFFICIENT_BUFFER
    return 0;
  }
  if ( RtlGetVersion((PRTL_OSVERSIONINFOW)lpVersionInformation) )
    return 0;
  if ( lpVersionInformation->dwOSVersionInfoSize == 284 )
    lpVersionInformation->wReserved = BaseRCNumber;// Maybe additional test build market?
                                                // It is 0 on my build
  GetManifestedOSCompatibilityLevel = (__int64 (*)(void))qword_18039E540;// has_manifest
  if ( qword_18039E540
    || (GetManifestedOSCompatibilityLevel = (__int64 (*)(void))SbSelectProcedure(0xABABABABLL, 0, "kLsE", 3),
        (qword_18039E540 = (__int64)GetManifestedOSCompatibilityLevel) != 0) )
  {
    v4 = GetManifestedOSCompatibilityLevel() - 1;// 0 == ALL (0-1 == 0xFFFFFFFF)
    if ( v4 )
    {
      if ( v4 == 1 )                            // Windows 8.1?
      {
        lpVersionInformation->dwMinorVersion = 3;
        lpVersionInformation->dwBuildNumber = 9600;
        lpVersionInformation->dwMajorVersion = 6;
      }
    }
    else
    {
      lpVersionInformation->dwMinorVersion = 2; // Windows 8
      lpVersionInformation->dwBuildNumber = 9200;
      lpVersionInformation->dwMajorVersion = 6;
    }
  }
  return 1;
}
```

For simplicity's sake we will be going off the default passed arg size which is 284 from `OSVERSIONINFOW`.

I was surprised to learn that the primary logic for this function was **not** within `RtlGetVersion`, despite the function being called `GetVersionExW`. To find out why, I loaded `ntdll.dll` into IDA with its PDB. After I located `RtlGetVersion` the first thing I saw at the top was:

```c
v3 = NtCurrentPeb();
lpVersionInformation->dwMajorVersion = v3->OSMajorVersion;
lpVersionInformation->dwMinorVersion = v3->OSMinorVersion;
lpVersionInformation->dwBuildNumber = v3->OSBuildNumber;
lpVersionInformation->dwPlatformId  = v3->OSPlatformId;
```

I initially assumed there would be more to this, but I found out there wasn't. Later in the function it calls `NtQuerySystemInformationEx` with the class `SystemBuildVersionInformation`, but this is never reached. That's due to a check within the function:

```c
if ( lpVersionInformation->dwOSVersionInfoSize != 300 )
  return 0;
```

This prevents it from ever reaching the seemingly perfect syscall for the job.

After this point it returns back to `KernelBase.dll` and continues on. The important part is `SbSelectProcedure`. This function is responsible for parsing the manifest if there is one. It returns a mini stub which proceeds to return a number. We will call this function `GetManifestedOSCompatibilityLevel`, but know it was **not** named within the PDB.

The function returned will always be something like:

```asm
xor eax, eax
ret
```
OR
```asm
mov eax, 1
ret
```

When I was using a manifest with support for everything, it returned `0`. In this case that means support for *every* version. This is because `0 - 1` in an unsigned int is `0xFFFFFFFF`, meaning it would take the first branch `if ( v4 )` but then skip over the Windows 8.1 branch — ultimately returning directly what the PEB provided.

When I was *not* using a manifest, it returned `1`. Result: `1 - 1 = 0`, first branch not taken, and the Windows 8 major/minor version is returned to the user.

The most important thing I learnt from this function is not that it's deprecated, but that **it has the wrong name**. So I have decided to bestow a new name upon it...

> `GetVersionFromPEBThenCompletelyIgnoreItIfNoManifest()`
>
> aka `GetVersionExClampedToManifest()`

I have attached two Cheat Engine traces for `GetVersionEx` — one with the manifest applied and one without. They are located within the project GitHub folder.

## Takeaway

`GetVersionEx` still exists, but it's no longer a reliable way to check the OS version. If you need the truth, use `RtlGetVersion` or read it from the registry — and if you want `GetVersionEx` itself to behave, ship a manifest.

## Reference

- TrainSec — Windows Internals Day 1 / Windows Master Developer bundle
- Microsoft Docs: [Targeting your application for Windows](https://learn.microsoft.com/en-us/windows/win32/sysinfo/targeting-your-application-at-windows-8-1)
