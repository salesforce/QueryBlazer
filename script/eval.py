'''
/*
* Copyright (c) 2018, salesforce.com, inc.
* All rights reserved.
* SPDX-License-Identifier: BSD-3-Clause
* For full license text, see the LICENSE file in the repo root or https://opensource.org/licenses/BSD-3-Clause
*/
'''

import argparse
import numpy as np

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--query', type=str, required=True)
    parser.add_argument('--completions', type=str, required=True)
    parser.add_argument('--seen', type=str, default='')
    parser.add_argument('--topk', type=int, default=10)

    args = parser.parse_args()

    if args.seen:
        with open(args.seen, 'r', encoding='utf-8') as f:
            seen = set([l.strip() for l in f])
    else:
        seen = set()

    ranks = []
    seen_ranks = []
    unseen_ranks = []
    with open(args.query, 'r', encoding='utf-8') as q, \
        open(args.completions, 'r', encoding='utf-8') as c:
        for qline, cline in zip(q,c):
            query = qline.strip()
            completions = cline.strip().split('\t')
            try:
                ranks.append(completions.index(query) + 1)
            except:
                ranks.append(0)
            if query in seen:
                seen_ranks.append(ranks[-1])
            else:
                unseen_ranks.append(ranks[-1])


    mrr_scores = [0.] + [1.0/x for x in range(1,args.topk + 1)]
    mrr_scores = np.asarray(mrr_scores)
    sr_scores = [0.] + [1.0] * args.topk
    sr_scores = np.asarray(sr_scores)
    ranks = np.asarray(ranks)

    for rank,dtype in zip([seen_ranks, unseen_ranks, ranks], ["seen", "unseen", "total"]):
        mrr = np.mean([mrr_scores[i] for i in rank])
        sr = np.mean([sr_scores[i] for i in rank])
        print('# %s queries: %d' % (dtype, len(rank)))
        print('MRR: %0.4f' % mrr)
        print('Success Rate: %0.4f' % sr)


if __name__ == '__main__':
    main()