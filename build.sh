if [ ! -d buildDirARM64 ]; then
		echo "buildDirARM64 dir created"
		mkdir buildDirARM64
fi
if [ ! -d buildDirx86 ]; then
		echo "buildDirX86 dir created"
		mkdir buildDirX86
fi
git submodule init
git submodule update
#if [ ! -d qemu ]; then
#    git clone https://github.com/gthparch/qemu.git
#    git checkout feature/qsim_plugin
#fi
cd buildDirARM64
 ../qemu/configure --target-list=aarch64-softmmu --enable-plugins
 make -j 4 &
cd ..
cd buildDirX86
../qemu/configure --target-list=i386-softmmu --enable-plugins
 make -j 4

