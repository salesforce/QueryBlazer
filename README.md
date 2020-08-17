# QueryBlazer

This is an official repository of QueryBlazer released in conjunction with the publication below.
```
Young Mo Kang, Wenhao Liu, and Yingbo Zhou, "QueryBlazer: Efficient Query Autocompletion Framework"
```
**This is not an official Salesforce's product.**

## Overview

QueryBlazer is a blazingly-fast generative query autocompletion (QAC) framework with applications in large-scale search engines.
QueryBlazer implements longest prefix match (LPM) subword unit encoder and employs classical n-gram language model to generate query completions.
QueryBlazer performs significantly better than conventional QAC methods in terms of completion accuracy, but also does it extremely fast.

## Technical Highlights

- Extremely fast and accurate completions
- Computationally efficient and does not need GPU resources
- Generative model, being able to complete _unseen_ queries
- Easy to use

## Prerequisite

Currently, QueryBlazer supports only *nix platforms.
Tested on Ubuntu & macOS.

```bash script
# first clone all submodules
git submodule update --init --recursive

# install OpenFST & NGram libraries
cd third_party
./install_openfst.sh

# build sentencepiece
cd sentencepiece && mkdir build && cd build
cmake .. -DCMAKE_CXX_FLAGS=-O2 && make -j4
cd ../..

# build KenLM with max n-gram order of 8 (configurable)
# KenLM requires boost library & eigen3 library
cd kenlm && mkdir build && cd build
cmake .. -DCMAKE_CXX_FLAGS=-O2 -DKENLM_MAX_ORDER=8 && make -j4
cd ../..
    
# back to project root
cd ..
```

## Installation
We will build some executable files as well as Python-binding library file.

```bash script
# make sure default python interpreter is python3
# you may want to load up python virtualenv
# source /your/python/virtual/env/bin/activate

# will create binary executables in build dir and Python-binding library in project root dir
mkdir build && cd build
# requires Boost library serialization module
cmake .. -DCMAKE_CXX_FLAGS=-O2
make -j4
cd ..
```

## Building a Docker Image

We provide `Dockerfile` that performs prerequisite & installation steps above to get you started quickly.
Please use a clean repository to build the docker image.

```bash script
git clone THIS_REPOSITORY && cd QueryBlazer
docker build . -t queryblazer
```

## Quick Start
Let's assume we have a train query dataset `train.txt`.
Each line in the file should be a single query.
If you don't have query dataset, AOL query [logs](https://jeffhuang.com/search_query_logs.html) may be a good starting point.

Note that QueryBlazer is case sensitive, 
so if you want case-insensitive QAC, 
make sure the train dataset is in lowercase letters only.

#### Extract Subword Vocabulary

First, we need to extract subword vocabulary from the train corpus.
QueryBlazer uses [sentencepiece](https://github.com/google/sentencepiece) style whitespace.
We extract vocabulary from sentencepiece.

```bash script
# extract vocabulary to subword.vocab file
# specify either 'bpe' or 'unigram' for --model_type
# you may want to experiment with different values for --vocab_size and --character_coverage options
# in general, larger vocab size leads to better prediction accuracy but larger model and slower runtime
# this may take some time, depending on your train dataset size
# if the dataset is too large, you may want to consider adding --input_sentence_size and --shuffle_input_sentence options 
third_party/sentencepiece/build/src/spm_train --model_type bpe --model_prefix subword --input train.txt --vocab_size 4096 --character_coverage 0.9995
```

#### Encoder Construction

Once we have the subword vocabulary, we need to create an LPM encoder from the vocabulary.

```bash script
# use subword vocabulary and create encoder.fst file
cut -f 1 subword.vocab | build/qbz_build_encoder /dev/stdin encoder.fst
```

#### Encode Train Corpus

Now, we need to encode the train dataset using the encoder just created

```bash script
# use LPM encoder to encode corpus into subword tokens, output to train.enc
build/qbz_encode encoder.fst train.txt > train.enc
```

#### Language Model Construction

Next, we use KenLM to train n-gram language model using the encoded train dataset

```bash script
# create n-gram language model and save to ngram.arpa
# you may want to experiment with different orders and pruning options
# this may take a long time and consume a lot of memory, depending on the dataset size
third_party/kenlm/build/bin/lmplz --order 8 --discount_fallback --prune 0 1 1 2 2 3 3 4 -T . < train.enc > ngram.arpa
```

We need to convert arpa format n-gram model to FST

```bash script
# convert ngram.arpa to ngram.fst
# this may take quite a bit
bash script/build_fst_model.sh encoder.fst ngram.arpa ngram.fst
```

#### Sanity Check

We are ready to test QueryBlazer!

```bash script
# Python binding to complete a single prefix 'autoc'
# PYTHONPATH is set to the directory containing the queryblazer python binding library
PYTHONPATH=`pwd` python -c 'from queryblazer import *; qbz = QueryBlazer(encoder="encoder.fst", model="ngram.fst"); print(qbz.Complete("autoc"))'
```

#### Precomputation

In order to take full advantage of QueryBlazer, we will precompute beam search results during the training stage,
so that we obtain significant improvement in runtime during evaluation.
However, it comes at a cost. 
Precomputation takes a long time and requires a big chunk of memory, especially for a large model.

```bash script
# Training stage
PYTHONPATH=`pwd` python
>>> from queryblazer import *
>>> ## set config as desired; make sure precompute is set
>>> config = Config(branch_factor=30, beam_size=30, topk=10, length_limit=100, precompute=True)
>>> ## this will take a very long time...
>>> qbz = QueryBlazer(encoder="encoder.fst", model="ngram.fst", config=config)
>>> ## this will save the precomputed result into 'precomputed.bin' file
>>> ## the file size varies depending on the model size & config topk option
>>> assert qbz.SavePrecomputed('precomputed.bin')

# Evaluation stage
PYTHONPATH=`pwd` python
>>> from queryblazer import *
>>> ## load the same config as before but make sure to unset precompute or omit (default is unset)
>>> config = Config(branch_factor=30, beam_size=30, topk=10, length_limit=100)
>>> qbz = QueryBlazer(encoder="encoder.fst", model="ngram.fst", config=config)
>>> ## load the precomputed results
>>> assert qbz.LoadPrecomputed('precomputed.bin')
>>> ## ready to run autocompletion!
>>> qbz.Complete('autoc')
```

Note that you must use the same or higher version of Boost for loading compared to saving precomputation.
That is, if you Boost 1.65 to precompute & save, then you must also use Boost 1.65 or above version to load it.
Otherwise, you will encounter `unsupported version` error while loading. 

#### Evaluation

We will assume we have a test queries file `test.txt` which again has one query per line.
We first extract prefixes from the test queries.

```bash
# read in each query per line and save each prefix/query pair per line, separated by a tab
python script/extract_prefix.py < test.txt > test.prefix.query
```

Next, we use QueryBlazer to generate query completions

```bash
# read in prefixes and generate query completions, saved in a single line separated by a tab
cut -f1 -d$'\t' test.prefix.query | build/qbz_test_queryblazer encoder.fst ngram.fst precomputed.bin /dev/stdin > test.completions
```

Finally, we evaluate our query completions

```bash
# read in completions and target queries for evaluation; optionally provide the train set to subdivide into seen vs unseen categories
cut -f2 -d$'\t' test.prefix.query | python script/eval.py --completions test.completions --query /dev/stdin --seen train.txt
```

## Configuration Options

For custom configurations, provide Config defined in `src/queryblazer.h` when creating a QueryBlazer instance.
 
* branch_factor: # of top transitions to explore per given beam
* beam_size: # of top beams to explore per each decoding iteration
* topk: # of top completion candidates to return
* length_limit: maximum # of subword tokens as a completion candidate
* precompute: compute beam search results in advance; recommended for production stage
* verbose: print out some logs

Note that precompute may take quite some time, and requires large memory.
Precomputation will automatically run multithreaded, utilizing all avaiable cores.
For low memory environment, one can reduce the model size by (at the expense of losing prediction accuracy)
lower n-gram order and/or aggressive pruning option during language model construction.


## Integration

#### C++ Library

`src/test_queryblazer.cc` demonstrates how to directly integrate QueryBlazer library into a C++ application.

#### Python Library

Python binding provides a convenient way to integrate QueryBlazer to web servers.
Python binding classes and methods are defined in `src/queryblazer.cc`.

## Contributions

All contributions are welcome. 
Please submit a PR.
