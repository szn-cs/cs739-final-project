#pragma once

#include "library.h"
#include "rpc.h"
#include "struct.h"
#include "utility.h"

/*
   ____  _____ ____  _   _  ____   _     ___   ____  ____ _____ ____
  |  _ \| ____| __ )| | | |/ ___| | |   / _ \ / ___|/ ___| ____|  _ \
  | | | |  _| |  _ \| | | | |  _  | |  | | | | |  _| |  _|  _| | |_) |
  | |_| | |___| |_) | |_| | |_| | | |__| |_| | |_| | |_| | |___|  _ <
  |____/|_____|____/ \___/ \____| |_____\___/ \____|\____|_____|_| \_\
*/

namespace app::consensus::_logger::_backtrace {
  // LCOV_EXCL_START

#define SIZE_T_UNUSED size_t __attribute__((unused))
#define VOID_UNUSED void __attribute__((unused))
#define UINT64_T_UNUSED uint64_t __attribute__((unused))
#define STR_UNUSED std::string __attribute__((unused))
#define INTPTR_UNUSED intptr_t __attribute__((unused))

#define _snprintf(msg, avail_len, cur_len, msg_len, ...)         \
  avail_len = (avail_len > cur_len) ? (avail_len - cur_len) : 0; \
  msg_len = snprintf(msg + cur_len, avail_len, __VA_ARGS__);     \
  cur_len += (avail_len > msg_len) ? msg_len : avail_len

  static SIZE_T_UNUSED _stack_backtrace(void** stack_ptr, size_t stack_ptr_capacity) {
    return backtrace(stack_ptr, stack_ptr_capacity);
  }

  static SIZE_T_UNUSED _stack_interpret_linux(void** stack_ptr, char** stack_msg, int stack_size, char* output_buf, size_t output_buflen);

  static SIZE_T_UNUSED _stack_interpret_apple(void** stack_ptr, char** stack_msg, int stack_size, char* output_buf, size_t output_buflen);

  static SIZE_T_UNUSED _stack_interpret_other(void** stack_ptr, char** stack_msg, int stack_size, char* output_buf, size_t output_buflen);

  static SIZE_T_UNUSED _stack_interpret(void** stack_ptr, int stack_size, char* output_buf, size_t output_buflen) {
    char** stack_msg = nullptr;
    stack_msg = backtrace_symbols(stack_ptr, stack_size);

    size_t len = 0;

    len = _stack_interpret_linux(
        stack_ptr,
        stack_msg,
        stack_size,
        output_buf,
        output_buflen);

    free(stack_msg);

    return len;
  }

  static SIZE_T_UNUSED _stack_interpret_linux(void** stack_ptr, char** stack_msg, int stack_size, char* output_buf, size_t output_buflen) {
    size_t cur_len = 0;
    size_t frame_num = 0;

    // NOTE: starting from 1, skipping this frame.
    for (int i = 1; i < stack_size; ++i) {
      // `stack_msg[x]` format:
      //   /foo/bar/executable() [0xabcdef]
      //   /lib/x86_64-linux-gnu/libc.so.6(__libc_start_main+0xf0) [0x123456]

      // NOTE: with ASLR
      //   /foo/bar/executable(+0x5996) [0x555555559996]

      int fname_len = 0;
      while (stack_msg[i][fname_len] != '(' && stack_msg[i][fname_len] != ' ' && stack_msg[i][fname_len] != 0x0) {
        ++fname_len;
      }

      char addr_str[256];
      uintptr_t actual_addr = 0x0;
      if (stack_msg[i][fname_len] == '(' && stack_msg[i][fname_len + 1] == '+') {
        // ASLR is enabled, get the offset from here.
        int upto = fname_len + 2;
        while (stack_msg[i][upto] != ')' && stack_msg[i][upto] != 0x0) {
          upto++;
        }
        snprintf(addr_str, 256, "%.*s", upto - fname_len - 2, &stack_msg[i][fname_len + 2]);

        // Convert hex string -> integer address.
        std::stringstream ss;
        ss << std::hex << addr_str;
        ss >> actual_addr;

      } else {
        actual_addr = (uintptr_t)stack_ptr[i];
        snprintf(addr_str, 256, "%" PRIxPTR, actual_addr);
      }

      char cmd[1024];
      snprintf(cmd, 1024, "addr2line -f -e %.*s %s", fname_len, stack_msg[i], addr_str);
      FILE* fp = popen(cmd, "r");
      if (!fp) continue;

      char mangled_name[1024];
      char file_line[1024];
      int ret = fscanf(fp, "%1023s %1023s", mangled_name, file_line);
      (void)ret;
      pclose(fp);

      size_t msg_len = 0;
      size_t avail_len = output_buflen;
      _snprintf(output_buf, avail_len, cur_len, msg_len, "#%-2zu 0x%016" PRIxPTR " in ", frame_num++, actual_addr);

      int status;
      char* cc = abi::__cxa_demangle(mangled_name, 0, 0, &status);
      if (cc) {
        _snprintf(output_buf, avail_len, cur_len, msg_len, "%s at ", cc);
      } else {
        std::string msg_str = stack_msg[i];
        std::string _func_name = msg_str;
        size_t s_pos = msg_str.find("(");
        size_t e_pos = msg_str.rfind("+");
        if (e_pos == std::string::npos) e_pos = msg_str.rfind(")");
        if (s_pos != std::string::npos && e_pos != std::string::npos) {
          _func_name = msg_str.substr(s_pos + 1, e_pos - s_pos - 1);
        }
        _snprintf(output_buf, avail_len, cur_len, msg_len, "%s() at ", (_func_name.empty() ? mangled_name : _func_name.c_str()));
      }

      _snprintf(output_buf, avail_len, cur_len, msg_len, "%s\n", file_line);
    }

    return cur_len;
  }

  static VOID_UNUSED skip_whitespace(const std::string base_str, size_t& cursor) {
    while (base_str[cursor] == ' ')
      cursor++;
  }

  static VOID_UNUSED skip_glyph(const std::string base_str, size_t& cursor) {
    while (base_str[cursor] != ' ')
      cursor++;
  }

  static SIZE_T_UNUSED _stack_interpret_apple(void** stack_ptr, char** stack_msg, int stack_size, char* output_buf, size_t output_buflen) {
    size_t cur_len = 0;
    return cur_len;
  }

  static SIZE_T_UNUSED _stack_interpret_other(void** stack_ptr, char** stack_msg, int stack_size, char* output_buf, size_t output_buflen) {
    size_t cur_len = 0;
    size_t frame_num = 0;
    (void)frame_num;

    // NOTE: starting from 1, skipping this frame.
    for (int i = 1; i < stack_size; ++i) {
      // On non-Linux platform, just use the raw symbols.
      size_t msg_len = 0;
      size_t avail_len = output_buflen;
      _snprintf(output_buf, avail_len, cur_len, msg_len, "%s\n", stack_msg[i]);
    }
    return cur_len;
  }

  static SIZE_T_UNUSED stack_backtrace(char* output_buf, size_t output_buflen) {
    void* stack_ptr[256];
    int stack_size = _stack_backtrace(stack_ptr, 256);
    return _stack_interpret(stack_ptr, stack_size, output_buf, output_buflen);
  }

  // LCOV_EXCL_STOP
}  // namespace app::consensus::_logger::_backtrace

#ifndef _CLM_DEFINED
#define _CLM_DEFINED (1)

#ifdef LOGGER_NO_COLOR
#define _CLM_D_GRAY ""
#define _CLM_GREEN ""
#define _CLM_B_GREEN ""
#define _CLM_RED ""
#define _CLM_B_RED ""
#define _CLM_BROWN ""
#define _CLM_B_BROWN ""
#define _CLM_BLUE ""
#define _CLM_B_BLUE ""
#define _CLM_MAGENTA ""
#define _CLM_B_MAGENTA ""
#define _CLM_CYAN ""
#define _CLM_END ""

#define _CLM_WHITE_FG_RED_BG ""
#else
#define _CLM_D_GRAY "\033[1;30m"
#define _CLM_GREEN "\033[32m"
#define _CLM_B_GREEN "\033[1;32m"
#define _CLM_RED "\033[31m"
#define _CLM_B_RED "\033[1;31m"
#define _CLM_BROWN "\033[33m"
#define _CLM_B_BROWN "\033[1;33m"
#define _CLM_BLUE "\033[34m"
#define _CLM_B_BLUE "\033[1;34m"
#define _CLM_MAGENTA "\033[35m"
#define _CLM_B_MAGENTA "\033[1;35m"
#define _CLM_CYAN "\033[36m"
#define _CLM_B_GREY "\033[1;37m"
#define _CLM_END "\033[0m"

#define _CLM_WHITE_FG_RED_BG "\033[37;41m"
#endif

#define _CL_D_GRAY(str) _CLM_D_GRAY str _CLM_END
#define _CL_GREEN(str) _CLM_GREEN str _CLM_END
#define _CL_RED(str) _CLM_RED str _CLM_END
#define _CL_B_RED(str) _CLM_B_RED str _CLM_END
#define _CL_MAGENTA(str) _CLM_MAGENTA str _CLM_END
#define _CL_BROWN(str) _CLM_BROWN str _CLM_END
#define _CL_B_BROWN(str) _CLM_B_BROWN str _CLM_END
#define _CL_B_BLUE(str) _CLM_B_BLUE str _CLM_END
#define _CL_B_MAGENTA(str) _CLM_B_MAGENTA str _CLM_END
#define _CL_CYAN(str) _CLM_CYAN str _CLM_END
#define _CL_B_GRAY(str) _CLM_B_GREY str _CLM_END

#define _CL_WHITE_FG_RED_BG(str) _CLM_WHITE_FG_RED_BG str _CLM_END

#endif

namespace app::consensus::_logger {
  using namespace app::consensus::_logger::_backtrace;

// To suppress false alarms by thread sanitizer,
// add -DSUPPRESS_TSAN_FALSE_ALARMS=1 flag to CXXFLAGS.
// #define SUPPRESS_TSAN_FALSE_ALARMS (1)

// 0: System  [====]
// 1: Fatal   [FATL]
// 2: Error   [ERRO]
// 3: Warning [WARN]
// 4: Info    [INFO]
// 5: Debug   [DEBG]
// 6: Trace   [TRAC]

// printf style log macro
#define _log_(level, l, ...)          \
  if (l && l->getLogLevel() >= level) \
  (l)->put(level, __FILE__, __func__, __LINE__, __VA_ARGS__)

#define _log_sys(l, ...) _log_(SimpleLogger::SYS, l, __VA_ARGS__)
#define _log_fatal(l, ...) _log_(SimpleLogger::FATAL, l, __VA_ARGS__)
#define _log_err(l, ...) _log_(SimpleLogger::ERROR, l, __VA_ARGS__)
#define _log_warn(l, ...) _log_(SimpleLogger::WARNING, l, __VA_ARGS__)
#define _log_info(l, ...) _log_(SimpleLogger::INFO, l, __VA_ARGS__)
#define _log_debug(l, ...) _log_(SimpleLogger::DEBUG, l, __VA_ARGS__)
#define _log_trace(l, ...) _log_(SimpleLogger::TRACE, l, __VA_ARGS__)

// stream log macro
#define _stream_(level, l)            \
  if (l && l->getLogLevel() >= level) \
  l->eos() = l->stream(level, l, __FILE__, __func__, __LINE__)

#define _s_sys(l) _stream_(SimpleLogger::SYS, l)
#define _s_fatal(l) _stream_(SimpleLogger::FATAL, l)
#define _s_err(l) _stream_(SimpleLogger::ERROR, l)
#define _s_warn(l) _stream_(SimpleLogger::WARNING, l)
#define _s_info(l) _stream_(SimpleLogger::INFO, l)
#define _s_debug(l) _stream_(SimpleLogger::DEBUG, l)
#define _s_trace(l) _stream_(SimpleLogger::TRACE, l)

// Do printf style log, but print logs in `lv1` level during normal time,
// once in given `interval_ms` interval, print a log in `lv2` level.
// The very first log will be printed in `lv2` level.
//
// This function is global throughout the process, so that
// multiple threads will share the interval.
#define _timed_log_g(l, interval_ms, lv1, lv2, ...)         \
  {                                                         \
    _timed_log_definition(static);                          \
    _timed_log_body(l, interval_ms, lv1, lv2, __VA_ARGS__); \
  }

// Same as `_timed_log_g` but per-thread level.
#define _timed_log_t(l, interval_ms, lv1, lv2, ...)         \
  {                                                         \
    _timed_log_definition(thread_local);                    \
    _timed_log_body(l, interval_ms, lv1, lv2, __VA_ARGS__); \
  }

#define _timed_log_definition(prefix)                         \
  prefix std::mutex timer_lock;                               \
  prefix bool first_event_fired = false;                      \
  prefix std::chrono::system_clock::time_point last_timeout = \
      std::chrono::system_clock::now();

#define _timed_log_body(l, interval_ms, lv1, lv2, ...)          \
  std::chrono::system_clock::time_point cur =                   \
      std::chrono::system_clock::now();                         \
  bool timeout = false;                                         \
  {                                                             \
    std::lock_guard<std::mutex> l(timer_lock);                  \
    std::chrono::duration<double> elapsed = cur - last_timeout; \
    if (elapsed.count() * 1000 > interval_ms ||                 \
        !first_event_fired) {                                   \
      cur = std::chrono::system_clock::now();                   \
      elapsed = cur - last_timeout;                             \
      if (elapsed.count() * 1000 > interval_ms ||               \
          !first_event_fired) {                                 \
        timeout = first_event_fired = true;                     \
        last_timeout = cur;                                     \
      }                                                         \
    }                                                           \
  }                                                             \
  if (timeout) {                                                \
    _log_(lv2, l, __VA_ARGS__);                                 \
  } else {                                                      \
    _log_(lv1, l, __VA_ARGS__);                                 \
  }

  class SimpleLoggerMgr;
  class SimpleLogger {
    friend class SimpleLoggerMgr;

   public:
    static const int MSG_SIZE = 4096;
    static const std::memory_order MOR = std::memory_order_relaxed;

    enum Levels {
      SYS = 0,
      FATAL = 1,
      ERROR = 2,
      WARNING = 3,
      INFO = 4,
      DEBUG = 5,
      TRACE = 6,
      UNKNOWN = 99,
    };

    class LoggerStream : public std::ostream {
     public:
      LoggerStream() : std::ostream(&buf), level(0), logger(nullptr), file(nullptr), func(nullptr), line(0) {}

      template <typename T>
      inline LoggerStream& operator<<(const T& data) {
        sStream << data;
        return *this;
      }

      using MyCout = std::basic_ostream<char, std::char_traits<char>>;
      typedef MyCout& (*EndlFunc)(MyCout&);
      inline LoggerStream& operator<<(EndlFunc func) {
        func(sStream);
        return *this;
      }

      inline void put() {
        if (logger) {
          logger->put(level, file, func, line, "%s", sStream.str().c_str());
        }
      }

      inline void setLogInfo(int _level, SimpleLogger* _logger, const char* _file, const char* _func, size_t _line) {
        sStream.str(std::string());
        level = _level;
        logger = _logger;
        file = _file;
        func = _func;
        line = _line;
      }

     private:
      std::stringbuf buf;
      std::stringstream sStream;
      int level;
      SimpleLogger* logger;
      const char* file;
      const char* func;
      size_t line;
    };

    class EndOfStmt {
     public:
      EndOfStmt() {}
      EndOfStmt(LoggerStream& src) { src.put(); }
      EndOfStmt& operator=(LoggerStream& src) {
        src.put();
        return *this;
      }
    };

    LoggerStream& stream(int level, SimpleLogger* logger, const char* file, const char* func, size_t line) {
      thread_local LoggerStream msg;
      msg.setLogInfo(level, logger, file, func, line);
      return msg;
    }

    EndOfStmt& eos() {
      thread_local EndOfStmt _eos;
      return _eos;
    }

   private:
    struct LogElem {
      enum Status {
        CLEAN = 0,
        WRITING = 1,
        DIRTY = 2,
        FLUSHING = 3,
      };

      LogElem();

      // True if dirty.
      bool needToFlush();

      // True if no other thread is working on it.
      bool available();

      int write(size_t _len, char* msg);
      int flush(std::ofstream& fs);

      size_t len;
      char ctx[MSG_SIZE];
      std::atomic<Status> status;
    };

   public:
    SimpleLogger(const std::string& file_path, size_t max_log_elems = 4096, uint64_t log_file_size_limit = 32 * 1024 * 1024, uint32_t max_log_files = 16);
    ~SimpleLogger();

    static void setCriticalInfo(const std::string& info_str);
    static void setCrashDumpPath(const std::string& path, bool origin_only = true);
    static void setStackTraceOriginOnly(bool origin_only);
    static void logStackBacktrace();

    static void shutdown();
    static std::string replaceString(const std::string& src_str, const std::string& before, const std::string& after);

    int start();
    int stop();

    inline bool traceAllowed() const { return (curLogLevel.load(MOR) >= 6); }
    inline bool debugAllowed() const { return (curLogLevel.load(MOR) >= 5); }

    void setLogLevel(int level);
    void setDispLevel(int level);
    void setMaxLogFiles(size_t max_log_files);

    inline int getLogLevel() const { return curLogLevel.load(MOR); }
    inline int getDispLevel() const { return curDispLevel.load(MOR); }

    void put(int level, const char* source_file, const char* func_name, size_t line_number, const char* format, ...);
    void flushAll();

   private:
    void calcTzGap();
    void findMinMaxRevNum(size_t& min_revnum_out, size_t& max_revnum_out);
    void findMinMaxRevNumInternal(bool& min_revnum_initialized, size_t& min_revnum, size_t& max_revnum, std::string& f_name);
    std::string getLogFilePath(size_t file_num) const;
    void execCmd(const std::string& cmd);
    void doCompression(size_t file_num);
    bool flush(size_t start_pos);

    std::string filePath;
    size_t minRevnum;
    size_t curRevnum;
    std::atomic<size_t> maxLogFiles;
    std::ofstream fs;

    uint64_t maxLogFileSize;
    std::atomic<uint32_t> numCompJobs;

    // Log up to `curLogLevel`, default: 6.
    // Disable: -1.
    std::atomic<int> curLogLevel;

    // Display (print out on terminal) up to `curDispLevel`,
    // default: 4 (do not print debug and trace).
    // Disable: -1.
    std::atomic<int> curDispLevel;

    std::mutex displayLock;

    int tzGap;
    std::atomic<uint64_t> cursor;
    std::vector<LogElem> logs;
    std::mutex flushingLogs;
  };

  // Singleton class
  class SimpleLoggerMgr {
   public:
    struct CompElem;

    struct TimeInfo {
      TimeInfo(std::tm* src);
      TimeInfo(std::chrono::system_clock::time_point now);
      int year;
      int month;
      int day;
      int hour;
      int min;
      int sec;
      int msec;
      int usec;
    };

    struct RawStackInfo {
      RawStackInfo() : tidHash(0), kernelTid(0), crashOrigin(false) {}
      uint32_t tidHash;
      uint64_t kernelTid;
      std::vector<void*> stackPtrs;
      bool crashOrigin;
    };

    static SimpleLoggerMgr* init();
    static SimpleLoggerMgr* get();
    static SimpleLoggerMgr* getWithoutInit();
    static void destroy();
    static int getTzGap();
    static void handleSegFault(int sig);
    static void handleSegAbort(int sig);
    static void handleStackTrace(int sig, siginfo_t* info, void* secret);
    static void flushWorker();
    static void compressWorker();

    void logStackBacktrace(size_t timeout_ms = 60 * 1000);
    void flushCriticalInfo();
    void enableOnlyOneDisplayer();
    void flushAllLoggers() { flushAllLoggers(0, std::string()); }
    void flushAllLoggers(int level, const std::string& msg);
    void addLogger(SimpleLogger* logger);
    void removeLogger(SimpleLogger* logger);
    void addThread(uint64_t tid);
    void removeThread(uint64_t tid);
    void addCompElem(SimpleLoggerMgr::CompElem* elem);
    void sleepFlusher(size_t ms);
    void sleepCompressor(size_t ms);
    bool chkTermination() const;
    void setCriticalInfo(const std::string& info_str);
    void setCrashDumpPath(const std::string& path, bool origin_only);
    void setStackTraceOriginOnly(bool origin_only);

    /**
     * Set the flag regarding exiting on crash.
     * If flag is `true`, custom segfault handler will not invoke
     * original handler so that process will terminate without
     * generating core dump.
     * The flag is `false` by default.
     *
     * @param exit_on_crash New flag value.
     * @return void.
     */
    void setExitOnCrash(bool exit_on_crash);

    const std::string& getCriticalInfo() const;

    static std::mutex displayLock;

   private:
    // Copy is not allowed.
    SimpleLoggerMgr(const SimpleLoggerMgr&) = delete;
    SimpleLoggerMgr& operator=(const SimpleLoggerMgr&) = delete;

    static const size_t stackTraceBufferSize = 65536;

    // Singleton instance and lock.
    static std::atomic<SimpleLoggerMgr*> instance;
    static std::mutex instanceLock;

    SimpleLoggerMgr();
    ~SimpleLoggerMgr();

    void _flushStackTraceBuffer(size_t buffer_len, uint32_t tid_hash, uint64_t kernel_tid, bool crash_origin);
    void flushStackTraceBuffer(RawStackInfo& stack_info);
    void flushRawStack(RawStackInfo& stack_info);
    void addRawStackInfo(bool crash_origin = false);
    void logStackBackTraceOtherThreads();

    bool chkExitOnCrash();

    std::mutex loggersLock;
    std::unordered_set<SimpleLogger*> loggers;

    std::mutex activeThreadsLock;
    std::unordered_set<uint64_t> activeThreads;

    // Periodic log flushing thread.
    std::thread tFlush;

    // Old log file compression thread.
    std::thread tCompress;

    // List of files to be compressed.
    std::list<CompElem*> pendingCompElems;

    // Lock for `pendingCompFiles`.
    std::mutex pendingCompElemsLock;

    // Condition variable for BG flusher.
    std::condition_variable cvFlusher;
    std::mutex cvFlusherLock;

    // Condition variable for BG compressor.
    std::condition_variable cvCompressor;
    std::mutex cvCompressorLock;

    // Termination signal.
    std::atomic<bool> termination;

    // Original segfault handler.
    void (*oldSigSegvHandler)(int);

    // Original abort handler.
    void (*oldSigAbortHandler)(int);

    // Critical info that will be displayed on crash.
    std::string globalCriticalInfo;

    // Reserve some buffer for stack trace.
    char* stackTraceBuffer;

    // TID of thread where crash happens.
    std::atomic<uint64_t> crashOriginThread;

    std::string crashDumpPath;
    std::ofstream crashDumpFile;

    // If `true`, generate stack trace only for the origin thread.
    // Default: `true`.
    bool crashDumpOriginOnly;

    // If `true`, do not invoke original segfault handler
    // so that process just terminates.
    // Default: `false`.
    bool exitOnCrash;

    std::atomic<uint64_t> abortTimer;

    // Assume that only one thread is updating this.
    std::vector<RawStackInfo> crashDumpThreadStacks;
  };
}  // namespace app::consensus::_logger

namespace app::consensus::_logger {
  using namespace nuraft;

  /**
   * Example implementation of Raft logger, on top of SimpleLogger.
   */
  class logger_wrapper : public logger {
   public:
    logger_wrapper(const std::string& log_file, int log_level = 6) {
      my_log_ = new SimpleLogger(log_file, 1024, 32 * 1024 * 1024, 10);
      my_log_->setLogLevel(log_level);
      my_log_->setDispLevel(-1);
      my_log_->setCrashDumpPath("./", true);
      my_log_->start();
    }

    ~logger_wrapper() {
      destroy();
    }

    void destroy() {
      if (my_log_) {
        my_log_->flushAll();
        my_log_->stop();
        delete my_log_;
        my_log_ = nullptr;
      }
    }

    void put_details(int level, const char* source_file, const char* func_name, size_t line_number, const std::string& msg) {
      if (my_log_) {
        my_log_->put(level, source_file, func_name, line_number, "%s", msg.c_str());
      }
    }

    void set_level(int l) {
      if (!my_log_) return;

      if (l < 0) l = 1;
      if (l > 6) l = 6;
      my_log_->setLogLevel(l);
    }

    int get_level() {
      if (!my_log_) return 0;
      return my_log_->getLogLevel();
    }

    SimpleLogger* getLogger() const { return my_log_; }

   private:
    SimpleLogger* my_log_;
  };
}  // namespace app::consensus::_logger
