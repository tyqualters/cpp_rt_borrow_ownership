/**
 * @file lifetime.hpp
 * @author Ty Qualters (contact@tyqualters.com)
 * @brief Rust ownership & borrowing implemented as Runtime checks for C++
 * @version 0.1
 * @date 2022-12-17
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#pragma once

#ifndef CPP_LIFETIME_H_
#define CPP_LIFETIME_H_

#include <sstream>
#include <stdexcept>
// #include <source_location> (since C++20)
#include <set>
#include <utility>
#include <mutex>
#include <cassert>

// TODO: ... Waiting on further compiler support for C++20 & C++23
// auto inline get_source_position() -> std::string
// {
//     const std::source_location location = std::source_location::current();
//     std::stringstream ss;
//     ss << "File: \t" << location.filename() << "\n";
//     ss << "Line/Col: \t" << location.line() << ", " << location.column() << "\n";
//     ss << "Func: \t" << location.function_name() << "\n";
//     return ss.str();
// }

// Similar to a std::shared_ptr<T>

template<class T>
class Lifetime {
public:
    class LifetimeMutator;

    // Constructor (a new Lifetime)
    Lifetime(T* child, Lifetime** ownership, LifetimeMutator** mutator, std::mutex* mut, std::set<Lifetime*>* set, bool force_take_ownership = false, bool force_take_mutability = false) noexcept
    {
        this->m_T = child;
        this->m_mutator = (mutator == nullptr) ? new LifetimeMutator*{nullptr} : mutator;
        if(force_take_mutability && *this->m_mutator == nullptr)
            *this->m_mutator = new LifetimeMutator(this, new LifetimeMutator*{*this->m_mutator});
        this->m_owner = (ownership == nullptr ? new Lifetime*{this} : ownership);
        if(force_take_ownership && *this->m_owner != this) *this->m_owner = this;
        this->m_mutex = (mut == nullptr ? new std::mutex : mut);
        this->m_refs = (set == nullptr ? new std::set<Lifetime*> : set);
        this->m_refs->insert(this);
    }

    // Destructor (disable noexcept)
    ~Lifetime() noexcept(false)
    {
        assert(this->m_T != nullptr);
        assert(this->m_owner != nullptr);
        assert(this->m_refs != nullptr);
        assert(this->m_mutator != nullptr);

        // Remove mutability
        if(*this->m_mutator != nullptr && (*this->m_mutator)->m_mutator == this) delete *this->m_mutator;

        // Remove
        for(const auto& ptr : *this->m_refs)
        {
            if(ptr == this)
            {
                this->m_refs->erase(ptr);
                break;
            }
        }

        if(this == *this->m_owner)
        {
            if(this->m_refs->size() > 0U)
                throw std::runtime_error("Owner freed but references still exist.");
            this->m_refs->clear();
        }

        // Delete
        if(this->m_refs->empty())
        {
            delete this->m_T;
            delete this->m_owner;
            delete this->m_refs;
            std::cout << "Lifetime deleted." << std::endl;
        }

        // Reset
        this->m_T = nullptr;
        this->m_owner = nullptr;
        this->m_refs = nullptr;
    }

    // Disable copying
    Lifetime(Lifetime const&) = delete;
    void operator=(Lifetime const &x) = delete;

    // Create a new Lifetime
    auto static from(T&& value) noexcept -> Lifetime<T>
    {
        return Lifetime<T>(new T{value}, nullptr, nullptr, nullptr, nullptr);
    }

    // Get mutable
    auto get_mutable() -> T&
    {
        assert(this->m_T != nullptr);
        assert(this->m_owner != nullptr);
        assert(this->m_mutator != nullptr);
        assert(this->m_mutex != nullptr);

        if(*this->m_mutator != nullptr && (*this->m_mutator)->m_mutator == this);
        else if(this != *this->m_owner) throw std::runtime_error("Lifetime tried to get a mutable reference without maintaining object ownership or mutability.");
        
        std::scoped_lock<std::mutex> lock(*this->m_mutex);

        return *this->m_T;
    }

    // Set new value
    auto set(T&& value)
    {
        assert(this->m_T != nullptr);
        assert(this->m_owner != nullptr);
        assert(this->m_mutator != nullptr);
        assert(this->m_mutex != nullptr);

        if(*this->m_mutator != nullptr && (*this->m_mutator)->m_mutator == this);
        else if(this != *this->m_owner) throw std::runtime_error("Lifetime tried to write a new value without maintaining object ownership or mutability.");

        std::scoped_lock<std::mutex> lock(*this->m_mutex);

        *this->m_T = value;
    }

    // Get the value
    auto get() noexcept -> const T&
    {
        return *this->m_T;
    }

    // Borrow
    auto borrow() noexcept -> Lifetime<T>
    {
        return Lifetime<T>(this->m_T, this->m_owner, this->m_mutator, this->m_mutex, this->m_refs);
    }

    // Borrow mutable
    auto borrow_mutable() -> Lifetime<T>
    {
        assert(this->m_mutator != nullptr);

        if(*this->m_mutator != nullptr) throw std::runtime_error("Tried to borrow mutable access from a Lifetime for which mutable access already exists.");

        return Lifetime<T>(this->m_T, this->m_owner, this->m_mutator, this->m_mutex, this->m_refs, false, true);
    }

    // Clone
    auto clone() noexcept -> Lifetime<T>
    {
        T _T = *this->m_T;
        return Lifetime<T>::from(std::move(_T));
    }

    // Get mutability
    auto is_mutator() noexcept -> bool
    {
        return this->m_mutator != nullptr && (*this->m_mutator)->m_mutator == this;
    }

    // Get ownership
    auto is_owner() noexcept -> bool
    {
        return this == *this->m_owner;
    }

    // Move to
    auto move(Lifetime& lifetime) -> void
    {
        assert(this->m_owner != nullptr);
        assert(this->m_refs != nullptr);
        assert(this->m_mutator != nullptr);

        if(this != this->m_owner) throw std::runtime_error("Lifetime tried to transfer ownership without maintaining object ownership.");
        
        if(this == &lifetime) throw std::runtime_error("Lifetime tried to transfer ownership to the same instance.");

        if(this->m_refs.contains(&lifetime))
        {

            // Remove mutability
            if(*this->m_mutator != nullptr && *this->m_mutator->m_mutator == this) delete *this->m_mutator;

            // Transfer ownership
            this->m_owner = false;
            lifetime.m_owner = true;
        } else throw std::runtime_error("Lifetime tried to transfer ownership to a different Lifetime.");
    }

    // Move
    auto move() -> Lifetime<T>
    {
        assert(this->m_T != nullptr);
        assert(this->m_owner != nullptr);
        assert(this->m_refs != nullptr);
        assert(this->m_mutator != nullptr);

        if(this != *this->m_owner) throw std::runtime_error("Lifetime tried to transfer ownership without maintaining object ownership.");
        return Lifetime<T>(this->m_T, this->m_owner, this->m_mutator, this->m_mutex, this->m_refs, true, false);
    }

    class LifetimeMutator {
    public:
        friend class Lifetime;

        LifetimeMutator(Lifetime* mutator, LifetimeMutator** mutator_access)
        {
            this->m_mutator = mutator;
            this->m_mutatorAccess = mutator_access;
            *this->m_mutatorAccess = this;
        }

        ~LifetimeMutator()
        {
            this->m_mutator = nullptr;
            *this->m_mutatorAccess = nullptr;
        }
    
    protected:
        Lifetime* m_mutator = nullptr;
        LifetimeMutator** m_mutatorAccess = nullptr;
    };

    protected:
    mutable T* m_T = nullptr;
    mutable Lifetime** m_owner = nullptr;
    mutable std::mutex* m_mutex;
    mutable LifetimeMutator** m_mutator;
    mutable std::set<Lifetime*>* m_refs;

};

#endif
