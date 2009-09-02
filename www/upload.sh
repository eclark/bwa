./cleanup.sh
find . -name "*.html" | xargs ./csi2ssi.pl
mv index.html index.true.html
rsync -Cavz * lh3lh3,bio-bwa@web.sourceforge.net:htdocs/
./cleanup.sh