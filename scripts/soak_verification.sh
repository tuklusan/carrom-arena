#!/bin/bash
# =============================================================================
# Carrom Arena - Soak Verification Script
# Runs 10 seeds × 3 matches headless to verify regression fix holds across variance
# =============================================================================

set -euo pipefail

# ---- Configuration ----
SEEDS=(12345 42 999 123456 7890 555 777 888 9999 4242)
MATCHES_PER_SEED=3
MAX_TOTAL_STEPS=600000
MAX_STEPS_PER_SHOT=1200
TIMEOUT_SECONDS=1800  # 30 minutes total

# ---- Colors for output ----
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# ---- Find test executable ----
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Check build directory
if [[ -f "$PROJECT_ROOT/build/tests/test_regression" ]]; then
    TEST_EXE="$PROJECT_ROOT/build/tests/test_regression"
elif [[ -f "$PROJECT_ROOT/build_asan/tests/test_regression" ]]; then
    TEST_EXE="$PROJECT_ROOT/build_asan/tests/test_regression"
else
    echo -e "${RED}ERROR: test_regression executable not found. Build first with:${NC}"
    echo "  cmake -B build -S . && cmake --build build --target test_regression"
    exit 1
fi

echo -e "${BLUE}=== Carrom Arena Soak Verification ===${NC}"
echo "Test executable: $TEST_EXE"
echo "Seeds: ${SEEDS[*]}"
echo "Matches per seed: $MATCHES_PER_SEED"
echo "Max total steps: $MAX_TOTAL_STEPS"
echo "Max steps per shot: $MAX_STEPS_PER_SHOT"
echo "Timeout: ${TIMEOUT_SECONDS}s"
echo ""

# ---- Results tracking ----
TOTAL_RUNS=0
PASSED=0
FAILED=0
FAILURE_DETAILS=()

# Output JSON summary
RESULTS_FILE="$PROJECT_ROOT/soak_results_$(date +%Y%m%d_%H%M%S).json"
echo '{"soak_verification": {' > "$RESULTS_FILE"
echo '  "timestamp": "'$(date -Iseconds)'",' >> "$RESULTS_FILE"
echo '  "config": {' >> "$RESULTS_FILE"
echo '    "seeds": ['$(printf '"%s",' "${SEEDS[@]}" | sed 's/,$//')'],' >> "$RESULTS_FILE"
echo '    "matches_per_seed": '$MATCHES_PER_SEED',' >> "$RESULTS_FILE"
echo '    "max_total_steps": '$MAX_TOTAL_STEPS',' >> "$RESULTS_FILE"
echo '    "max_steps_per_shot": '$MAX_STEPS_PER_SHOT',' >> "$RESULTS_FILE"
echo '  },' >> "$RESULTS_FILE"
echo '  "results": [' >> "$RESULTS_FILE"

FIRST_RESULT=true

# ---- Run soak ----
START_TIME=$(date +%s)

for seed_idx in "${!SEEDS[@]}"; do
    SEED=${SEEDS[$seed_idx]}
    
    echo -e "${BLUE}[Seed $((seed_idx+1))/10] Seed: $SEED${NC}"
    
    for match_idx in $(seq 1 $MATCHES_PER_SEED); do
        TOTAL_RUNS=$((TOTAL_RUNS + 1))
        RUN_LABEL="seed=$SEED match=$match_idx"
        
        echo -n "  Match $match_idx/$MATCHES_PER_SEED... "
        
        RUN_START=$(date +%s)
        
        # Run single match with timeout
        if timeout "$TIMEOUT_SECONDS" "$TEST_EXE" \
            --seed="$SEED" \
            --max-steps="$MAX_TOTAL_STEPS" \
            --max-shot-steps="$MAX_STEPS_PER_SHOT" \
            > /dev/null 2>&1; then
            
            RUN_END=$(date +%s)
            DURATION=$((RUN_END - RUN_START))
            
            echo -e "${GREEN}PASS${NC} (${DURATION}s)"
            PASSED=$((PASSED + 1))
            
            # Append to JSON
            if [[ "$FIRST_RESULT" == "true" ]]; then
                FIRST_RESULT=false
            else
                echo ',' >> "$RESULTS_FILE"
            fi
            echo '    {"seed": '$SEED', "match": '$match_idx', "status": "pass", "duration_sec": '$DURATION'}' >> "$RESULTS_FILE"
            
        else
            RUN_END=$(date +%s)
            DURATION=$((RUN_END - RUN_START))
            EXIT_CODE=$?
            
            if [[ $EXIT_CODE -eq 124 ]]; then
                echo -e "${RED}TIMEOUT${NC} (${DURATION}s)"
                FAILURE_DETAILS+=("$RUN_LABEL: TIMEOUT after ${DURATION}s")
            else
                echo -e "${RED}FAIL${NC} (${DURATION}s, exit=$EXIT_CODE)"
                FAILURE_DETAILS+=("$RUN_LABEL: FAIL exit=$EXIT_CODE after ${DURATION}s")
            fi
            
            FAILED=$((FAILED + 1))
            
            # Append to JSON
            if [[ "$FIRST_RESULT" == "true" ]]; then
                FIRST_RESULT=false
            else
                echo ',' >> "$RESULTS_FILE"
            fi
            echo '    {"seed": '$SEED', "match": '$match_idx', "status": "fail", "duration_sec": '$DURATION', "exit_code": '$EXIT_CODE'}' >> "$RESULTS_FILE"
        fi
        
        # Check overall timeout
        ELAPSED=$(($(date +%s) - START_TIME))
        if [[ $ELAPSED -gt $TIMEOUT_SECONDS ]]; then
            echo -e "${RED}Overall timeout reached (${ELAPSED}s > ${TIMEOUT_SECONDS}s)${NC}"
            break 2
        fi
    done
done

END_TIME=$(date +%s)
TOTAL_DURATION=$((END_TIME - START_TIME))

# ---- Finalize JSON ----
echo '' >> "$RESULTS_FILE"
echo '  ],' >> "$RESULTS_FILE"
echo '  "summary": {' >> "$RESULTS_FILE"
echo '    "total_runs": '$TOTAL_RUNS',' >> "$RESULTS_FILE"
echo '    "passed": '$PASSED',' >> "$RESULTS_FILE"
echo '    "failed": '$FAILED',' >> "$RESULTS_FILE"
echo '    "duration_sec": '$TOTAL_DURATION',' >> "$RESULTS_FILE"
echo '    "success_rate": '$(awk "BEGIN {printf \"%.2f\", $PASSED*100/$TOTAL_RUNS}")',' >> "$RESULTS_FILE"
echo '    "results_file": "'$RESULTS_FILE'"' >> "$RESULTS_FILE"
echo '  }' >> "$RESULTS_FILE"
echo '}}' >> "$RESULTS_FILE"

# ---- Summary Output ----
echo ""
echo -e "${BLUE}=== Soak Verification Summary ===${NC}"
echo -e "Total runs:  $TOTAL_RUNS"
echo -e "${GREEN}Passed:      $PASSED${NC}"
echo -e "${RED}Failed:      $FAILED${NC}"
echo -e "Duration:    ${TOTAL_DURATION}s"
echo -e "Success rate: $(awk "BEGIN {printf \"%.1f%%\", $PASSED*100/$TOTAL_RUNS}")"
echo -e "Results:     $RESULTS_FILE"

if [[ $FAILED -gt 0 ]]; then
    echo ""
    echo -e "${RED}Failures:${NC}"
    for detail in "${FAILURE_DETAILS[@]}"; do
        echo "  - $detail"
    done
    echo ""
    echo -e "${RED}SOAK VERIFICATION FAILED${NC}"
    exit 1
else
    echo ""
    echo -e "${GREEN}SOAK VERIFICATION PASSED (${TOTAL_RUNS} matches)${NC}"
    exit 0
fi