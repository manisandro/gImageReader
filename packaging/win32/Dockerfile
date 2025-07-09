FROM fedora:rawhide

MAINTAINER Sandro Mani <manisandro@gmail.com>

RUN dnf -y update; dnf clean all

ADD mingwdeps.sh /tmp/mingwdeps.sh

RUN /tmp/mingwdeps.sh


WORKDIR /workspace
VOLUME ["/workspace"]
