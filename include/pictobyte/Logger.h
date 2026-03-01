#pragma once
#include <string>
#include <functional>
#include <mutex>
#include <sstream>
#include <string_view>

namespace pb {

    using LogFn = std::function<void(std::string_view)>;

    /**
     * Thread-safe, lightweight logger backed by a user-supplied callback.
     * A default-constructed (or null-callback) Logger is a silent no-op.
     */
    class Logger {
    public:
        Logger() = default;
        explicit Logger(LogFn fn) : fn_(std::move(fn)) {}

        // Non-copyable: a shared mutex should not be silently duplicated.
        Logger(const Logger&)            = delete;
        Logger& operator=(const Logger&) = delete;

        Logger(Logger&&)            = delete;
        Logger& operator=(Logger&&) = delete;

        /** Emit a pre-formed message. No-op if no callback is set. */
        void log(std::string_view msg) const {
            if (!fn_) return;
            std::lock_guard lk(mtx_);
            fn_(msg);
        }

        /**
         * Format any number of streamable arguments into a single message,
         * then emit it atomically.
         *
         *   logger.logf("chunk ", idx, " of ", total, " done");
         */
        template <typename... Args>
        void logf(Args&&... args) const {
            if (!fn_) return;

            // Build the string before acquiring the lock to keep the
            // critical section as short as possible.
            std::ostringstream ss;
            (ss << ... << std::forward<Args>(args));

            log(ss.str());
        }

        /** Returns true when a callback is installed. */
        [[nodiscard]] bool active() const noexcept { return fn_ != nullptr; }

    private:
        mutable std::mutex mtx_;
        LogFn              fn_;
    };

} // namespace pb