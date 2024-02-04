FROM mcr.microsoft.com/devcontainers/base:debian-12
USER root

ARG CURL_VERSION=8.6.0
ARG CURL_SUFFIX=win32-mingw

ENV CURL_PREFIX="curl-${CURL_VERSION}_1-${CURL_SUFFIX}"
ENV CURL_DOWNLOAD="https://curl.se/windows/dl-${CURL_VERSION}_1/${CURL_PREFIX}.zip"

ARG XWIN_VERSION=0.5.0
ARG XWIN_TRIPLE=x86_64-unknown-linux-musl

ENV XWIN_PREFIX="xwin-${XWIN_VERSION}-${XWIN_TRIPLE}"
ENV XWIN_DOWNLOAD="https://github.com/Jake-Shadle/xwin/releases/download/${XWIN_VERSION}/${XWIN_PREFIX}.tar.gz"

# install packages
RUN \
	set -eux; \
	export DEBIAN_FRONTEND=noninteractive; \
	apt-get update; \
	apt-get install -y build-essential cppcheck valgrind clang-16 lldb-16 lld-16 llvm-16 gdb; \
	apt-get autoremove -y; \
	apt-get clean -y;

# create necessary symlinks
RUN \
	set -eux; \
	ln -s clang-16 /usr/bin/clang-cl-16;

# install curl
RUN \
	set -eux; \
	curl --fail -L "$CURL_DOWNLOAD" > /tmp/curl.zip; \
	unzip /tmp/curl.zip -d /tmp; \
	mv /tmp/${CURL_PREFIX} /opt/curl; \
	rm /tmp/curl.zip;

# install xwin
RUN \
	set -eux; \
	curl --fail -L "$XWIN_DOWNLOAD" | tar -xzC /tmp; \
	/tmp/${XWIN_PREFIX}/xwin --arch x86 --accept-license splat --output /opt/xwin; \
	rm -rf .xwin_cache /tmp/${XWIN_PREFIX};
