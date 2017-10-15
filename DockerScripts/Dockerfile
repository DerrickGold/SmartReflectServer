FROM debian:jessie
MAINTAINER "Derrick Gold"

#grab dependencies
RUN apt-get update  && apt-get install -y git uuid-dev cmake gcc zlib1g-dev libssl-dev fortune-mod cowsay \
    && rm -rf /var/lib/apt/lists/*


#install libwebsockets
RUN git clone https://github.com/warmcat/libwebsockets.git \
    && cd libwebsockets \
	&& git checkout remotes/origin/v2.0-stable \
	&& mkdir build \
	&& cd build \
	&& cmake .. \
	&& make \
	&& make install \
	&& ldconfig

WORKDIR /

#delete libwebsockets repository
RUN rm -rf libwebsockets

#install smartreflectserver
RUN git clone https://github.com/DerrickGold/SmartReflectServer.git \
	&& cd SmartReflectServer \
	&& mkdir build \
	&& cd build \
	&& cmake .. \
	&& make



ENV SR_PORT 5000
VOLUME /SmartReflectServer/PluginDaemon/web/Plugins
EXPOSE 5000

ADD docker-entrypoint.sh /docker-entrypoint.sh

CMD ["/bin/bash", "-c", "/docker-entrypoint.sh"]


