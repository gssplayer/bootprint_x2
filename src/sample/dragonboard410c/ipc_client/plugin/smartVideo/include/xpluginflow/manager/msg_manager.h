/*!
 * -------------------------------------------
 * Copyright (c) 2019, Horizon Robotics, Inc.
 * All rights reserved.
 * \File     msg_manager.h
 * \Author   Yingmin Li
 * \Mail     yingmin.li@horizon.ai
 * \Version  1.0.0.0
 * \Date     2019-04-22
 * \Brief    implement of msg_manager.h
 * \DO NOT MODIFY THIS COMMENT, \
 * \WHICH IS AUTO GENERATED BY EDITOR
 * -------------------------------------------
 */

#ifndef XPLUGINFLOW_INCLUDE_XPLUGINFLOW_MANAGER_MSG_MANAGER_H_
#define XPLUGINFLOW_INCLUDE_XPLUGINFLOW_MANAGER_MSG_MANAGER_H_

#include <iostream>

#include <map>
#include <memory>
#include <mutex>
#include <vector>
#include <string>
#include <utility>
#include <unordered_map>
#include "xpluginflow/message/pluginflow/flowmsg.h"
#include "xpluginflow/plugin/xplugin.h"
#include "xpluginflow/utils/singleton.h"
#include "xpluginflow/threads/threadpool.h"
#include "xpluginflow/message/pluginflow/msg_registry.h"
#include "xpluginflow/proxy/proxy.h"
#include "hobotlog/hobotlog.hpp"

namespace horizon {
namespace vision {
namespace xpluginflow {
class XMsgQueue : public hobot::CSingleton<XMsgQueue> {
 public:
  XMsgQueue() {
    msg_handle_.CreatThread(1);
    int rv = Init();
    if (rv != 0) {
      throw rv;
    }
  }
  ~XMsgQueue() {
    XProxy::Instance().Deinit();
  }

  int Init() {
      return XProxy::Instance().Init(
              std::bind(&XMsgQueue::PushMsgInternal, this,
                      std::placeholders::_1),
              std::bind(&XMsgQueue::MsgUnserialize, this,
                      std::placeholders::_1,
                      std::placeholders::_2));
  }

 public:
  void RegistMsgType(const std::string& type,
          XPluginFlowMessagePtr (*factory)(const std::string &)) {
      std::lock_guard<std::mutex> lock(m_MsgDeserializeMapMutex);
      if (!m_MsgDeserializeMap.count(type)) {
         m_MsgDeserializeMap.insert(std::make_pair(type, factory));
      }
  }

  void RegisterPlugin(const XPluginPtr &plugin, const std::string& msg_type) {
    std::lock_guard<std::mutex> lck(mutex_);
    auto type_handle = XPluginMsgRegistry::Instance().RegisterOrGet(msg_type);
    HOBOT_CHECK(type_handle != XPLUGIN_INVALID_MSG_TYPE)
      << "try to register invalid msg type:" << msg_type
      << ", for plugin " << plugin->desc();
    table_[type_handle].push_back(plugin);
    // 向gateway注册
    XProxy::Instance().RegisterMsg(msg_type);
  }
  void UnRegisterPlugin(const XPluginPtr &plugin, const std::string& type) {
    std::lock_guard<std::mutex> lck(mutex_);
    // todo
  }

  void PushMsg(XPluginFlowMessagePtr msg) {
    XProxy::Instance().PublishMsg(msg);
  }

 private:
  // 消息反序列化
  XPluginFlowMessagePtr MsgUnserialize(const std::string &type,
          const std::string &data) {
    std::lock_guard<std::mutex> lock(m_MsgDeserializeMapMutex);
    if (m_MsgDeserializeMap.count(type)) {
       return m_MsgDeserializeMap[type](data);
    }
    return NULL;
  }

  void PushMsgInternal(XPluginFlowMessagePtr msg) {
      msg_handle_.PostTask(std::bind(&XMsgQueue::Dispatch, this, msg));
  }

  void Dispatch(XPluginFlowMessagePtr msg) {
    std::lock_guard<std::mutex> lck(mutex_);
    auto type_handle = XPluginMsgRegistry::Instance().Get(msg->type());
    if (type_handle == XPLUGIN_INVALID_MSG_TYPE) {
      LOGW << "push no consumer message，type:" << msg->type();
      return;
    }
    auto &plugins = table_[type_handle];
    for (auto &plugin : plugins) {
      plugin->OnMsg(msg);
    }
  }

 private:
  std::map<XPluginMsgTypeHandle, std::vector<XPluginPtr>> table_;
  hobot::CThreadPool msg_handle_;

  std::mutex m_MsgDeserializeMapMutex;
  std::unordered_map<std::string,
      XPluginFlowMessagePtr(*)(const std::string &)> m_MsgDeserializeMap;

  std::mutex mutex_;
};

}  // namespace xpluginflow
}  // namespace vision
}  // namespace horizon
#endif  // XPLUGINFLOW_INCLUDE_XPLUGINFLOW_MANAGER_MSG_MANAGER_H_