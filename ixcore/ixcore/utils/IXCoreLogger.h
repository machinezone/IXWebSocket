#pragma once
#include <functional>

namespace ix
{
    class IXCoreLogger
    {
    public:
        using LogFunc = std::function<void(const char*)>;
        static void Log(const char* msg);

        static void setLogFunction(LogFunc& func) { _currentLogger = func; }

    private:
        static LogFunc _currentLogger;
    };

} // namespace ix
