#pragma once
#include <string>

// Lightweight JSONL event logger for demo-video auto-editing.
// Writes lines of {"ms":<elapsed>,"tag":"<tag>","note":"<note>"} to a file.
// Thread-safe. ms is measured from EventLogger::Init() time.
class EventLogger {
public:
    // Opens the log file and starts the clock. Returns false on failure.
    // Call once at app startup, ideally at the exact moment OBS recording starts.
    static bool Init(const std::string& path);

    // Flushes and closes the log.
    static void Shutdown();

    // Appends one event. Safe to call from any thread.
    static void Log(const char* tag, const char* note = "");

    // Same as Log(), but only the first call for a given tag takes effect.
    // Useful for "first_fire" / "first_kill" style markers.
    static void LogOnce(const char* tag, const char* note = "");
};
