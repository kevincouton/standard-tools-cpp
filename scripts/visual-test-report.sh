#!/usr/bin/env bash
# Generates a visual HTML report from CTest / test output.
set -euo pipefail

INPUT="${1:-test-output.log}"
REPORT="${2:-test-report.html}"

if [ ! -f "$INPUT" ]; then
  echo "ERROR: input file not found: $INPUT" >&2
  exit 1
fi

PASSED=$(grep -cE '^[[:space:]]*[0-9]+/[0-9]+ Test #[0-9]+: .*Passed' "$INPUT" 2>/dev/null || true)
FAILED=$(grep -cE '^[[:space:]]*[0-9]+/[0-9]+ Test #[0-9]+: .*Failed' "$INPUT" 2>/dev/null || true)
BUILD_ERRORS=$(grep -cE '^Error|CMake Error|make: \*\*\*|ninja: error' "$INPUT" 2>/dev/null || true)
RUNTIME_FAILS=$(grep -cE 'FAILED|assertion failed|runtime error' "$INPUT" 2>/dev/null || true)

if [ "$FAILED" -eq 0 ] && [ "$BUILD_ERRORS" -eq 0 ] && [ "$RUNTIME_FAILS" -eq 0 ]; then
  STATUS="PASS"
  COLOR="#22c55e"
else
  STATUS="FAIL"
  COLOR="#ef4444"
fi

cat > "$REPORT" <<REPORT_EOF
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Test Report</title>
  <style>
    body { font-family: system-ui, sans-serif; margin: 2rem; background: #0f172a; color: #e2e8f0; }
    .card { background: #1e293b; border-radius: 1rem; padding: 1.5rem; max-width: 600px; margin: 0 auto; box-shadow: 0 10px 30px rgba(0,0,0,0.3); }
    h1 { margin-top: 0; }
    .status { font-size: 3rem; font-weight: bold; color: $COLOR; }
    .stats { display: grid; grid-template-columns: repeat(2, 1fr); gap: 1rem; margin-top: 1rem; }
    .stat { background: #334155; padding: 1rem; border-radius: 0.5rem; text-align: center; }
    .stat .number { font-size: 2rem; font-weight: bold; }
    pre { background: #0f172a; padding: 1rem; border-radius: 0.5rem; overflow-x: auto; }
  </style>
</head>
<body>
  <div class="card">
    <h1>Standard-Tools C++ Test Report</h1>
    <div class="status">$STATUS</div>
    <div class="stats">
      <div class="stat"><div class="number">$PASSED</div><div>Tests Passed</div></div>
      <div class="stat"><div class="number">$FAILED</div><div>Tests Failed</div></div>
      <div class="stat"><div class="number">$BUILD_ERRORS</div><div>Build Errors</div></div>
      <div class="stat"><div class="number">$RUNTIME_FAILS</div><div>Runtime Failures</div></div>
    </div>
    <h2>Raw Output</h2>
    <pre>$(sed 's/&/\&amp;/g; s/</\&lt;/g; s/>/\&gt;/g; s/"/\&quot;/g; s/'"'"'/\&#39;/g; s/\$/\&#36;/g; s/`/\&#96;/g' "$INPUT")</pre>
  </div>
</body>
</html>
REPORT_EOF

echo "Report written to $REPORT"
