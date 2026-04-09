#include "EventLogger.h"

#include <chrono>
#include <cstdio>
#include <mutex>
#include <string>
#include <unordered_set>

namespace {
    FILE* g_file = nullptr;
    std::chrono::steady_clock::time_point g_start;
    std::mutex g_mutex;
    std::unordered_set<std::string> g_onceTags;

    // Assumes g_mutex is held by caller.
    void WriteLocked(const char* tag, const char* note) {
        if (!g_file) return;
        const auto now = std::chrono::steady_clock::now();
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - g_start).count();
        std::fprintf(g_file,
                     "{\"ms\":%lld,\"tag\":\"%s\",\"note\":\"%s\"}\n",
                     static_cast<long long>(ms),
                     tag ? tag : "",
                     note ? note : "");
        std::fflush(g_file);
    }
}

bool EventLogger::Init(const std::string& path) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_file) return true;
#ifdef _MSC_VER
    if (fopen_s(&g_file, path.c_str(), "w") != 0) g_file = nullptr;
#else
    g_file = std::fopen(path.c_str(), "w");
#endif
    if (!g_file) return false;
    g_start = std::chrono::steady_clock::now();
    g_onceTags.clear();
    WriteLocked("recording_start", "");
    return true;
}

void EventLogger::Shutdown() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_file) {
        WriteLocked("recording_end", "");
        std::fclose(g_file);
        g_file = nullptr;
    }
    g_onceTags.clear();
}

void EventLogger::Log(const char* tag, const char* note) {
    std::lock_guard<std::mutex> lock(g_mutex);
    WriteLocked(tag, note);
}

void EventLogger::LogOnce(const char* tag, const char* note) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_onceTags.insert(tag ? tag : "").second) return;
    WriteLocked(tag, note);
}
