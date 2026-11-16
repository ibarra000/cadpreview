#include "platform/process.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <future>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>
#include <windows.h>

namespace cadpreview {

namespace {

constexpr DWORD READ_CHUNK = 64 * 1024;
constexpr DWORD KILLED_STATUS = 1;
// After the child has ended, its output pipes should close at once. If something else still
// holds a write end (a process another host thread started while our handles were briefly
// inheritable), blocked reads are cancelled after this grace period.
constexpr std::chrono::milliseconds READER_GRACE{2000};
constexpr std::chrono::milliseconds CANCEL_RETRY{20};
// Exit statuses at or above this are NTSTATUS error codes (access violation, stack overflow...).
constexpr DWORD FIRST_EXCEPTION_STATUS = 0xC0000000U;

[[noreturn]] void throwLastError(const char* what) {
    throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), what);
}

class Handle {
public:
    Handle() = default;
    explicit Handle(HANDLE handle) noexcept : handle_(handle) {}
    ~Handle() { reset(); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    Handle(Handle&& other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}
    Handle& operator=(Handle&& other) noexcept {
        if (this != &other) {
            reset();
            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }

    [[nodiscard]] HANDLE get() const noexcept { return handle_; }
    HANDLE* out() noexcept { return &handle_; }
    void reset() noexcept {
        if (handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(handle_);
        }
        handle_ = nullptr;
    }

private:
    HANDLE handle_ = nullptr;
};

std::wstring widen(std::string_view utf8) {
    if (utf8.empty()) {
        return {};
    }
    if (utf8.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::system_error(std::make_error_code(std::errc::argument_list_too_long));
    }
    const int length = static_cast<int>(utf8.size());
    const char* first = std::to_address(utf8.begin());
    const int needed =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, first, length, nullptr, 0);
    if (needed <= 0) {
        throwLastError("argument is not valid UTF-8");
    }
    std::wstring wide(static_cast<std::size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, first, length, wide.data(), needed);
    return wide;
}

// Quotes one argument so that the C runtime's command-line parser (CommandLineToArgvW rules)
// gives it back unchanged: backslashes are literal except before a quote.
void appendQuoted(std::wstring& line, std::wstring_view argument) {
    if (!argument.empty() && argument.find_first_of(L" \t\n\v\"") == std::wstring_view::npos) {
        line.append(argument);
        return;
    }
    line.push_back(L'"');
    std::size_t backslashes = 0;
    for (const wchar_t c : argument) {
        if (c == L'\\') {
            ++backslashes;
            continue;
        }
        // Backslashes before a quote are doubled, and the quote itself is escaped.
        line.append(c == L'"' ? (backslashes * 2) + 1 : backslashes, L'\\');
        line.push_back(c);
        backslashes = 0;
    }
    line.append(backslashes * 2, L'\\'); // doubled before the closing quote
    line.push_back(L'"');
}

std::wstring commandLine(const std::filesystem::path& program,
                         const std::vector<std::string>& arguments) {
    std::wstring line;
    appendQuoted(line, program.native());
    for (const std::string& argument : arguments) {
        line.push_back(L' ');
        appendQuoted(line, widen(argument));
    }
    return line;
}

struct Pipe {
    Handle read;
    Handle write; // handed to the child
};

// Handles are created non-inheritable; see InheritableDuring for the one moment they are.
Pipe makePipe() {
    Pipe pipe;
    if (CreatePipe(pipe.read.out(), pipe.write.out(), nullptr, 0) == 0) {
        throwLastError("cannot create a pipe");
    }
    return pipe;
}

Handle openNullInput() {
    Handle null(CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                            OPEN_EXISTING, 0, nullptr));
    if (null.get() == INVALID_HANDLE_VALUE) {
        throwLastError("cannot open the null device");
    }
    return null;
}

// Makes the child's handles inheritable only around CreateProcessW. The handle list restricts
// our child to exactly these handles; keeping the window short limits what a CreateProcess call
// on another host thread could pick up meanwhile.
class InheritableDuring {
public:
    explicit InheritableDuring(std::array<HANDLE, 3> handles) : handles_(handles) {
        for (HANDLE handle : handles_) {
            if (SetHandleInformation(handle, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT) == 0) {
                clear();
                throwLastError("cannot pass handles to the child");
            }
        }
    }
    ~InheritableDuring() { clear(); }
    InheritableDuring(const InheritableDuring&) = delete;
    InheritableDuring& operator=(const InheritableDuring&) = delete;
    InheritableDuring(InheritableDuring&&) = delete;
    InheritableDuring& operator=(InheritableDuring&&) = delete;

private:
    void clear() noexcept {
        for (HANDLE handle : handles_) {
            SetHandleInformation(handle, HANDLE_FLAG_INHERIT, 0);
        }
    }
    std::array<HANDLE, 3> handles_;
};

// A job that holds exactly one process and kills it when the job handle closes, so neither a
// timeout nor an exception can leave the child (or anything it starts) running.
Handle makeJob() {
    Handle job(CreateJobObjectW(nullptr, nullptr));
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit{};
    limit.BasicLimitInformation.LimitFlags =
        JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_ACTIVE_PROCESS;
    limit.BasicLimitInformation.ActiveProcessLimit = 1;
    if (job.get() == nullptr ||
        SetInformationJobObject(job.get(), JobObjectExtendedLimitInformation, &limit,
                                sizeof(limit)) == 0) {
        throwLastError("cannot create a job object");
    }
    return job;
}

// Restricts inheritance to exactly the given handles (PROC_THREAD_ATTRIBUTE_HANDLE_LIST), so a
// child started concurrently by another thread cannot receive these pipes.
class InheritList {
public:
    explicit InheritList(std::array<HANDLE, 3> handles) : handles_(handles) {
        SIZE_T size = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
        storage_.resize(size);
        list_ = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>( // NOLINT: Win32 opaque buffer
            storage_.data());
        if (InitializeProcThreadAttributeList(list_, 1, 0, &size) == 0) {
            throwLastError("cannot prepare process attributes");
        }
        if (UpdateProcThreadAttribute(list_, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                      static_cast<void*>(handles_.data()),
                                      sizeof(HANDLE) * handles_.size(), nullptr, nullptr) == 0) {
            DeleteProcThreadAttributeList(list_);
            throwLastError("cannot prepare process attributes");
        }
    }
    ~InheritList() { DeleteProcThreadAttributeList(list_); }
    InheritList(const InheritList&) = delete;
    InheritList& operator=(const InheritList&) = delete;
    InheritList(InheritList&&) = delete;
    InheritList& operator=(InheritList&&) = delete;

    [[nodiscard]] LPPROC_THREAD_ATTRIBUTE_LIST get() const noexcept { return list_; }

private:
    std::array<HANDLE, 3> handles_;
    std::vector<std::byte> storage_;
    LPPROC_THREAD_ATTRIBUTE_LIST list_ = nullptr;
};

struct Child {
    Handle job;
    Handle process;
    Pipe output;
    Pipe error;
};

// Creates the process suspended, places it in the job, then lets it run.
void launch(Child& child, const std::filesystem::path& program, std::wstring line,
            STARTUPINFOEXW& startup, const std::array<HANDLE, 3>& handles) {
    PROCESS_INFORMATION info{};
    {
        const InheritableDuring inheritable(handles);
        if (CreateProcessW(program.c_str(), line.data(), nullptr, nullptr, TRUE,
                           EXTENDED_STARTUPINFO_PRESENT | CREATE_NO_WINDOW |
                               CREATE_UNICODE_ENVIRONMENT | CREATE_SUSPENDED,
                           nullptr, nullptr, &startup.StartupInfo, &info) == 0) {
            throwLastError("cannot start the program");
        }
    }
    const Handle thread(info.hThread);
    child.process = Handle(info.hProcess);
    if (AssignProcessToJobObject(child.job.get(), info.hProcess) == 0) {
        const DWORD error = GetLastError();
        TerminateProcess(info.hProcess, KILLED_STATUS);
        throw std::system_error(static_cast<int>(error), std::system_category(),
                                "cannot place the program in a job");
    }
    ResumeThread(thread.get());
}

Child start(const std::filesystem::path& program, const std::vector<std::string>& arguments) {
    Child child{makeJob(), Handle{}, makePipe(), makePipe()};
    const Handle input = openNullInput();
    const std::array<HANDLE, 3> handles{input.get(), child.output.write.get(),
                                        child.error.write.get()};
    const InheritList inherit(handles);

    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = input.get();
    startup.StartupInfo.hStdOutput = child.output.write.get();
    startup.StartupInfo.hStdError = child.error.write.get();
    startup.lpAttributeList = inherit.get();

    launch(child, program, commandLine(program, arguments), startup, handles);
    // The parent's copies must close, or the pipes never report end of output.
    child.output.write.reset();
    child.error.write.reset();
    return child;
}

// Reads a pipe to its end, keeping at most `limit` bytes. Returns false if more arrived.
bool drain(HANDLE pipe, std::string& into, std::size_t limit, bool stopWhenFull) noexcept {
    std::array<char, READ_CHUNK> buffer{};
    bool fits = true;
    DWORD got = 0;
    while (ReadFile(pipe, buffer.data(), READ_CHUNK, &got, nullptr) != 0 && got > 0) {
        try {
            const std::size_t room = limit - std::min(limit, into.size());
            into.append(buffer.data(), std::min<std::size_t>(room, got));
            fits = fits && got <= room;
        } catch (...) {
            fits = false;
        }
        if (!fits && stopWhenFull) {
            return false;
        }
    }
    return fits;
}

// Runs a pipe reader on its own thread. finish() waits for it, cancelling its blocked read if
// the pipe does not close within a grace period, so joining can never hang.
class Reader {
public:
    template <class Body>
    explicit Reader(Body body)
        : thread_([this, body = std::move(body)]() mutable {
              thread_id_ = GetCurrentThreadId();
              body();
              done_.set_value();
          }) {}
    ~Reader() { finish(std::chrono::milliseconds{0}); }
    Reader(const Reader&) = delete;
    Reader& operator=(const Reader&) = delete;
    Reader(Reader&&) = delete;
    Reader& operator=(Reader&&) = delete;

    void finish(std::chrono::milliseconds grace) noexcept {
        if (!thread_.joinable()) {
            return;
        }
        try {
            if (finished_.wait_for(grace) != std::future_status::ready) {
                // std::thread::native_handle is not a HANDLE with every runtime, so open one.
                // The id is set once the thread runs; until then there is nothing to cancel.
                Handle thread;
                while (finished_.wait_for(CANCEL_RETRY) != std::future_status::ready) {
                    if (thread.get() == nullptr && thread_id_ != 0) {
                        thread = Handle(OpenThread(THREAD_TERMINATE, FALSE, thread_id_));
                    }
                    CancelSynchronousIo(thread.get());
                }
            }
            thread_.join();
        } catch (...) { // NOLINT(bugprone-empty-catch): join cannot fail for a joinable thread
        }
    }

private:
    std::promise<void> done_;
    std::future<void> finished_ = done_.get_future();
    std::atomic<DWORD> thread_id_{0};
    std::thread thread_;
};

// Ends the job (and with it the child) when the scope is left, normally or by an exception.
class KillOnExit {
public:
    explicit KillOnExit(HANDLE job) noexcept : job_(job) {}
    ~KillOnExit() { TerminateJobObject(job_, KILLED_STATUS); }
    KillOnExit(const KillOnExit&) = delete;
    KillOnExit& operator=(const KillOnExit&) = delete;
    KillOnExit(KillOnExit&&) = delete;
    KillOnExit& operator=(KillOnExit&&) = delete;

private:
    HANDLE job_;
};

DWORD waitMilliseconds(std::chrono::milliseconds timeout) noexcept {
    return static_cast<DWORD>(
        std::clamp<long long>(timeout.count(), 0, std::numeric_limits<DWORD>::max() - 1));
}

ProcessEnd classify(bool tooLarge, DWORD status) noexcept {
    if (tooLarge) {
        return ProcessEnd::OutputTooLarge;
    }
    return status >= FIRST_EXCEPTION_STATUS ? ProcessEnd::Crashed : ProcessEnd::Exited;
}

} // namespace

ProcessResult runProcess(const std::filesystem::path& program,
                         const std::vector<std::string>& arguments, const ProcessLimits& limits) {
    Child child = start(program, arguments);
    ProcessResult result;
    std::atomic<bool> tooLarge{false};
    void* const job = child.job.get(); // a HANDLE
    Reader output([&] {
        if (!drain(child.output.read.get(), result.output, limits.maxOutput, true)) {
            tooLarge = true;
            TerminateJobObject(job, KILLED_STATUS);
        }
    });
    Reader error([&] {
        static_cast<void>(
            drain(child.error.read.get(), result.errorOutput, limits.maxErrorOutput, false));
    });
    {
        const KillOnExit kill(job); // also on exceptions, before the readers are joined
        const DWORD waited =
            WaitForSingleObject(child.process.get(), waitMilliseconds(limits.timeout));
        if (waited == WAIT_FAILED) {
            throwLastError("cannot wait for the program");
        }
        if (waited == WAIT_TIMEOUT) {
            result.end = ProcessEnd::TimedOut;
        }
    }
    WaitForSingleObject(child.process.get(), INFINITE); // ends promptly: the job was terminated
    output.finish(READER_GRACE);
    error.finish(READER_GRACE);
    DWORD status = 0;
    GetExitCodeProcess(child.process.get(), &status);
    if (result.end != ProcessEnd::TimedOut) {
        result.end = classify(tooLarge, status);
    }
    result.exitCode = static_cast<int>(status);
    return result;
}

} // namespace cadpreview
