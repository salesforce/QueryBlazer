# download & install openfst & opengrm ngram
for dir in openfst-1.7.7 ngram-1.3.10; do
    if [ -d "$dir" ]; then
        echo "$dir already exists" && exit 255
    fi
done

for file in "openfst-1.7.7.tar.gz" "ngram-1.3.10.tar.gz"; do
    if [ -f "$file" ]; then
        rm $file
    fi
done

wget http://www.openfst.org/twiki/pub/FST/FstDownload/openfst-1.7.7.tar.gz
tar xfz openfst-1.7.7.tar.gz && cd openfst-1.7.7
OPENFSTINC=$(pwd)/src/include
mkdir build && cd build
OPENFSTBUILD=$(pwd)/src
../configure --enable-far --prefix $(pwd) && make -j4
cd ../..

wget http://www.opengrm.org/twiki/pub/GRM/NGramDownload/ngram-1.3.10.tar.gz
tar xfz ngram-1.3.10.tar.gz && cd ngram-1.3.10
mkdir build && cd build
CPPFLAGS="-I$OPENFSTINC" LDFLAGS="-L$OPENFSTBUILD/lib -L$OPENFSTBUILD/script -L$OPENFSTBUILD/extensions/far" ../configure --prefix $(pwd) && make -j4
cd ../..
