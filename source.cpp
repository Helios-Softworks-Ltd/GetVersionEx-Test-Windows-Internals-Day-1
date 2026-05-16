// File: source.cpp
// Path: /source.cpp
// Created Date: Saturday May 16th 2026
// Author: Helios Softworks Ltd. (director@helios-softworks.com)
//
// Copyright (c) 2025 Helios Softworks Ltd. All rights reserved.

#include <print>
#include <windows.h>

int main() {
	OSVERSIONINFOEXW osvi;
	ZeroMemory( &osvi, sizeof( OSVERSIONINFOEXW ) );
	osvi.dwOSVersionInfoSize = sizeof( OSVERSIONINFOEXW );

	if ( GetVersionExW( ( OSVERSIONINFOW* )&osvi ) ) {
		// Output the major and minor version numbers (Should display 6.2)
		std::println( "Windows Version: {}.{}", osvi.dwMajorVersion, osvi.dwMinorVersion );
	} else {
		std::println( "Failed to get Windows version information." );
	}

	return 0;
}
