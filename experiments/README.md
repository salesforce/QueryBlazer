# Data Preparation

To run a query autocompletion experiment, we need two files: `train.txt` and `test.txt`.
Below, we show how we obtained the experimental data reported in the paper.
Once the train and test query files are prepared, refer to `Quick Start` to run an experiment.

## AOL Dataset

Clone https://github.com/clovaai/subword-qac (commit df7a9a5) and follow it up to `Prepare Data` section
using the default setting. That is, download the data and split it into train, valid, and test sets.
When the split script is complete, use `train.query.txt` and `test.query.txt` files 
from `data/aol/full/` directory as the train and test sets.