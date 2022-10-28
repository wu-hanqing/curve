- [x] 集群部署
- [ ] chunkserver注册，启动
- [ ] client端获取端口及自己配置

    // 两种情况下
    // 1. 服务端不支持ucp
    // 2. 服务端支持ucp，且client端使用ucp
    //                 服务端不使用ucp 

- [ ] 工具获取服务的端口

1. internal/external只有ip不同，port是相同的，ucp需要遵循这个规则

unstable标记的是一个chunkserver，所以不需要感知其ip/port
