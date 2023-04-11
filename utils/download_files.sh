DOWNLOAD_SIZE=.50MB
CUT_SIZE=10MiB
# run in project root directory
curl -o data/sources.gz http://pizzachili.dcc.uchile.cl/texts/code/sources$DOWNLOAD_SIZE.gz
curl -o data/english.gz http://pizzachili.dcc.uchile.cl/texts/nlang/english$DOWNLOAD_SIZE.gz
curl -o data/dna.gz http://pizzachili.dcc.uchile.cl/texts/dna/dna$DOWNLOAD_SIZE.gz

(
cd data
gzip -d sources.gz
gzip -d english.gz
gzip -d dna.gz
head -c $CUT_SIZE dna > dna.$CUT_SIZE
head -c $CUT_SIZE english > english.$CUT_SIZE
head -c $CUT_SIZE source > source.$CUT_SIZE
)

