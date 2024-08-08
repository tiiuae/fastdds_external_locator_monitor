FROM ghcr.io/tiiuae/fog-ros-baseimage-builder:v3.3.0

# config dependencies install
ARG DEBIAN_FRONTEND=noninteractive
RUN echo -e '\
APT::Install-Recommends "0";\n\
APT::Install-Suggests "0";\n\
' > /etc/apt/apt.conf.d/01norecommend

ARG UNDERLAY_WS=/main_ws/underlay_ws
ARG OVERLAY_WS=/main_ws/overlay_ws

WORKDIR $UNDERLAY_WS
COPY fastdds_statistics_backend.repos $UNDERLAY_WS/src/fastdds_monitor_tutorial/underlay.repos

# Import the underlay dependencies that doesn't exist in SDK image
RUN vcs import ./src/ < $UNDERLAY_WS/src/fastdds_monitor_tutorial/underlay.repos
COPY locator.patch $UNDERLAY_WS/src/fastdds_statistics_backend/locator.patch

RUN ls -la $UNDERLAY_WS/src && \
    git -C $UNDERLAY_WS/src/fastdds_statistics_backend apply locator.patch && \
    . /usr/bin/ros_setup.bash && \
    colcon build --cmake-args -DBUILD_TESTING=0

RUN apt-get update && \
    apt-get install -y \
      nlohmann-json-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR $OVERLAY_WS

COPY . $OVERLAY_WS/src/fastdds_monitor_tutorial

RUN . $UNDERLAY_WS/install/setup.sh && \
    . /usr/bin/ros_setup.bash && \
    colcon build --cmake-args -DBUILD_TESTING=0 && \
    cp $OVERLAY_WS/install/fastdds_monitor_tutorial/lib/fastdds_monitor_tutorial/fastdds_monitor $OVERLAY_WS/

ENTRYPOINT ["/bin/bash", "-c", ". /main_ws/overlay_ws/install/setup.bash && ros2 run fastdds_monitor_tutorial fastdds_monitor"]
