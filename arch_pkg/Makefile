
export BUILDDIR

default: PKGBUILD
	sed -i '/md5sums=/d' $<
	makepkg -g  >> $<
	makepkg

clean:
	@echo "Removing ARCH package build artifacts..."
	@rm -r pkg 2> /dev/null || true
	@rm -r src 2> /dev/null || true
	@rm *.tar.* 2> /dev/null || true

