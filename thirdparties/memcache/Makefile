
.PHONY: download build clean

build: clean download
	@cd libmemcached-1.1.2 && mkdir build-libmemcached && cd build-libmemcached && cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/root/curve/thirdparties/memcache/libmemcached .. && make libmemcached -j$$(nproc) && make install -j2

download: clean
	@wget https://curve-build.nos-eastchina1.126.net/memcache/libmemcached-1.1.2.tar.gz && tar -zxf libmemcached-1.1.2.tar.gz

clean:
	@rm -fr libmemcached*
