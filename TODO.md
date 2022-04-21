- 把 volumeextent 从 inode 中拆分

  - [ ] protobuf 需要修改
  - [ ] 新增 rpc 接口，可以参考 cubefs

    - getextent，获取所有的extent，流式传输，或者分片获取
    - updateextent，更新一个或多个 extent
    - metaserver 存储格式

  - [ ] client端修改

    - 存储格式
    - 如何更新？