SUBDIRS = src
dist_doc_DATA = README

EXTRA_DIST = arch_pkg/Makefile

#Generate package for use with pacman (Arch, Manjaro, etc.)
package: dist
	$(MAKE) -C arch_pkg SRCDEST=`pwd`

clean-local:
	$(MAKE) -C arch_pkg clean
	rm *.tar.gz 2> /dev/null || true
