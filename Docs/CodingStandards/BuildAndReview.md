<!-- Copyright (c) 2026 Nelaric -->

English | [简体中文](BuildAndReview.zh-CN.md)

# Build Scripts, Tooling, and Review

## Copyright notices

Every project-authored file must carry a Nelaric copyright notice when its format permits one. In a text file that supports comments, the notice must be the first logical line, before a heading, code, configuration entry, or other prose. A UTF-8 BOM does not count as a line. Use `Copyright (c) <year> Nelaric`, where `<year>` is the file's first publication year; do not update it merely because the file changes.

Use the format's native comment syntax:

| File format | First line |
| --- | --- |
| C++, C#, and JavaScript source | `// Copyright (c) 2026 Nelaric` |
| Markdown and HTML | `<!-- Copyright (c) 2026 Nelaric -->` |
| Python, shell, YAML, and hash-comment configuration | `# Copyright (c) 2026 Nelaric` |

If an executable script needs a shebang, keep `#!...` on line one and put the copyright notice immediately on line two. If a format requires another first-line directive, put the notice at the first legal comment position. Do not add comments to strict JSON or another format that forbids them. Do not prepend a comment to `LICENSE` or alter its license text; its existing copyright line is authoritative. Binary assets, generated files, and unmodified third-party files retain their own applicable attribution and are exempt from an in-file Nelaric header. Record ownership for project-authored files that cannot carry a notice in the repository license or a nearby notice file rather than breaking the file format.

Apply this rule to every new project-authored file and to existing project-authored files when modifying them. Reviewers must check the header or documented format exception. Keep existing third-party notices intact; do not claim copyright over code or assets Nelaric does not own.

## Unreal build scripts

- Apply CSharpier to `.Build.cs` and `.Target.cs`. It is as mandatory as clang-format for C++.
- Keep module dependencies minimal and classify them as public or private according to the headers that need them. Do not put vendor SDK dependencies in Core.
- Target UE 5.6 and later. Engine compilation CI should cover both the minimum supported release and the latest supported release. CircleCI builds the Linux plugin with UE 5.6.1 before and after merge; the latest supported release remains a coverage gap. See `Docs/DevelopmentWorkflow.md`.

## Local checks

Use the versions fixed by `.github/workflows/quality.yml` and `.config/dotnet-tools.json`:

1. `python Scripts/check_text.py` checks text encoding and line endings.
2. `python Scripts/check_format.py` checks C++ and UE build-script formatting. Install the pinned clang-format and restore the local .NET tool manifest first.
3. `python Scripts/check_public_docs.py` checks that every public header has a file comment.
4. `python Scripts/run_doxygen.py` builds the API site and fails on Doxygen warnings. Install the pinned Doxygen release first.

The format, documentation, PR naming, and fork PR Linux build statuses are required pull request checks. They do not rewrite a PR. For a fork PR, the Linux build validates the exact PR commit in CircleCI before merge; a separate build runs after a push to `main`. The build bot rejects fork PR changes to CI configuration, automation scripts, Unreal build scripts, plugin descriptors, and related build controls. An in-repository PR receives a successful not-applicable fork-build status. No clang-tidy or test coverage gate is required now.

## Commit and branch names

Use only these prefixes for new commits and working branches:

| Prefix | Purpose |
| --- | --- |
| `feat` | Add a capability. |
| `fix` | Correct a defect. |
| `docs` | Change documentation only. |
| `style` | Change formatting without changing behavior. |
| `refactor` | Restructure code without changing behavior. |
| `perf` | Improve performance. |
| `test` | Add or change tests. |
| `build` | Change build configuration or dependencies. |
| `ci` | Change automation and CI workflows. |
| `chore` | Perform repository maintenance not covered above. |
| `revert` | Revert an earlier change. |

Commit subjects must use `<prefix>: <short English summary>` or `<prefix>(<scope>): <short English summary>`. Use a lowercase scope when present and start the summary with a verb. Squash-merge commit titles must follow the same rule. Examples: `feat(session): add reservation support` and `docs: clarify provider boundaries`.

PR titles must follow the same format as commit subjects. CI checks the PR title, source branch, and every commit subject in the PR. It checks the allowed prefix, structure, and an English summary beginning with a lowercase letter; reviewers confirm that the first word is a verb and that the summary describes the change.

Working branches must use `<prefix>/<lowercase-kebab-case-description>`, for example `fix/admission-timeout`. The prefix must come from the table above; `main` is the reserved default-branch exception. Do not introduce another prefix without first updating this standard.

## Text-file exceptions

Human-written Markdown and LICENSE, C++ source and headers, and UE C# build scripts use UTF-8 BOM and CRLF. `.gitattributes`, `.editorconfig`, `.clang-format`, `.csharpierrc`, `.gitignore`, `.json`, `.yml`, `.yaml`, `Doxyfile`, and executable `.py` scripts use UTF-8 without BOM and LF for tool compatibility. The checker enforces these categories.

## Review

Review correctness, copyright notices, module boundaries, public API documentation, cancellation and failure behavior, ownership, performance, and secret handling. A PR seeking an exception to a project guideline must name the rule, reason, affected code, and alternatives. A maintainer must approve the exception; it cannot override an Epic requirement or resolve a conflict between the standards. Report such conflicts in an issue. Current CI success is sufficient as an automated gate; tests may still be added for a concrete behavioral risk.
