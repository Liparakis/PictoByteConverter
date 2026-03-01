#pragma once
#include <string>
#include <functional>
#include <mutex>
#include <string_view>
#include <type_traits>

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

        Logger(const Logger&)            = delete;
        Logger& operator=(const Logger&) = delete;
        Logger(Logger&&)                 = delete;
        Logger& operator=(Logger&&)      = delete;

        /** Emit a pre-formed message. No-op if no callback is set. */
        void log(std::string_view msg) const {
            if (!fn_) return;
            std::lock_guard lk(mtx_);
            fn_(msg);
        }

        /**
         * Format any number of streamable arguments into a single message,
         * then emit it atomically.  Uses direct string concatenation instead
         * of std::ostringstream to avoid locale overhead and extra allocations.
         */
        template <typename... Args>
        void logf(Args&&... args) const {
            if (!fn_) return;
            std::string msg;
            msg.reserve(128);
            (append_to(msg, std::forward<Args>(args)), ...);
            log(msg);
        }

        [[nodiscard]] bool active() const noexcept { return fn_ != nullptr; }

    private:
        // Type-dispatched append — covers every type used in the codebase
        // without pulling in <sstream>.
        template <typename T>
        static void append_to(std::string& out, T&& val) {
            using U = std::decay_t<T>;
            if constexpr (std::is_same_v<U, std::string>) {
                out += val;
            } else if constexpr (std::is_same_v<U, std::string_view>) {
                out.append(val.data(), val.size());
            } else if constexpr (std::is_same_v<U, const char*> || std::is_same_v<U, char*>) {
                out += val;
            } else if constexpr (std::is_same_v<U, char>) {
                out += val;
            } else if constexpr (std::is_arithmetic_v<U>) {
                out += std::to_string(val);
            } else {
                static_assert(sizeof(U) == 0, "Logger::logf: unsupported argument type");
            }
        }

        mutable std::mutex mtx_;
        LogFn              fn_;
    };

} // namespace pb
