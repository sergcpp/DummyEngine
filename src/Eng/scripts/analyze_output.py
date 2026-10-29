
import json
import re
import sys

# Firefly thresholds are noise caps, not targets: flag any threshold >FIRE_SLACK_DIV x the worst measured count (debug-dump noise spans ~80-500 px across pipelines)
FIRE_SLACK_DIV = 3

def process_file(f, all_tests):
    for line in f.readlines():
        m = re.search(r'^Test\s*(\w+).*PSNR:\s*([.\w]+)\/([.\w]+).*', line)
        if m:
            test_name, psnr_tested, psnr_threshold = m[1], float(m[2]), float(m[3])
            if test_name not in all_tests:
                all_tests[test_name] = {'psnr_tested': psnr_tested, 'psnr_threshold': psnr_threshold}
            else:
                test = all_tests[test_name]
                if 'psnr_tested' in test:
                    test['psnr_tested'] = min(test['psnr_tested'], psnr_tested)
                else:
                    test['psnr_tested'] = psnr_tested
                    test['psnr_threshold'] = psnr_threshold
            continue

        m = re.search(r'^Test\s*(\w+).*Fireflies:\s*(\d+)\/(\d+).*', line)
        if m:
            test_name, fireflies_tested, fireflies_threshold = m[1], int(m[2]), int(m[3])
            if test_name not in all_tests:
                all_tests[test_name] = {'fireflies_tested': fireflies_tested, 'fireflies_threshold': fireflies_threshold}
            else:
                test = all_tests[test_name]
                if 'fireflies_tested' in test:
                    test['fireflies_tested'] = max(test['fireflies_tested'], fireflies_tested)
                else:
                    test['fireflies_tested'] = fireflies_tested
                    test['fireflies_threshold'] = fireflies_threshold

def test_failed(key, value):
    if 'psnr_tested' in value:
        if value['psnr_tested'] < value['psnr_threshold'] or (value['psnr_tested'] - value['psnr_threshold']) > 0.5:
            return True
    if 'fireflies_tested' in value:
        if value['fireflies_tested'] > value['fireflies_threshold']:
            return True
        if value['fireflies_threshold'] > FIRE_SLACK_DIV * value['fireflies_tested']:
            return True
    return False

def main():
    all_tests = {}

    # Loop through all test results and keep minimal 'PSNR' and maximal 'Fireflies' values
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
    failed_tests = {key: value for key, value in all_tests.items() if test_failed(key, value)}

    print("-- Failed tests --")
    print(json.dumps(failed_tests, indent=4))

    print("-- All tests --")
    print(json.dumps(all_tests, indent=4))

    sys.exit(-1 if len(failed_tests) > 0 else 0)

if __name__ == "__main__":
    main()
