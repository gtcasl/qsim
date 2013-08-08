user_guide.pdf: user_guide.tex qemu_diagram.pdf mod_qemu.pdf qsim_diagram.pdf \
                translation.pdf simple_example.pdf fast_forwarder.pdf \
                ../VERSION ../examples/simple.cpp
	sed s/%%%VERSION%%%/`cat ../VERSION`/ user_guide.tex > \
          user_guide.tmp.tex
	nl ../examples/simple.cpp > simple.cpp.num
	pdflatex user_guide.tmp.tex
	pdflatex user_guide.tmp.tex
	mv user_guide.tmp.pdf user_guide.pdf
	rm user_guide.tmp.tex
	rm simple.cpp.num

%.pdf : %.fig
	fig2mpdf $<

clean:
	rm -f *.pdf *.aux *.log *.toc *~ *\#
