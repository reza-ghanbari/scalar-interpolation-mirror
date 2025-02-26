"""
Test that unused inlined parameters are displayed.
"""

import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test import lldbutil


class TestUnusedInlinedParameters(TestBase):

    def test_unused_inlined_parameters(self):
        self.build()
        lldbutil.run_to_source_breakpoint(self, "// break here", lldb.SBFileSpec("main.c"))

        # For the unused parameters, only check the types.
        self.assertIn("(void *) unused1",
                      lldbutil.get_description(self.frame().FindVariable("unused1")))
        self.assertEqual(42, self.frame().FindVariable("used").GetValueAsUnsigned())
