## FastDDS locator identification using fastdds_statistics_backend

Build docker image

```bash
docker build --output type=docker -t fastdds_monitor .
```

Run
```bash
docker run --rm -it --network="host" --name=fastdds-monitor fastdds_monitor:latest
```

At the initial start, there might be quite a log about the discoveries of topics and participants.

After a while, every five seconds, the locators for each participant will be logged.

### Ros2 node name matching

Normally, the participants do not have a name in the DDS network. However, each of the ROS2 node has the default services as (`/NAMESPACE/NODE_NAME/get_parameters`). The script uses the regex below to match the ROS2 nodes' topics and participants using the first partition of the GUID.

```
^DataReader_rq/(.+)/get_parametersRequest_[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+$
```
