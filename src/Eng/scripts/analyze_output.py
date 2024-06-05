
import json
import re
import sys

def process_file(f, all_tests):
    for line in f.readlines():
        m = re.search(r'^Test\s*(\w+).*PSNR:\s*([.\w]+)\/([.\w]+).*', line)
        if m:
            test_name, psnr_tested, psnr_threshold = m[1], float(m[2]), float(m[3])
            if test_name not in all_tests:
                new_test = {}
                new_test['psnr_tested'] = psnr_tested
                new_test['psnr_threshold'] = psnr_threshold

                all_tests[test_name] = new_test
            else:
                test = all_tests[test_name]
                if 'required_samples' not in test:
                    test['psnr_tested'] = min(test['psnr_tested'], psnr_tested)

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
        if test['psnr_tested'] == test['psnr_threshold']:
            del all_tests[test_name]

    # Print failed tests first
    condition = lambda key, value: value['psnr_tested'] < value['psnr_threshold'] or (value['psnr_tested'] - value['psnr_threshold']) > 0.5
    failed_tests = {key: value for key, value in all_tests.items() if condition(key, value)}

    print("-- Failed tests --")
    print(json.dumps(failed_tests, indent=4))

    print("-- All tests --")
    print(json.dumps(all_tests, indent=4))

    sys.exit(-1 if len(failed_tests) > 0 else 0)

if __name__ == "__main__":
    main()
