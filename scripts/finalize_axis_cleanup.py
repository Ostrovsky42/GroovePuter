#!/usr/bin/env python3
from pathlib import Path

workflow = Path('.github/workflows/four-axis-ui.yml')
text = workflow.read_text(encoding='utf-8')
start_marker = '  # BEGIN TEMP AXIS CLEANUP\n'
end_marker = '  # END TEMP AXIS CLEANUP\n'
start = text.find(start_marker)
end = text.find(end_marker)
if start < 0 or end < 0 or end < start:
    raise SystemExit('temporary cleanup job markers not found')
end += len(end_marker)
text = text[:start] + text[end:]
text = text.replace('permissions:\n  contents: write\n',
                    'permissions:\n  contents: read\n', 1)
workflow.write_text(text, encoding='utf-8')
