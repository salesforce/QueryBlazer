'''
/*
* Copyright (c) 2018, salesforce.com, inc.
* All rights reserved.
* SPDX-License-Identifier: BSD-3-Clause
* For full license text, see the LICENSE file in the repo root or https://opensource.org/licenses/BSD-3-Clause
*/
'''

import sys
import argparse
import random
import math
import io


"""
Split a file into train, valid, and test
Ignores empty lines
"""

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--seed', type=int, default=42)
    parser.add_argument('--valid', type=float, default=0.1)
    parser.add_argument('--test', type=float, default=0.1)
    parser.add_argument('--output_prefix', type=str, default='')

    args = parser.parse_args()
    assert 0 <= args.valid < 1
    assert 0 <= args.test < 1
    assert args.valid + args.test < 1

    train_file = args.output_prefix + '_train.txt'
    valid_file = args.output_prefix + '_val.txt'
    test_file = args.output_prefix + '_test.txt'

    random.seed(args.seed)

    queries = []
    with io.TextIOWrapper(sys.stdin.buffer, encoding='utf-8') as f:
        for line in f:
            query = line.rstrip()
            if len(query) == 0: continue
            query = bytes(query, 'utf-8').decode('utf-8', 'ignore')
            queries.append(query)

    random.shuffle(queries)
    n_valid = math.floor(len(queries) * args.valid)
    n_test = math.floor(len(queries) * args.test)
    n_train = len(queries) - n_valid - n_test

    with open(train_file, 'w', encoding='utf8') as f:
        f.write('\n'.join(queries[:n_train]))
    with open(valid_file, 'w', encoding='utf8') as f:
        f.write('\n'.join(queries[n_train:n_train+n_valid]))
    with open(test_file, 'w', encoding='utf8') as f:
        f.write('\n'.join(queries[-n_test:]))


if __name__ == '__main__':
    main()
