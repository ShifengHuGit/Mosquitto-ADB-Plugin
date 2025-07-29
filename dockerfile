# ---------- 构建阶段 ----------

FROM debian:bullseye as builder

WORKDIR /opt

RUN apt-get update && apt-get install -y \
    libstdc++6 \
    curl unzip make cmake wget xsltproc \
    gcc g++ build-essential \
    libssl-dev \
    libaio1 libnsl2 libc6 libgcc-s1 \
    git \
    && rm -rf /var/lib/apt/lists/*
    


# 下载并构建 ODPI-C
# 构建 ODPI-C（v5.1.0）
# 下载并编译 ODPI-C（推荐版本 v5.1.0）
RUN curl -LO https://github.com/oracle/odpi/archive/refs/tags/v5.1.0.zip && \
    unzip v5.1.0.zip && \
    mv odpi-5.1.0 odpi && \
    cd odpi && make


# 解压 Oracle Instant Client（你提前下载好 zip）
COPY instantclient-basic-linux.x64-19.28.0.0.0dbru.zip /opt/instantclient-basic.zip
RUN unzip /opt/instantclient-basic.zip -d /opt/oracle/ && \
    ln -s /opt/oracle/instantclient_* /opt/oracle/instantclient && \
    cd /opt/oracle/instantclient && ln -s libclntsh.so.* libclntsh.so && \
    ln -s libocci.so.* libocci.so || true

# 获取 Mosquitto 源码（官方建议）
WORKDIR /opt
RUN wget -q https://mosquitto.org/files/source/mosquitto-2.0.18.tar.gz && \
tar xf mosquitto-2.0.18.tar.gz > /dev/null && \
    mv mosquitto-2.0.18 mosquitto


# 编译（只需要头文件时，可以只做 install step）
# WORKDIR /opt/mosquitto
# #RUN cmake -DWITH_PLUGINS=ON -DWITH_STATIC_LIBRARIES=OFF -DWITH_DOCS=OFF  -DWITH_TLS=ON -DCMAKE_INSTALL_PREFIX=/usr/local . && \
# #    make && make install

# RUN  cmake -Bbuild -H. -DWITH_STATIC_LIBRARIES=OFF -DWITH_TLS=ON && \
#      make -C build   


     # 拷贝源码
WORKDIR /build
COPY json.hpp /plugin/
COPY plugin/ /build/


RUN mkdir -p /build/odpi-obj && \
    for src in /opt/odpi/src/*.c; do \
        gcc -c "$src" -I/opt/odpi/include -fPIC -o "/build/odpi-obj/$(basename "$src" .c).o"; \
    done


RUN g++ -fPIC -shared -o mosq_adb_plugin.so mosq_adb_plugin.cpp \
    /build/odpi-obj/*.o \
    -I/opt/odpi/include \
    -I/plugin \
    -I/opt/oracle/instantclient \
    -I/opt/mosquitto/include \
    -L/opt/oracle/instantclient -lclntsh \
    -Wl,--no-as-needed -lclntsh \
    -Wno-write-strings


# ---------- 最小运行阶段 ----------
# 运行阶段：用 Debian 作为基础运行环境
FROM debian:bullseye AS runtime

# 安装必要的运行时库（glibc 环境）
RUN apt-get update && apt-get install -y \
    wget unzip libssl1.1 libaio1 libnsl2 libstdc++6 libgcc-s1  && \
    rm -rf /var/lib/apt/lists/*

# 安装 Mosquitto
RUN apt-get update && apt-get install -y mosquitto 
WORKDIR /opt/install
RUN wget https://download.oracle.com/otn_software/linux/instantclient/191000/instantclient-basiclite-linux.x64-19.10.0.0.0dbru.zip && \
    wget https://download.oracle.com/otn_software/linux/instantclient/191000/instantclient-sqlplus-linux.x64-19.10.0.0.0dbru.zip && \
    unzip instantclient-basiclite-linux.x64-19.10.0.0.0dbru.zip && \
    unzip instantclient-sqlplus-linux.x64-19.10.0.0.0dbru.zip && \
    rm *.zip

# 创建插件目录
RUN mkdir -p /mosquitto/plugins


# 拷贝插件 .so 和 config 文件
COPY --from=builder /build/mosq_adb_plugin.so /mosquitto/plugins/
COPY --from=builder /build/plugin-config.json /mosquitto/
COPY --from=builder /opt/oracle/instantclient /opt/oracle/instantclient
COPY --from=builder /opt/mosquitto /opt/mosquitto
# 设置 LD_LIBRARY_PATH
ENV LD_LIBRARY_PATH=/opt/oracle/instantclient
# 设置环境变量
ENV TNS_ADMIN=/opt/oracle/wallets/adb

# 拷贝 mosquitto.conf
COPY mosquitto.conf /mosquitto/config/mosquitto.conf
COPY Wallet_IoTdatabase /opt/oracle/wallets/adb
COPY plugin/plugin-config.json /mosquitto/plugin-config.json 
# 默认启动
CMD ["/usr/sbin/mosquitto", "-c", "/mosquitto/config/mosquitto.conf"]
