#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]
header_path = root / "src/state/ui_session_state.h"
text = header_path.read_text(encoding="utf-8")

include_anchor = "#include <cstdint>\n\nnamespace GroovePuterState {\n"
include_replacement = (
    "#include <cstdint>\n\n"
    "// Compatibility forward declaration for existing UI call sites.\n"
    "enum class WorkflowMode : uint8_t;\n\n"
    "namespace GroovePuterState {\n"
)
if text.count(include_anchor) != 1:
    raise RuntimeError("session header include anchor missing")
text = text.replace(include_anchor, include_replacement, 1)

overload_anchor = '''inline int rememberedWorkflowPage(const UiSessionState& state,
                                  SessionWorkflow workflow) {
    const int page = state.lastPageByWorkflow[workflowSessionIndex(workflow)];
    return pageBelongsToWorkflow(page, workflow)
        ? page
        : defaultPageForWorkflow(workflow);
}
'''
overload_replacement = overload_anchor + '''
inline int rememberedWorkflowPage(const UiSessionState& state,
                                  WorkflowMode workflow) {
    return rememberedWorkflowPage(
        state, static_cast<SessionWorkflow>(workflow));
}
'''
if text.count(overload_anchor) != 1:
    raise RuntimeError("remembered page overload anchor missing")
header_path.write_text(text.replace(overload_anchor, overload_replacement, 1),
                       encoding="utf-8")

test_path = root / "tests/test_ui_session_state.cpp"
test = test_path.read_text(encoding="utf-8")
test = test.replace("WorkflowMode::", "SessionWorkflow::")
test = test.replace("WorkflowPages::", "SessionPages::")
if "WorkflowMode::" in test or "WorkflowPages::" in test:
    raise RuntimeError("legacy UI types remain in pure session test")
test_path.write_text(test, encoding="utf-8")
