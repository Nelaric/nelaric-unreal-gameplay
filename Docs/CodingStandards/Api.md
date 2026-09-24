<!-- Copyright (c) 2026 Nelaric -->

English | [简体中文](Api.zh-CN.md)

# Public API, Errors, and Documentation

Design public contracts for C++ first. Expose selected operations to Blueprint when they are useful there, through a clear adapter or reflected entry point. There is no compatibility promise yet; API changes still need to be visible and explained in their PR.

## Contracts and ownership

- Use UE types in stable public interfaces. Provider contracts are pure C++ interfaces unless a separate Blueprint-facing adapter is needed.
- State who creates, owns, and destroys every service. Modules or Subsystems manage service lifetime. Use `TUniquePtr` or `TSharedPtr` for plain C++ ownership where appropriate; a raw pointer is non-owning. Follow UObject ownership and garbage-collection rules for reflected objects.
- Public methods must state the thread from which they may be called and the thread on which their callbacks run. The default for asynchronous public callbacks is the Game Thread.
- Do not capture an unprotected UObject raw pointer in work that may outlive it. Use an appropriate weak reference or another lifetime-safe mechanism.

## Asynchronous operations and errors

- An asynchronous public operation returns a cancellation handle and may accept separate success, cancellation, timeout, and failure callbacks. Callers need not supply every callback. Exactly one matching terminal callback runs for an operation; an omitted callback produces no call. There is no universal completion callback.
- Cancellation must propagate to downstream work where supported. The operation contract must explain what cancellation means if work has already finished or cannot be stopped.
- Use a unified result model containing a domain-specific error enum. Add a Gameplay Tag for more complex classifications and text detail only when needed. Do not substitute arbitrary strings for basic error codes.
- Use `check` and `ensure` for programmer mistakes or internal invariants. Authentication failures, invalid external configuration, network failures, and player input are ordinary result errors.

## Required API documentation

All public types, methods, enums and enum values, constants, Provider contracts, and Blueprint-exposed entry points must have English Doxygen comments. Explain purpose and usage; document parameters and return values, ownership and lifetime, thread expectations, and failure, timeout, and cancellation semantics when relevant. Each public header must have an `@file` or `\file` comment so file-level declarations are included in generated documentation.

The pull request check builds Doxygen HTML and fails on warnings or undocumented public declarations. Reviewers also check whether comments explain behavior accurately; a mechanically present comment is not enough. After merge, the approved documentation is published on GitHub Pages.
