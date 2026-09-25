<!-- Copyright (c) 2026 Nelaric -->

English | [简体中文](API.zh-CN.md)

# Public API, Errors, and Documentation

Design public contracts for C++ first. Expose selected operations to Blueprint when they are useful there, through a clear adapter or reflected entry point. There is no compatibility promise yet; API changes still need to be visible and explained in their PR.

## Contracts and ownership

- Use UE types in stable public interfaces. Design non-reflected gameplay extension contracts as C++ interfaces; expose Blueprint-facing adapters when authors need them.
- State who creates, owns, and destroys every service. Modules or Subsystems manage service lifetime. Use `TUniquePtr` or `TSharedPtr` for plain C++ ownership where appropriate; a raw pointer is non-owning. Follow UObject ownership and garbage-collection rules for reflected objects.
- Public methods must state the thread from which they may be called and the thread on which their callbacks run. The default for asynchronous public callbacks is the Game Thread.
- Do not capture an unprotected UObject raw pointer in work that may outlive it. Use an appropriate weak reference or another lifetime-safe mechanism.

## Asynchronous operations and errors

- An asynchronous public operation returns a cancellation handle and may accept separate success, cancellation, timeout, and failure callbacks. Callers need not supply every callback. Exactly one matching terminal callback runs for an operation; an omitted callback produces no call. There is no universal completion callback.
- Cancellation must propagate to downstream work where supported. The operation contract must explain what cancellation means if work has already finished or cannot be stopped.
- Use a unified result model containing a domain-specific error enum. Add a Gameplay Tag for more complex classifications and text detail only when needed. Do not substitute arbitrary strings for basic error codes.
- Use `check` and `ensure` for programmer mistakes or internal invariants. Authentication failures, invalid external configuration, network failures, and player input are ordinary result errors.

## Required API documentation

In a class that exposes gameplay operations, place the gameplay-facing methods in its first `public:` section, directly after the class opening and Unreal reflection macro when present. Place methods called only by framework integration or Unreal lifecycle code in a second `public:` section after the gameplay API. No other access specifier may appear between these two `public:` sections. The second section is a C++ access boundary, not a gameplay API; its methods do not require Doxygen comments. Keep methods that need no external access private or protected. A method's position must reflect its intended caller, not merely its current call sites.

All public types, gameplay-facing methods in the first `public:` section, enums and enum values, constants, gameplay extension contracts, and Blueprint-exposed entry points must have English Doxygen comments. Explain purpose and usage; document parameters and return values, ownership and lifetime, thread expectations, and failure, timeout, and cancellation semantics when relevant. Each public header must have an `@file` comment so file-level declarations are included in generated documentation. The second `public:` section may use ordinary implementation comments when helpful.

Follow the [Doxygen comment standard](Doxygen.md) for declaration coverage, format, content, and comment-line length.

Reviewers check that methods in the first `public:` section have useful Doxygen comments and that no other access specifier separates the two sections. The pull request check builds Doxygen HTML and fails on documentation warnings. Tooling must be aligned with this distinction before framework-only methods are left undocumented in a submitted change. After merge, the approved documentation is published on GitHub Pages.
