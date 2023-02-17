
#pragma warning(disable : 4996)

#include "AssetFile.cpp"
#include "AssetFileIO.cpp"
#include "Json.cpp"
#include "Pack.cpp"
#include "Time.cpp"

#if defined(_WIN32)
#include "AsyncFileReader_win32.cpp"
#elif defined(__linux__)
#include "AsyncFileReader_aio.cpp"
#elif defined(__APPLE__)
#include "AsyncFileReader_posix_aio.cpp"
#endif

// includes Windows.h with all macro crap, so should be last
#include "DynLib.cpp"
