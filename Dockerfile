# Ubuntu 16.04 LTS with Baseimage and Runit
FROM phusion/baseimage:0.10.1 AS builder

WORKDIR /app/mapping-core
COPY . /app/mapping-core

RUN DEBIAN_FRONTEND=noninteractive \
    # update packages and upgrade system
    apt-get update && \
    # install OpenCL
    chmod +x docker-files/install-opencl-build.sh && \
    docker-files/install-opencl-build.sh && \
    # install MAPPING dependencies
    chmod +x docker-files/ppas.sh && \
    docker-files/ppas.sh && \
    python3 docker-files/read_dependencies.py docker-files/dependencies.csv "build dependencies" \
        | xargs -d '\n' -- apt-get install --yes && \
    # Build MAPPING
    cmake -DCMAKE_BUILD_TYPE=Release . && \
    make -j$(cat /proc/cpuinfo | grep processor | wc -l)

# Ubuntu 16.04 LTS with Baseimage and Runit
FROM phusion/baseimage:0.10.1

WORKDIR /app
COPY --from=builder /app/mapping-core/target/bin /app

COPY docker-files /app/docker-files

RUN DEBIAN_FRONTEND=noninteractive \
    # update packages and upgrade system
    apt-get update && \
    apt-get upgrade --yes -o Dpkg::Options::="--force-confold" && \
    # install OpenCL
    chmod +x docker-files/install-opencl-runtime.sh && \
    docker-files/install-opencl-runtime.sh && \
    # install MAPPING dependencies
    chmod +x docker-files/ppas.sh && \
    docker-files/ppas.sh && \
    python3 docker-files/read_dependencies.py docker-files/dependencies.csv "runtime dependencies" \
        | xargs -d '\n' -- apt-get install --yes && \
    # Make mountable files and give rights to www-data
    chown www-data:www-data . && \
    touch userdb.sqlite && \
    chown www-data:www-data userdb.sqlite && \
    mkdir gdalsources_data && \
    chown www-data:www-data gdalsources_data && \
    mkdir gdalsources_description && \
    chown www-data:www-data gdalsources_description && \
    mkdir ogrsources_data && \
    chown www-data:www-data ogrsources_data && \
    mkdir ogrsources_description && \
    chown www-data:www-data ogrsources_description && \
    # Make servie available
    mkdir --parents /etc/service/mapping/ && \
    mv docker-files/mapping-service.sh /etc/service/mapping/run && \
    chmod +x /etc/service/mapping/run && \
    ln -sfT /dev/stderr /var/log/mapping.log && \
    # Clean APT and install scripts
    apt-get clean && \
    rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/* /app/docker-files

# Make port 10100 available to the world outside this container
EXPOSE 10100

# Expose mountable volumes
VOLUME /app/gdalsources_data \
       /app/gdalsources_description \
       # /app/userdb.sqlite \
       # /app/conf/settings.toml \
       /app/ogrsources_data \
       /app/ogrsources_description

# Use baseimage-docker's init system.
CMD ["/sbin/my_init"]
