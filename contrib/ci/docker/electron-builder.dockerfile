FROM registry.oxen.rocks/lokinet-ci-debian-win32-cross
RUN /bin/bash -c 'apt install -q -y curl'
RUN /bin/bash -c 'curl -sSL https://deb.nodesource.com/setup_14.x | bash -'
RUN /bin/bash -c 'apt install -q -y wine'
