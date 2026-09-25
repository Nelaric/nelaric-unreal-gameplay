# Copyright (c) 2026 Nelaric
"""Exercise the Doxygen style gate at its length and syntax boundaries."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "Scripts"))

from check_doxygen_style import check_source
from check_public_docs import has_file_comment


class DoxygenStyleTests(unittest.TestCase):
    def test_75_character_comment_passes(self):
        sentence = "The world keeps the settings chosen by its level until the map is unloaded."
        self.assertEqual(len(sentence), 75)
        self.assertEqual(check_source(f"/// {sentence}\n", "sample.h"), [])

    def test_80_character_sentence_must_wrap(self):
        sentence = "The map retains the world settings chosen by its level until this world unloads."
        self.assertEqual(len(sentence), 80)
        issues = check_source(f"/// {sentence}\n", "sample.h")
        self.assertTrue(any("1 to 75" in issue for issue in issues))
        wrapped = (
            "/** @brief The map retains the world settings chosen by its level\n"
            " * until this world unloads.\n"
            " */\n"
        )
        self.assertEqual(check_source(wrapped, "sample.h"), [])

    def test_multiline_requires_tag(self):
        issues = check_source("/** Explains the result.\n * More detail.\n */\n", "sample.h")
        self.assertTrue(any("requires a Doxygen @tag" in issue for issue in issues))
        untagged_second_paragraph = (
            "/** @brief Explains the result.\n *\n"
            " * This paragraph has no tag.\n */\n"
        )
        issues = check_source(untagged_second_paragraph, "sample.h")
        self.assertTrue(any("start each paragraph" in issue for issue in issues))
        tagged = untagged_second_paragraph.replace(
            " * This paragraph has no tag.", " * @details This paragraph has a tag."
        )
        self.assertEqual(check_source(tagged, "sample.h"), [])
        issues = check_source("/// First line.\n/// Second line.\n", "sample.h")
        self.assertTrue(any("tagged /**" in issue for issue in issues))
        issues = check_source("/** @brief Short description. */\n", "sample.h")
        self.assertTrue(any("use ///" in issue for issue in issues))

    def test_tags_and_continuation_lines(self):
        source = "/** @brief Explains the result.\n * @param Count Number of players.\n */\n"
        self.assertEqual(check_source(source, "sample.h"), [])
        issues = check_source("/** Explains the result. @param Count Players. */\n", "sample.h")
        self.assertTrue(any("own line" in issue for issue in issues))
        issues = check_source("/** \\param Count Players. */\n", "sample.h")
        self.assertTrue(any("use @" in issue for issue in issues))

    def test_trailing_variable_comment_boundary(self):
        self.assertEqual(check_source("int Count; ///< Maximum admitted players.\n", "sample.h"), [])
        issues = check_source("int Count; ///< Maximum admitted player count.\n", "sample.h")
        self.assertTrue(any("1 to 25" in issue for issue in issues))
        issues = check_source("///< Description.\nint Count;\n", "sample.h")
        self.assertTrue(any("after a variable" in issue for issue in issues))

    def test_enum_trailing_comments_align_at_the_first_slash(self):
        aligned = (
            "enum class EResult\n{\n"
            "    Success,          ///< Task completed.\n"
            "    RetryableFailure, ///< Retry later.\n"
            "};\n"
        )
        self.assertEqual(check_source(aligned, "sample.h"), [])
        misaligned = aligned.replace("RetryableFailure, ///<", "RetryableFailure,  ///<")
        issues = check_source(misaligned, "sample.h")
        self.assertTrue(any("align the first /" in issue for issue in issues))

    def test_enum_uses_leading_comments_for_all_values_when_one_is_long(self):
        mixed = (
            "enum class EResult\n{\n"
            "    Success, ///< Task completed.\n"
            "    /// Task failed but can be retried after a short delay.\n"
            "    RetryableFailure,\n"
            "};\n"
        )
        issues = check_source(mixed, "sample.h")
        self.assertTrue(any("before every enum value" in issue for issue in issues))
        leading = mixed.replace("    Success, ///< Task completed.\n", "    /// Task completed.\n    Success,\n")
        self.assertEqual(check_source(leading, "sample.h"), [])
        long_trailing = mixed.replace("Task completed.", "Task failed but may be retried.")
        issues = check_source(long_trailing, "sample.h")
        self.assertTrue(any("1 to 25" in issue for issue in issues))

    def test_other_doxygen_markers_fail(self):
        issues = check_source("//! Description.\n", "sample.h")
        self.assertTrue(any("use ///" in issue for issue in issues))

    def test_inline_comment_and_unbroken_long_content_fail(self):
        issues = check_source("int Count; /** Description. */\n", "sample.h")
        self.assertTrue(any("before declarations" in issue for issue in issues))
        issues = check_source(f"/** @brief {'x' * 76} */\n", "sample.h")
        self.assertTrue(any("maximum 75" in issue for issue in issues))

    def test_public_file_tag_must_be_in_first_doxygen_comment(self):
        self.assertTrue(has_file_comment("// Copyright\n\n/** @file Sample.h */\n#pragma once\n"))
        self.assertFalse(has_file_comment("// @file Sample.h\n#pragma once\n"))
        self.assertFalse(has_file_comment("#pragma once\n/** @file Sample.h */\n"))


if __name__ == "__main__":
    unittest.main()
