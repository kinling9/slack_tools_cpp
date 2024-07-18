# Use the official CentOS 7 image
FROM centos:7

# Install the necessary tools to compile C++
# RUN sed -i.bak \
#     -e 's|^mirrorlist=|#mirrorlist=|g' \
#     -e 's|^#baseurl=http://mirror.centos.org/centos|baseurl=https://mirrors.ustc.edu.cn/centos-vault/centos|g' \
#     /etc/yum.repos.d/CentOS-*.repo && \
#     yum clean all && \
#     yum makecache
RUN sed -e "s|^mirrorlist=|#mirrorlist=|g" \
    -e "s|^#baseurl=http://mirror.centos.org/centos/\$releasever|baseurl=https://mirrors.tuna.tsinghua.edu.cn/centos-vault/7.9.2009|g" \
    -e "s|^#baseurl=http://mirror.centos.org/\$contentdir/\$releasever|baseurl=https://mirrors.tuna.tsinghua.edu.cn/centos-vault/7.9.2009|g" \
    -i.bak \
    /etc/yum.repos.d/CentOS-*.repo && \
    yum clean all && \
    yum makecache
RUN yum -y update && \
    yum -y install gcc gcc-c++ make python3 epel-release && \
    yum -y install ninja-build cmake3 bzip2 zlib-devel && \
    ln -s /usr/bin/cmake3 /usr/bin/cmake
# RUN yum -y install centos-release-scl && \
#     yum -y install devtoolset-10-gcc

RUN curl -LO https://mirrors.aliyun.com/blfs/conglomeration/boost/boost_1_84_0.tar.bz2 && \
    tar --bzip2 -xf boost_1_84_0.tar.bz2

RUN yum -y install bzip2 wget gmp-devel mpfr-devel libmpc-devel

RUN wget https://mirrors.aliyun.com/gnu/gcc/gcc-10.2.0/gcc-10.2.0.tar.xz && \
    tar -xf gcc-10.2.0.tar.xz && \
    rm -rf gcc-10.2.0.tar.xz

RUN mkdir gcc-build && \
    cd gcc-build && \
    ../gcc-10.2.0/configure --enable-languages=c,c++ --prefix=/usr/local --disable-multilib && \
    make -j 8 && \
    make install && \
    cd .. && \
    rm -rf gcc-build

RUN rm boost_1_84_0.tar.bz2 && \
    cd boost_1_84_0 && \
    ls && \
    ./bootstrap.sh  --prefix=/usr/local --with-libraries=iostreams && \
    LD_LIBRARY_PATH=/usr/local/lib64/:$LD_LIBRARY_PATH ./b2 install && \
    cd / && \
    rm -rf boost_1_84_0


# Create a directory for the application
WORKDIR /app
