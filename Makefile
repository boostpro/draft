### -*- mode: makefile-gmake -*-

# Note: If building on Mac OS X, and if you use MacPorts, the following ports
# should be installed:
#
#   texlive-latex
#   texlive-plain-extra
#   texlive-latex-recommended
#   texlive-latex-extra
#   texlive-fonts-recommended
#   texlive-fonts-extra

FIGURES = $(patsubst %.dot,%.pdf,$(wildcard source/*.dot))
SOURCES = $(wildcard source/*.tex)

all: std.pdf

%.pdf: %.dot
	dot -o $@ -Tpdf $<

grammar:
	(cd source ; sh ../tools/makegram)

xrefs:
	(cd source ; sh ../tools/makexref)

std.pdf: $(SOURCES) $(FIGURES) grammar xrefs
	(cd source ; pdflatex std)
	(cd source ; pdflatex std)
	(cd source ; pdflatex std)
	(cd source ; makeindex generalindex)
	(cd source ; makeindex libraryindex)
	(cd source ; makeindex grammarindex)
	(cd source ; makeindex impldefindex)
	(cd source ; pdflatex std)
	(cd source ; pdflatex std)
	(cd source ; pdflatex std)
	mv source/std.pdf $@
	@echo Draft standard compiled

convert2texi: convert2texi.cpp
	g++ -g2 -o $@ $<

source/std.texi: $(SOURCES) convert2texi
	./convert2texi -I source source/std.tex > $@
	perl -i -pe 's/``\@quotation''/``\@\@quotation''/g;' $@
	perl -i -pe 's/``\@end quotation''/``\@\@end quotation''/g;' $@
	texinfo-update $@

std.info: source/std.texi
	makeinfo --error-limit=10000 -o $@ $<
	@echo Draft standard converted to TeXinfo

info: std.info

clean:
	rm -f convert2texi std.info
	(cd source; rm -f *.aux *.texi)

### Makefile ends here
