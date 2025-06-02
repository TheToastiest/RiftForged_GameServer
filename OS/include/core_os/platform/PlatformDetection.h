#ifndef RIFTFORGED_CORE_PLATFORMDETECTION_H
#define RIFTFORGED_CORE_PLATFORMDETECTION_H

// -------------------------------------------------------------------------
// Operating System Detection
// -------------------------------------------------------------------------
#if defined(_WIN32)
#define RF_PLATFORM_WINDOWS 1
#define RF_PLATFORM_NAME "Windows"
// Windows specific: distinguish between Win32 and Win64
#if defined(_WIN64)
#define RF_PLATFORM_WINDOWS_64 1
#else
#define RF_PLATFORM_WINDOWS_32 1
#endif
// Further Windows-specific definitions can go here if needed
// e.g., excluding rarely-used stuff from Windows headers
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX // Prevents Windows.h from defining min() and max() macros
#endif

#elif defined(__APPLE__) && defined(__MACH__)
#define RF_PLATFORM_MACOS 1
#define RF_PLATFORM_NAME "macOS"
// Apple specific: import TargetConditionals.h to check for iOS, tvOS, etc.
#if defined(__APPLE__) && defined(__MACH__)
#include <TargetConditionals.h>
#endif
#if TARGET_IPHONE_SIMULATOR == 1 || TARGET_OS_IPHONE == 1
#define RF_PLATFORM_IOS 1
#undef RF_PLATFORM_NAME // Redefine for more specific platform
#define RF_PLATFORM_NAME "iOS"
// Potentially further differentiate between iPhone and iPad if necessary
#elif TARGET_OS_MAC == 1
    // macOS specific (redundant with RF_PLATFORM_MACOS but can be explicit)
#define RF_PLATFORM_MACOS_DESKTOP 1
#endif

#elif defined(__linux__)
#define RF_PLATFORM_LINUX 1
#define RF_PLATFORM_NAME "Linux"
// Could add specific Linux distribution detection if ever needed, but generally not.

#elif defined(__FreeBSD__)
#define RF_PLATFORM_FREEBSD 1
#define RF_PLATFORM_NAME "FreeBSD"

#elif defined(__unix__) // Generic Unix-like OS (should be after more specific checks like Linux, macOS)
#define RF_PLATFORM_UNIX 1
#define RF_PLATFORM_NAME "Unix"

#else
#error "Unsupported platform: RiftForged currently does not support this operating system."
#define RF_PLATFORM_NAME "Unknown OS"
#endif

// -------------------------------------------------------------------------
// Architecture Detection
// -------------------------------------------------------------------------
#if defined(__amd64__) || defined(_M_X64)
#define RF_ARCH_X64 1
#define RF_ARCH_64_BIT 1
#define RF_ARCH_NAME "x86_64"
#elif defined(__i386__) || defined(_M_IX86)
#define RF_ARCH_X86 1
#define RF_ARCH_32_BIT 1
#define RF_ARCH_NAME "x86"
#elif defined(__aarch64__) || defined(_M_ARM64) // Check for ARM64 first
#define RF_ARCH_ARM64 1
#define RF_ARCH_ARM 1     // ARM64 is also ARM
#define RF_ARCH_64_BIT 1
#define RF_ARCH_NAME "ARM64"
#elif defined(__arm__) || defined(_M_ARM)
#define RF_ARCH_ARM 1
#define RF_ARCH_32_BIT 1 // ARM 32-bit
#define RF_ARCH_NAME "ARM"
#else
#error "Unsupported architecture."
#define RF_ARCH_NAME "Unknown Architecture"
#endif

// -------------------------------------------------------------------------
// Compiler Detection
// -------------------------------------------------------------------------
#if defined(_MSC_VER)
#define RF_COMPILER_MSVC 1
#define RF_COMPILER_NAME "MSVC"
#define RF_MSVC_VERSION _MSC_VER // e.g., 1930 for Visual Studio 2022 v17.0, 1940 for VS2022 v17.10
// You can create more user-friendly version strings if needed:
// #if RF_MSVC_VERSION == 1930
//     #define RF_MSVC_VERSION_STRING "VS2022 17.0"
// #elif ...

#elif defined(__clang__)
#define RF_COMPILER_CLANG 1
#define RF_COMPILER_NAME "Clang"
#define RF_CLANG_VERSION_MAJOR __clang_major__
#define RF_CLANG_VERSION_MINOR __clang_minor__
#define RF_CLANG_VERSION_PATCH __clang_patchlevel__
// Full version as a single number: (__clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__)

#elif defined(__GNUC__) // Check GCC after Clang, as Clang may define __GNUC__
#define RF_COMPILER_GCC 1
#define RF_COMPILER_NAME "GCC"
#define RF_GCC_VERSION_MAJOR __GNUC__
#define RF_GCC_VERSION_MINOR __GNUC_MINOR__
#define RF_GCC_VERSION_PATCH __GNUC_PATCHLEVEL__ // Might not always be defined, check usage
// Full version as a single number: (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)


#else
#warning "Unsupported compiler: Some features might not be available or optimal."
#define RF_COMPILER_NAME "Unknown Compiler"
#endif

// -------------------------------------------------------------------------
// Endianness Detection
// -------------------------------------------------------------------------
// C++20 provides std::endian for a standard way to check
#if __cplusplus >= 202002L
#include <bit> // For std::endian
#if std::endian::native == std::endian::little
#define RF_LITTLE_ENDIAN 1
#elif std::endian::native == std::endian::big
#define RF_BIG_ENDIAN 1
#else
#define RF_UNKNOWN_ENDIAN 1
#warning "Unknown endianness detected by std::endian."
#endif
#else // Pre-C++20 fallback (less reliable, often relies on architecture assumptions)
#if defined(RF_ARCH_X64) || defined(RF_ARCH_X86) || \
        (defined(RF_ARCH_ARM) && (defined(__ARMEL__) || defined(__LITTLE_ENDIAN__))) || \
        (defined(RF_ARCH_ARM64) && (defined(__AARCH64EL__) || defined(__LITTLE_ENDIAN__))) || \
        defined(RF_PLATFORM_WINDOWS) // Windows is predominantly little-endian
#define RF_LITTLE_ENDIAN 1
#elif (defined(RF_ARCH_ARM) && (defined(__ARMEB__) || defined(__BIG_ENDIAN__))) || \
          (defined(RF_ARCH_ARM64) && (defined(__AARCH64EB__) || defined(__BIG_ENDIAN__)))
#define RF_BIG_ENDIAN 1
#else
    // Attempt to use compiler/platform specific macros for endianness
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define RF_LITTLE_ENDIAN 1
#elif defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define RF_BIG_ENDIAN 1
#else
    #warning "Endianness not definitively determined for this platform/architecture. Assuming little-endian as a common default."
#define RF_LITTLE_ENDIAN 1 // Default assumption, use with caution
#endif
#endif
#endif


    // -------------------------------------------------------------------------
    // Debug / Release Mode
    // -------------------------------------------------------------------------
#if !defined(NDEBUG) || defined(_DEBUG) // NDEBUG is standard, _DEBUG is common for MSVC
#define RF_DEBUG_MODE 1
#define RF_BUILD_MODE_NAME "Debug"
#else
#define RF_RELEASE_MODE 1
#define RF_BUILD_MODE_NAME "Release"
#endif


// -------------------------------------------------------------------------
// Other useful macros
// -------------------------------------------------------------------------
#if defined(RF_PLATFORM_WINDOWS)
#ifdef RF_SYSTEM_LAYER_BUILD_DLL
#define RF_SYSTEM_API __declspec(dllexport)
#else
#define RF_SYSTEM_API __declspec(dllimport)
#endif
#else // For non-Windows platforms
#if defined(RF_COMPILER_GCC) || defined(RF_COMPILER_CLANG)
#define RF_SYSTEM_API __attribute__((visibility("default")))
#else
#define RF_SYSTEM_API // Empty for other compilers
#endif
#endif

#if defined(RF_COMPILER_MSVC)
#define RF_FORCE_INLINE __forceinline
#elif defined(RF_COMPILER_GCC) || defined(RF_COMPILER_CLANG)
#define RF_FORCE_INLINE inline __attribute__((always_inline))
#else
#define RF_FORCE_INLINE inline
#endif

#define RF_UNUSED(x) (void)x


#endif // RIFTFORGED_CORE_PLATFORMDETECTION_H