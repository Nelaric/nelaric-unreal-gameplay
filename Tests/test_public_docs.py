# Copyright (c) 2026 Nelaric
"""Regression checks for documentation on wrapped exported declarations."""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "Scripts"))

from check_public_docs import undocumented_first_public_methods


class PublicDocsTests(unittest.TestCase):
    def missing(self, body):
        source = "class FExample\n{\npublic:\n" + body + "\n};\n"
        return [name for _, name in undocumented_first_public_methods(source)]

    def test_exported_wrapped_return_type_keeps_documentation(self):
        self.assertEqual(self.missing("""
    /** @brief Requests a transition. */
    EXAMPLE_API UE::Example::FHandle
    RequestTransition(int Mode);
"""), [])

    def test_export_macro_and_template_return_on_separate_lines(self):
        self.assertEqual(self.missing("""
    /// Reads the optional value.
    EXAMPLE_API
    TOptional<int32>
    GetValue() const;
"""), [])

    def test_reflected_wrapped_method_keeps_documentation(self):
        self.assertEqual(self.missing("""
    /// Reads the current value.
    UFUNCTION(BlueprintPure)
    EXAMPLE_API int32
    GetValue() const;
"""), [])

    def test_wrapped_method_without_documentation_is_rejected(self):
        self.assertEqual(self.missing("""
    EXAMPLE_API UE::Example::FHandle
    RequestTransition(int Mode);
"""), ["RequestTransition"])

    def test_previous_method_documentation_is_not_reused(self):
        self.assertEqual(self.missing("""
    /// Reads the old value.
    int32 GetOldValue() const;
    EXAMPLE_API int32
    GetValue() const;
"""), ["GetValue"])

    def test_documentation_before_access_section_is_not_reused(self):
        source = """class FExample
{
    /// Describes the section, not the method.
public:
    EXAMPLE_API int32
    GetValue() const;
};
"""
        self.assertEqual([name for _, name in undocumented_first_public_methods(source)], ["GetValue"])


if __name__ == "__main__":
    unittest.main()
