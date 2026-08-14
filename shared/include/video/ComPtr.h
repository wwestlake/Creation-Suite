#pragma once

// Minimal, self-contained COM smart pointer shared by every shared/Video translation unit -
// deliberately not Microsoft::WRL::ComPtr (an extra SDK dependency this project doesn't
// otherwise take on) and not JUCE's internal ComSmartPtr (not part of JUCE's public API surface,
// and this header is JUCE-free by design - see this directory's own scope note).
namespace creation::video
{
template <typename T>
class ComPtr
{
public:
    ComPtr() = default;
    ComPtr(ComPtr&& other) noexcept : ptr(other.ptr) { other.ptr = nullptr; }
    ComPtr& operator=(ComPtr&& other) noexcept
    {
        if (this != &other)
        {
            reset();
            ptr = other.ptr;
            other.ptr = nullptr;
        }
        return *this;
    }
    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;
    ~ComPtr() { reset(); }

    void reset()
    {
        if (ptr != nullptr)
        {
            ptr->Release();
            ptr = nullptr;
        }
    }

    // Always resets first - matches the Windows convention that an out-param COM pointer must be
    // null before the call that's about to fill it in.
    T** address() noexcept { reset(); return &ptr; }
    T* get() const noexcept { return ptr; }
    T* operator->() const noexcept { return ptr; }
    explicit operator bool() const noexcept { return ptr != nullptr; }

private:
    T* ptr = nullptr;
};
}
