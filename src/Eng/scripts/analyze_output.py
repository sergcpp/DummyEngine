
import json
import re
import sys

# Firefly thresholds are noise caps, not targets: flag any threshold >FIRE_SLACK_DIV x the worst measured count (debug-dump noise spans ~80-500 px across pipelines)
FIRE_SLACK_DIV = 3
# Signed guards print on the same line as PSNR. Flattening is an upper bound in luma units, Detail ratio is a lower bound;
# these caps are shared across all frames of a scene while the metrics swing with content, so a guard is only dead when it
# misses the worst measurement over all tests by more than slack
FLAT_SLACK = 1.0
DETAIL_SLACK = 0.10

# Worst-side keep per field: 'min' for lower bounds (PSNR, Detail), 'max' for upper bounds (Fireflies, Flattening)
FIELDS = (('PSNR', 'psnr', float, 'min'),
          ('Flattening', 'flattening', float, 'max'),
          ('Detail', 'detail', float, 'min'),
          ('Fireflies', 'fireflies', int, 'max'))

def process_file(f, all_tests):
    for line in f.readlines():
        m = re.search(r'^Test\s*(\w+)', line)
        if not m:
            continue
        test_name = m[1]
        for tag, key, cast, worst in FIELDS:
            m = re.search(tag + r':\s*([+-]?[\d.]+)/([\d.]+)', line)
            if not m:
                continue
            tested, threshold = cast(m[1]), cast(m[2])
            if test_name not in all_tests:
                all_tests[test_name] = {key + '_tested': tested, key + '_threshold': threshold}
            else:
                test = all_tests[test_name]
                if key + '_tested' in test:
                    test[key + '_tested'] = min(test[key + '_tested'], tested) if worst == 'min' else max(test[key + '_tested'], tested)
                else:
                    test[key + '_tested'] = tested
                    test[key + '_threshold'] = threshold

def test_failed(key, value, flat_worst, detail_worst):
    if 'psnr_tested' in value:
        if value['psnr_tested'] < value['psnr_threshold'] or (value['psnr_tested'] - value['psnr_threshold']) > 0.5:
            return True
    if 'fireflies_tested' in value:
        if value['fireflies_tested'] > value['fireflies_threshold']:
            return True
        if value['fireflies_threshold'] > FIRE_SLACK_DIV * value['fireflies_tested']:
            return True
    if 'flattening_tested' in value:
        if value['flattening_tested'] > value['flattening_threshold']:
            return True
        if flat_worst is not None and value['flattening_threshold'] > flat_worst + FLAT_SLACK:
            return True
    if 'detail_tested' in value:
        if value['detail_tested'] < value['detail_threshold']:
            return True
        if detail_worst is not None and value['detail_threshold'] < detail_worst - DETAIL_SLACK:
            return True
    return False

def main():
    all_tests = {}

    # Loop through all test results and keep the worst measured / first threshold values per test and field
    for i in range(1, len(sys.argv)):
        try:
            with open(sys.argv[i], "r", encoding='utf-8') as f:
                process_file(f, all_tests)
        except:
            try:
                with open(sys.argv[i], "r", encoding='utf-16') as f:
                    process_file(f, all_tests)
            except:
                print("Failed to process ", sys.argv[i])

    # Remove exact matches
    for test_name in list(all_tests.keys()):
        test = all_tests[test_name]
        if 'psnr_tested' in test and test['psnr_tested'] == test['psnr_threshold']:
            del all_tests[test_name]

    # Print failed tests first
    flat_worst = max((t['flattening_tested'] for t in all_tests.values() if 'flattening_tested' in t), default=None)
    detail_worst = min((t['detail_tested'] for t in all_tests.values() if 'detail_tested' in t), default=None)
    failed_tests = {key: value for key, value in all_tests.items() if test_failed(key, value, flat_worst, detail_worst)}

    print("-- Failed tests --")
    print(json.dumps(failed_tests, indent=4))

    print("-- All tests --")
    print(json.dumps(all_tests, indent=4))

    sys.exit(-1 if len(failed_tests) > 0 else 0)

if __name__ == "__main__":
    main()
