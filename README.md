# GetVersionEx-Test-Windows-Internals-Day-1

A small C++ test program demonstrating the quirky behaviour of the Windows `GetVersionEx` API on modern versions of Windows.

Built as part of the **Windows Internals Day 1** course from [TrainSec](https://trainsec.net/), which is included in the **Windows Master Developer** bundle.

## What This Program Does

It calls `GetVersionEx` and prints the major version, minor version, and build number that the API reports back. On a modern Windows 10 or 11 machine, you'll likely see something unexpected — the API reports **6.2** (Windows 8) rather than the real version of the operating system.

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

This was Microsoft's way of "freezing" the version number for legacy apps so their bad version checks wouldn't break, while letting modern, manifest-aware apps see the truth.

## The Issues It Caused

The fix solved one problem but created plenty of fresh confusion:

- Developers writing new code see `6.2` returned on Windows 10 or 11 and assume the API is broken
- Tutorials and Stack Overflow answers became outdated overnight
- Software that genuinely needs the real version has to use workarounds like `RtlGetVersion` (from `ntdll.dll`), reading the file version of `kernel32.dll`, or checking the registry directly
- Even Windows 11 reports as `10.0` internally — there is no `11.0` — so detecting Windows 11 requires looking at the build number (≥ 22000), not the major version

## Takeaway

`GetVersionEx` still exists, but it's no longer a reliable way to check the OS version. It's a great teaching example of how API design, backwards compatibility, and developer behaviour interact in messy ways inside an OS as widely deployed as Windows.

## Reference

- TrainSec — Windows Internals Day 1 / Windows Master Developer bundle
- Microsoft Docs: [Targeting your application for Windows](https://learn.microsoft.com/en-us/windows/win32/sysinfo/targeting-your-application-at-windows-8-1)
