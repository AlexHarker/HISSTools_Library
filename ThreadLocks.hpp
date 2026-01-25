
/**
 * @file ThreadLocks.hpp
 * @brief Provides thread synchronization mechanisms for multi-threaded applications.
 *
 * This file contains classes and functions that help manage thread synchronization, ensuring
 * safe access to shared resources in a concurrent environment. It includes the `thread_lock` class
 * for managing thread locks and the `lock_hold` template class, which follows the RAII pattern
 * to automatically acquire and release locks. Platform-specific implementations may be included
 * to support various operating systems.
 *
 * The key components of this file include:
 * - `thread_lock`: A class providing manual lock management for synchronizing threads.
 * - `lock_hold`: A template class that ensures automatic lock management using RAII,
 *   acquiring the lock on construction and releasing it on destruction.
 * - Platform-specific utilities and OS abstractions for fine-grained control of threads.
 *
 * @note This file is crucial for ensuring thread safety in multi-threaded applications, preventing
 *       data races and ensuring consistency when multiple threads access shared resources.
 */

#ifndef THREADLOCKS_HPP
#define THREADLOCKS_HPP

#include <atomic>
#include <algorithm>
#include <chrono>
#include <thread>

#ifdef __linux__

// Linux specific definitions

/**
 * @namespace OS_Specific
 * @brief Contains functions and utilities that are specific to certain operating systems.
 *
 * This namespace provides a layer of abstraction for platform-dependent operations, ensuring
 * that the underlying OS differences are handled seamlessly. Functions within this namespace
 * are implemented differently based on the target operating system, such as thread management,
 * sleeping behavior, or synchronization primitives.
 *
 * @note When working in a cross-platform environment, use the utilities within this namespace
 *       to ensure compatibility across various operating systems.
 */

namespace OS_Specific
{
    
    /**
     * @brief Causes the current thread to sleep for a very short time (nano-level precision).
     *
     * This function introduces a small delay, useful for yielding execution in busy-waiting loops
     * or for fine-grained timing control in multi-threaded environments. The actual sleep time
     * may vary depending on the platform's thread scheduling and sleep resolution.
     *
     * @note The precision and behavior of this function may depend on the operating system
     *       and hardware capabilities.
     */

    inline void thread_nano_sleep()
    {
        std::this_thread::sleep_for(std::chrono::nanoseconds(100));
    }
}

/**
 * @brief Conditional compilation for Apple operating systems.
 *
 * This section of the code is compiled only when targeting an Apple platform, such as macOS or iOS.
 * The `__APPLE__` macro is defined by the compiler to indicate that the code is being compiled
 * on an Apple-based operating system. It is used to include platform-specific code or
 * configurations that are unique to macOS or iOS environments.
 *
 * @note Ensure that platform-specific code within this block adheres to the requirements
 *       of Apple systems.
 */

#elif defined(__APPLE__)

// OSX specific definitions

/**
 * @namespace OS_Specific
 * @brief Contains platform-specific implementations and utilities.
 *
 * The OS_Specific namespace groups together functions and types that provide operating system-specific
 * behavior. This allows for tailored handling of different OS-level features such as threading,
 * synchronization, and timing, depending on the platform. Code within this namespace abstracts
 * away the differences between various operating systems, ensuring that the appropriate
 * implementation is used based on the target environment.
 *
 * @note Use the utilities in this namespace for code that requires adaptation based on
 *       the underlying operating system.
 */

namespace OS_Specific
{

    /**
     * @brief Puts the calling thread to sleep for a brief period with nanosecond precision.
     *
     * This function introduces a very short sleep interval for the current thread, which can be
     * useful in scenarios requiring fine-grained timing or when a thread needs to yield execution
     * for a minimal amount of time. The exact duration and behavior of this sleep may vary
     * depending on the operating system and hardware capabilities.
     *
     * @note The actual sleep duration might not be exactly as requested, due to limitations in
     *       system sleep resolution or other system factors.
     */

    inline void thread_nano_sleep()
    {
        std::this_thread::sleep_for(std::chrono::nanoseconds(100));
    }
}

#else

// Windows OS specific definitions

/**
 * @brief Includes the Windows-specific API header file.
 *
 * This directive imports the Windows API, providing access to various system-level
 * functionalities specific to the Windows operating system. It includes functions for
 * managing processes, threads, file I/O, and other system services that are unique
 * to the Windows environment.
 *
 * @note This header should only be included when writing code specifically targeting
 *       the Windows platform. Code that depends on this header will not compile
 *       on non-Windows systems.
 */

#include <windows.h>

/**
 * @namespace OS_Specific
 * @brief Provides platform-specific implementations for various system-level operations.
 *
 * The OS_Specific namespace contains functions and utilities designed to handle
 * platform-dependent operations such as threading, synchronization, and timing.
 * These implementations ensure that the code can adapt to the peculiarities of different
 * operating systems, like Windows, macOS, and Linux, providing a layer of abstraction
 * for cross-platform compatibility.
 *
 * @note The functions within this namespace should be used when platform-specific
 *       behavior is required, ensuring proper operation across different environments.
 */

namespace OS_Specific
{

    /**
     * @brief Suspends the execution of the calling thread for a very short duration.
     *
     * This function causes the calling thread to sleep for a brief time, typically with
     * nanosecond-level precision. It is useful for implementing fine-grained delays or yielding
     * processor time in busy-wait loops or performance-critical applications.
     *
     * @note The exact duration of the sleep may vary depending on the operating system and
     *       hardware timer resolution. This function is typically used when high-precision
     *       timing or minimal delays are required.
     */

    inline void thread_nano_sleep()
    {
        SwitchToThread();
    }
}

#endif

/**
 * @class thread_lock
 * @brief A class that provides synchronization mechanisms for thread-safe operations.
 *
 * The `thread_lock` class is designed to manage access to shared resources in a
 * multi-threaded environment. It ensures that only one thread can hold the lock
 * at any given time, preventing data races and ensuring safe manipulation of shared data.
 *
 * This class typically provides mechanisms such as acquiring, releasing, and
 * optionally trying to acquire a lock, depending on its implementation.
 *
 * @note Use this class to ensure safe access to shared resources in concurrent programming.
 */

class thread_lock
{
    
    /**
     * @brief Alias for the steady clock type from the C++ Standard Library.
     *
     * Defines `Clock` as an alias for `std::chrono::steady_clock`, which is a monotonic clock
     * provided by the C++ Standard Library. A steady clock guarantees that time progresses
     * consistently without jumping backward or being adjusted by the system clock, making it
     * ideal for measuring intervals and durations accurately.
     *
     * @note Use `Clock` for timing operations where a reliable and non-adjustable time source is required,
     *       such as performance measurements or timeout calculations.
     */
    
    using Clock = std::chrono::steady_clock;
    
public:
    
    /**
     * @brief Default constructor for the thread_lock class.
     *
     * Initializes a new instance of the `thread_lock` class. This constructor sets up the
     * necessary internal state for managing the lock but does not acquire the lock itself.
     * The lock can be acquired later by calling the appropriate methods, depending on the
     * class's functionality.
     *
     * @note This constructor does not perform any locking operations; it only prepares the object
     *       for future use in synchronization.
     */
    
    thread_lock() {}
    
    /**
     * @brief Destructor for the thread_lock class that ensures the lock is acquired.
     *
     * The destructor attempts to acquire the lock upon object destruction. This ensures
     * that any cleanup or final operations that need to be synchronized are handled safely
     * before the object is destroyed.
     *
     * @note Since the destructor acquires the lock, care should be taken to avoid deadlocks,
     *       particularly if this behavior is not expected in the context of the program.
     */
    
    ~thread_lock() { acquire(); }
    
    // Non-copyable
    
    /**
     * @brief Deleted copy constructor to prevent copying of thread_lock objects.
     *
     * The copy constructor is explicitly deleted to prevent the copying of `thread_lock` instances.
     * This ensures that thread locks cannot be unintentionally copied, which could lead to
     * issues such as multiple threads trying to acquire the same lock or inconsistencies in
     * synchronization control.
     *
     * @note This design choice enforces the rule that `thread_lock` instances must be uniquely
     *       owned and cannot be shared or duplicated through copying.
     */
    
    thread_lock(const thread_lock&) = delete;
    
    /**
     * @brief Deleted copy assignment operator to prevent assignment of thread_lock objects.
     *
     * The copy assignment operator is explicitly deleted to prevent assigning one `thread_lock`
     * object to another. This ensures that thread locks cannot be inadvertently reassigned,
     * which could cause synchronization issues or lead to multiple threads attempting to
     * manage the same lock object.
     *
     * @note By deleting this operator, `thread_lock` objects are enforced to have unique ownership,
     *       preventing errors related to shared or duplicated locks.
     */
    
    thread_lock& operator=(const thread_lock&) = delete;
    
    /**
     * @brief Acquires the lock to ensure exclusive access to shared resources.
     *
     * This method is used to acquire the thread lock, blocking the calling thread if necessary
     * until the lock becomes available. Once the lock is acquired, the calling thread has
     * exclusive access to the shared resource protected by the lock.
     *
     * @note This function may cause the calling thread to block if another thread is already holding the lock.
     *       Make sure to release the lock appropriately to avoid deadlocks or resource contention.
     */
    
    void acquire()
    {
        for (int i = 0; i < 10; i++)
            if (attempt())
                return;
        
        auto timeOut = Clock::now() + std::chrono::nanoseconds(10000);
        
        while (Clock::now() < timeOut)
            if (attempt())
                return;
        
        while (!attempt())
            OS_Specific::thread_nano_sleep();
    }
    
    /**
     * @brief Attempts to acquire the lock without blocking.
     *
     * This method tries to acquire the lock by checking if it is currently available. If the lock
     * is available, it will be acquired, and the function will return `true`. If the lock is
     * already held by another thread, it returns `false`, allowing the calling thread to
     * continue executing without blocking.
     *
     * @return `true` if the lock was successfully acquired, `false` if the lock is already held.
     *
     * @note This non-blocking attempt can be useful in scenarios where a thread should not be
     *       delayed waiting for a lock, such as when implementing a spinlock or trying to
     *       minimize lock contention.
     */
    
    bool attempt() { return !m_atomic_lock.test_and_set(); }
    
    /**
     * @brief Releases the lock, allowing other threads to acquire it.
     *
     * This method clears the lock, signaling that it is now available for other threads
     * to acquire. After calling `release()`, the calling thread no longer holds the lock,
     * and other threads waiting to acquire the lock can proceed.
     *
     * @note It is important to call this function after acquiring the lock to prevent deadlocks
     *       or prolonged resource contention. Failing to release the lock can cause other threads
     *       to block indefinitely.
     */
    
    void release() { m_atomic_lock.clear(); }
    
private:
    
    /**
     * @brief Atomic flag used to implement the lock mechanism.
     *
     * The `m_atomic_lock` is an atomic flag that is used to control access to the shared resource.
     * It provides a low-level atomic lock that ensures thread-safe operations. The flag is
     * initialized to the `ATOMIC_FLAG_INIT` value, meaning it starts in the cleared state,
     * indicating that the lock is available.
     *
     * The atomic flag allows for atomic operations such as `test_and_set()` and `clear()` to
     * acquire and release the lock without the risk of race conditions.
     *
     * @note Atomic flags are ideal for lightweight locking mechanisms like spinlocks where
     *       performance is critical, as they avoid the overhead of more complex locking constructs.
     */
    
    std::atomic_flag m_atomic_lock = ATOMIC_FLAG_INIT;
};


// A generic lock holder using RAII

/**
 * @brief A RAII (Resource Acquisition Is Initialization) wrapper for managing locks.
 *
 * The `lock_hold` template class provides a mechanism for automatic lock management.
 * It ensures that a lock is acquired when an instance of this class is created and
 * automatically released when the instance goes out of scope. This pattern helps to
 * prevent resource leaks and ensures that locks are properly released, even in the
 * case of exceptions.
 *
 * @tparam T The type of the lock object, which must implement the specified acquire and release methods.
 * @tparam acquire_method A pointer to the member function of the lock object that acquires the lock.
 * @tparam release_method A pointer to the member function of the lock object that releases the lock.
 *
 * @note This class follows the RAII idiom, making it easier to manage locks by tying
 *       the lifecycle of the lock to the scope of the `lock_hold` object. This helps to
 *       avoid common issues such as forgetting to release locks.
 */

template <class T, void (T::*acquire_method)(), void (T::*release_method)()>
class lock_hold
{
public:
    
    /**
     * @brief Default constructor for the lock_hold class.
     *
     * Initializes a `lock_hold` object without associating it with any lock. This constructor
     * sets the internal lock pointer to `nullptr`, meaning that no lock is acquired.
     * It can be useful when the lock is assigned later or when the `lock_hold` object is
     * temporarily created without immediately acquiring a lock.
     *
     * @note After using this constructor, the lock must be manually associated and acquired
     *       to ensure proper functionality. This constructor does not perform any locking operations.
     */
    
    lock_hold() : m_lock(nullptr) {}
    
    /**
     * @brief Constructs a lock_hold object and acquires the specified lock.
     *
     * This constructor initializes the `lock_hold` object with a pointer to a `thread_lock` object.
     * If a valid lock is provided, the specified `acquire_method` is automatically called to acquire the lock.
     * This ensures that the lock is acquired immediately upon construction, following the RAII pattern.
     *
     * @param lock A pointer to the `thread_lock` object to manage. If the pointer is valid, the lock
     *             is acquired using the provided acquire method.
     *
     * @note This constructor ensures that the lock is held during the lifetime of the `lock_hold`
     *       object, and will be automatically released when the `lock_hold` object is destroyed.
     */
    
    lock_hold(thread_lock *lock) : m_lock(lock) { if (m_lock) m_lock->*acquire_method(); }
    
    /**
     * @brief Destructor for the lock_hold class that automatically releases the lock.
     *
     * This destructor ensures that the lock managed by the `lock_hold` object is released
     * when the object goes out of scope. If a valid lock is being managed, the `release()`
     * method is called to release the lock, ensuring proper resource cleanup and preventing
     * deadlocks.
     *
     * @note This automatic lock release follows the RAII pattern, ensuring that the lock
     *       is safely and consistently released even in the presence of exceptions or early returns.
     */
    
    ~lock_hold() { if (m_lock) m_lock->release(); }
    
    // Non-copyable
    
    /**
     * @brief Deleted copy constructor to prevent copying of lock_hold objects.
     *
     * The copy constructor is explicitly deleted to prevent copying of `lock_hold` instances.
     * This ensures that the ownership and management of the lock cannot be unintentionally duplicated,
     * which could lead to improper lock handling or multiple objects trying to release the same lock.
     *
     * @note By deleting this constructor, each `lock_hold` object has unique ownership of its lock,
     *       preventing issues related to double acquisition or release of the same lock.
     */
    
    lock_hold(const lock_hold&) = delete;
    
    /**
     * @brief Deleted copy assignment operator to prevent assignment of lock_hold objects.
     *
     * The copy assignment operator is explicitly deleted to prevent assigning one `lock_hold`
     * object to another. This ensures that the lock managed by one `lock_hold` instance cannot
     * be inadvertently transferred or duplicated, which could lead to improper lock handling
     * and resource management issues.
     *
     * @note Deleting the copy assignment operator enforces unique ownership of the lock,
     *       ensuring that each `lock_hold` object exclusively manages its lock without
     *       the risk of accidental reassignment.
     */
    
    lock_hold& operator=(const lock_hold&) = delete;
    
    /**
     * @brief Releases the held lock, allowing other threads to acquire it.
     *
     * This method is responsible for releasing the lock that was previously acquired.
     * Once the lock is released, other threads waiting for the lock will be able to acquire it.
     * This function should be called when the thread no longer needs exclusive access to the
     * shared resource protected by the lock.
     *
     * @note Ensure that this method is called after the lock has been acquired to prevent
     *       deadlocks or resource contention. Failing to release the lock properly may
     *       cause other threads to block indefinitely.
     */
    
    void release()
    {
        if (m_lock)
        {
            (m_lock->*release_method)();
            m_lock = nullptr;
        }
    }
    
private:
    
    /**
     * @brief Pointer to the thread lock being managed by the lock_hold object.
     *
     * This member variable holds a pointer to the `thread_lock` object that is managed by the
     * `lock_hold` instance. It is used to acquire and release the lock, ensuring that the
     * lock is properly managed according to the RAII pattern.
     *
     * @note The pointer can be `nullptr` if no lock is being managed. Always check whether
     *       the pointer is valid before attempting to acquire or release the lock.
     */
    
    thread_lock *m_lock;
};

#endif /* THREADLOCKS_HPP */
