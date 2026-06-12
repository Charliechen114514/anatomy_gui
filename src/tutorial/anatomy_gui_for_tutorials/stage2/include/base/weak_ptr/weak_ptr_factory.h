/**
 * @file    base/include/base/weak_ptr/weak_ptr_factory.h
 * @brief   Factory for producing WeakPtr instances for an exclusive owner.
 *
 * WeakPtrFactory should be declared as a member variable of the owner class,
 * as the last member to ensure it is destroyed first (C++ destroys members
 * in reverse declaration order).
 *
 * @author  N/A
 * @date    N/A
 * @version N/A
 * @since   N/A
 * @ingroup none
 */

#pragma once

#include "private/weak_ptr_internals.h"
#include "weak_ptr.h"

#include <cassert>
#include <memory>

namespace anatomy_gui::base {

/**
 * @brief  Factory for producing WeakPtr instances for an exclusive owner.
 *
 * WeakPtrFactory should be declared as a member variable of the owner class,
 * as the last member to ensure it is destroyed first (C++ destroys members
 * in reverse declaration order).
 *
 * @tparam T Type of the owner class.
 *
 * @ingroup none
 *
 * @note   Not copyable or movable. GetWeakPtr() should be called on the same
 *         thread where the WeakPtr is used.
 *
 * @warning Declare WeakPtrFactory as the LAST member of the owner class so it
 *          is destroyed first, invalidating all weak pointers before any other
 *          members are destroyed.
 *
 * @code
 * class Foo {
 * public:
 *     void doSomething() {}
 *
 * private:
 *     SomeResource   resource_;   // Destroyed first
 *     WeakPtrFactory<Foo> weak_factory_{this}; // Destroyed last
 * };
 *
 * Foo foo;
 * WeakPtr<Foo> ref = foo.weak_factory_.GetWeakPtr();
 * if (ref) {
 *     ref->doSomething();
 * }
 * @endcode
 */
template <typename T> class WeakPtrFactory {
  public:
    // ------------------------------------------------------------
    // Construction / Destruction
    // ------------------------------------------------------------

    /**
     * @brief  Constructs a WeakPtrFactory for the given owner.
     *
     * @param[in] owner Raw pointer to the owner object. Must not be nullptr.
     * @throws        None (assertion failure if owner is nullptr).
     * @note          The factory does not take ownership; the owner must
     *                outlive the factory or explicitly manage lifetime.
     * @warning       None
     * @since         N/A
     * @ingroup       none
     */
    explicit WeakPtrFactory(T* owner) noexcept
        : owner_(owner),
          flag_(std::make_shared<internal::WeakReferenceFlag>()) {
        assert(owner_ && "WeakPtrFactory owner must not be null");
    }

    /// @brief Destructor: automatically invalidates all issued WeakPtr
    /// instances.
    ~WeakPtrFactory() { InvalidateWeakPtrs(); }

    /// @brief Copy constructor: deleted.
    WeakPtrFactory(const WeakPtrFactory&) = delete;

    /// @brief Copy assignment: deleted.
    WeakPtrFactory& operator=(const WeakPtrFactory&) = delete;

    /// @brief Move constructor: deleted.
    WeakPtrFactory(WeakPtrFactory&&) = delete;

    /// @brief Move assignment: deleted.
    WeakPtrFactory& operator=(WeakPtrFactory&&) = delete;

    // ------------------------------------------------------------
    // Core Interface
    // ------------------------------------------------------------

    /**
     * @brief  Creates a WeakPtr pointing to the owner.
     *
     * @return WeakPtr<T> that can be used to safely access the owner.
     * @throws None (assertion failure if called after InvalidateWeakPtrs()).
     * @note   The returned WeakPtr does not extend the owner's lifetime.
     * @warning Do not call after InvalidateWeakPtrs() has been called.
     * @since  N/A
     * @ingroup none
     */
    [[nodiscard]] WeakPtr<T> GetWeakPtr() const noexcept {
        assert(flag_->IsAlive() &&
               "GetWeakPtr() called after InvalidateWeakPtrs()");
        return WeakPtr<T>(owner_, flag_);
    }

    /**
     * @brief  Invalidates all previously issued WeakPtr instances.
     *
     * After calling this method, all existing WeakPtr instances return
     * nullptr from Get(). The owner remains alive and new WeakPtr instances
     * can be created via GetWeakPtr().
     *
     * @throws None
     * @note   New WeakPtr instances can be created after invalidation.
     * @warning None
     * @since  N/A
     * @ingroup none
     */
    void InvalidateWeakPtrs() noexcept {
        if (flag_->IsAlive()) {
            flag_->Invalidate();
            // Allocate a new flag to support continued use after invalidation
            flag_ = std::make_shared<internal::WeakReferenceFlag>();
        }
    }

    /**
     * @brief  Checks if any WeakPtr instances are currently held externally.
     *
     * @return true if external WeakPtr instances exist, false otherwise.
     * @throws None
     * @note   Uses the shared_ptr use_count to determine if any WeakPtr
     *         instances are holding the flag.
     * @warning None
     * @since  N/A
     * @ingroup none
     */
    [[nodiscard]] bool HasWeakPtrs() const noexcept {
        return flag_.use_count() > 1;
    }

  private:
    T* owner_;
    // mutable allows GetWeakPtr() to be const
    mutable internal::WeakReferenceFlagPtr flag_;
};

} // namespace anatomy_gui::base
