#include "platform/process.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <fcntl.h>
#include <limits>
#include <poll.h>
#include <spawn.h>
#include <string>
#include <sys/wait.h>
#include <system_error>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

// POSIX defines environ but no header declares it; posix_spawn needs it to pass the
// environment on.
// NOLINTNEXTLINE(readability-redundant-declaration,cppcoreguidelines-avoid-non-const-global-variables)
extern char** environ;

namespace cadpreview {

namespace {

constexpr std::size_t READ_CHUNK = std::size_t{64} * 1024;

[[noreturn]] void throwErrno(int error, const char* what) {
    throw std::system_error(error, std::generic_category(), what);
}

class Fd {
public:
    Fd() = default;
    explicit Fd(int fd) noexcept : fd_(fd) {}
    ~Fd() { reset(); }
    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;
    Fd(Fd&& other) noexcept : fd_(std::exchange(other.fd_, -1)) {}
    Fd& operator=(Fd&& other) noexcept {
        if (this != &other) {
            reset();
            fd_ = std::exchange(other.fd_, -1);
        }
        return *this;
    }

    [[nodiscard]] int get() const noexcept { return fd_; }
    void reset() noexcept {
        if (fd_ >= 0) {
            close(fd_);
        }
        fd_ = -1;
    }

private:
    int fd_ = -1;
};

struct Pipe {
    Fd read;
    Fd write;
};

// Both ends are close-on-exec from the start, so a child spawned concurrently by another thread
// never inherits them; posix_spawn's dup2 clears the flag only on the child's stdout/stderr.
Pipe makePipe() {
    std::array<int, 2> fds{-1, -1};
#ifdef __linux__
    if (pipe2(fds.data(), O_CLOEXEC) != 0) {
        throwErrno(errno, "cannot create a pipe");
    }
#else
    // Not atomic: another thread's spawn could inherit the ends in between. Linux, the only
    // POSIX platform built today, takes the pipe2 branch above.
    if (pipe(fds.data()) != 0) {
        throwErrno(errno, "cannot create a pipe");
    }
    if (fcntl(fds[0], F_SETFD, FD_CLOEXEC) != 0 || fcntl(fds[1], F_SETFD, FD_CLOEXEC) != 0) {
        const int error = errno;
        close(fds[0]);
        close(fds[1]);
        throwErrno(error, "cannot create a pipe");
    }
#endif
    return {Fd(fds[0]), Fd(fds[1])};
}

class FileActions {
public:
    FileActions() {
        if (const int error = posix_spawn_file_actions_init(&actions_); error != 0) {
            throwErrno(error, "cannot prepare the child process");
        }
    }
    ~FileActions() { posix_spawn_file_actions_destroy(&actions_); }
    FileActions(const FileActions&) = delete;
    FileActions& operator=(const FileActions&) = delete;
    FileActions(FileActions&&) = delete;
    FileActions& operator=(FileActions&&) = delete;

    void redirect(int from, int to) {
        check(posix_spawn_file_actions_adddup2(&actions_, from, to));
    }
    void nullInput() {
        check(posix_spawn_file_actions_addopen(&actions_, STDIN_FILENO, "/dev/null", O_RDONLY, 0));
    }
    // Closes every descriptor above stderr in the child, including ones the host left without
    // close-on-exec, so the process parsing untrusted input sees nothing of the host's.
    void closeOthers() {
#if defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 34))
        check(posix_spawn_file_actions_addclosefrom_np(&actions_, STDERR_FILENO + 1));
#endif
    }
    [[nodiscard]] const posix_spawn_file_actions_t* get() const noexcept { return &actions_; }

private:
    static void check(int error) {
        if (error != 0) {
            throwErrno(error, "cannot prepare the child process");
        }
    }
    posix_spawn_file_actions_t actions_{};
};

// Starts the child with an empty signal mask, default signal handling (the host may ignore
// SIGPIPE or block signals), and its own process group, so a kill reaches anything it starts.
class SpawnAttributes {
public:
    SpawnAttributes() {
        check(posix_spawnattr_init(&attributes_));
        sigset_t none;
        sigset_t all;
        sigemptyset(&none);
        sigfillset(&all);
        check(posix_spawnattr_setsigmask(&attributes_, &none));
        check(posix_spawnattr_setsigdefault(&attributes_, &all));
        check(posix_spawnattr_setpgroup(&attributes_, 0));
        check(posix_spawnattr_setflags(
            &attributes_, POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETSIGDEF | POSIX_SPAWN_SETPGROUP));
    }
    ~SpawnAttributes() { posix_spawnattr_destroy(&attributes_); }
    SpawnAttributes(const SpawnAttributes&) = delete;
    SpawnAttributes& operator=(const SpawnAttributes&) = delete;
    SpawnAttributes(SpawnAttributes&&) = delete;
    SpawnAttributes& operator=(SpawnAttributes&&) = delete;

    [[nodiscard]] const posix_spawnattr_t* get() const noexcept { return &attributes_; }

private:
    static void check(int error) {
        if (error != 0) {
            throwErrno(error, "cannot prepare the child process");
        }
    }
    posix_spawnattr_t attributes_{};
};

pid_t spawn(const std::filesystem::path& program, const std::vector<std::string>& arguments,
            const Pipe& output, const Pipe& error) {
    FileActions actions;
    actions.nullInput();
    actions.redirect(output.write.get(), STDOUT_FILENO);
    actions.redirect(error.write.get(), STDERR_FILENO);
    actions.closeOthers();
    const SpawnAttributes attributes;
    std::vector<std::string> argv{program.string()};
    argv.insert(argv.end(), arguments.begin(), arguments.end());
    std::vector<char*> pointers;
    pointers.reserve(argv.size() + 1);
    for (std::string& argument : argv) {
        pointers.push_back(argument.data());
    }
    pointers.push_back(nullptr);
    pid_t pid = 0;
    if (const int failed = posix_spawn(&pid, program.c_str(), actions.get(), attributes.get(),
                                       pointers.data(), environ);
        failed != 0) {
        throwErrno(failed, "cannot start the program");
    }
    return pid;
}

struct Stream {
    int fd = -1;
    std::string* into = nullptr; // never null once constructed by runProcess
    std::size_t limit = 0;
    bool open = true;
};

// Reads what is available on one stream. Returns false if it exceeded its limit.
bool readSome(Stream& stream) {
    std::array<char, READ_CHUNK> buffer{};
    const ssize_t got = read(stream.fd, buffer.data(), buffer.size());
    if (got < 0 && errno == EINTR) {
        return true;
    }
    if (got <= 0) {
        stream.open = false;
        return true;
    }
    const auto count = static_cast<std::size_t>(got);
    const std::size_t room = stream.limit - std::min(stream.limit, stream.into->size());
    stream.into->append(buffer.data(), std::min(room, count));
    return count <= room;
}

// Reads both streams until they close or a limit is hit. Returns how the reading ended.
ProcessEnd collect(Stream& output, Stream& error, std::chrono::steady_clock::time_point deadline) {
    while (output.open || error.open) {
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now());
        if (left.count() <= 0) {
            return ProcessEnd::TimedOut;
        }
        std::array<pollfd, 2> polls{pollfd{output.open ? output.fd : -1, POLLIN, 0},
                                    pollfd{error.open ? error.fd : -1, POLLIN, 0}};
        const auto waitMs =
            static_cast<int>(std::min<long long>(left.count(), std::numeric_limits<int>::max()));
        const int ready = poll(polls.data(), polls.size(), waitMs);
        if (ready < 0 && errno != EINTR) {
            throwErrno(errno, "cannot wait for the child process");
        }
        if (ready <= 0) {
            continue;
        }
        if (polls[0].revents != 0 && !readSome(output)) {
            return ProcessEnd::OutputTooLarge;
        }
        if (polls[1].revents != 0) {
            static_cast<void>(readSome(error)); // error output beyond its limit is dropped
        }
    }
    return ProcessEnd::Exited;
}

enum class Reap : std::uint8_t {
    Done,    ///< reaped; the status is valid
    Running, ///< still running (only with WNOHANG)
    Lost,    ///< cannot be reaped: the host ignores SIGCHLD or another waiter took the child
};

Reap reap(pid_t pid, int options, int& status) {
    for (;;) {
        const pid_t done = waitpid(pid, &status, options);
        if (done == pid) {
            return Reap::Done;
        }
        if (done == 0) {
            return Reap::Running;
        }
        if (errno != EINTR) {
            return Reap::Lost;
        }
    }
}

// Kills the child's whole process group (it leads its own group). Call only before the child
// has been reaped.
void killGroup(pid_t pid) noexcept {
    kill(-pid, SIGKILL);
    kill(pid, SIGKILL);
}

// Waits for a child that has closed its output, but no longer than the deadline.
Reap reapBy(pid_t pid, std::chrono::steady_clock::time_point deadline, int& status) {
    constexpr auto POLL_INTERVAL = std::chrono::milliseconds(5);
    for (;;) {
        const Reap state = reap(pid, WNOHANG, status);
        if (state != Reap::Running || std::chrono::steady_clock::now() >= deadline) {
            return state;
        }
        std::this_thread::sleep_for(POLL_INTERVAL);
    }
}

void classify(ProcessResult& result, Reap state, int status) noexcept {
    if (state == Reap::Lost) {
        // The exit status is unknown, so the output cannot be trusted as complete.
        if (result.end == ProcessEnd::Exited) {
            result.end = ProcessEnd::Crashed;
        }
        result.exitCode = -1;
    } else if (result.end == ProcessEnd::Exited && WIFSIGNALED(status)) {
        result.end = ProcessEnd::Crashed;
        result.exitCode = WTERMSIG(status);
    } else if (WIFEXITED(status)) {
        result.exitCode = WEXITSTATUS(status);
    }
}

} // namespace

ProcessResult runProcess(const std::filesystem::path& program,
                         const std::vector<std::string>& arguments, const ProcessLimits& limits) {
    Pipe output = makePipe();
    Pipe error = makePipe();
    const pid_t pid = spawn(program, arguments, output, error);
    output.write.reset();
    error.write.reset();

    ProcessResult result;
    Stream out{output.read.get(), &result.output, limits.maxOutput};
    Stream err{error.read.get(), &result.errorOutput, limits.maxErrorOutput};
    const auto deadline = std::chrono::steady_clock::now() + limits.timeout;
    int status = 0;
    try {
        result.end = collect(out, err, deadline);
    } catch (...) {
        killGroup(pid);
        static_cast<void>(reap(pid, 0, status));
        throw;
    }
    Reap state = Reap::Running;
    if (result.end == ProcessEnd::Exited) {
        state = reapBy(pid, deadline, status);
        if (state == Reap::Running) {
            result.end = ProcessEnd::TimedOut;
        }
    }
    // The group is killed only while the child is not yet reaped: until then its PID (and so
    // the group id) cannot be reused by an unrelated process. A child that ran into a limit is
    // always unreaped here, so whatever it started dies with it.
    if (state == Reap::Running) {
        killGroup(pid);
        state = reap(pid, 0, status);
    }
    classify(result, state, status);
    return result;
}

} // namespace cadpreview
