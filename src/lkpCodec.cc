#include "lkpCodec.h"

#include "muduo/base/Logging.h"
#include "muduo/net/Endian.h"
#include "muduo/net/protorpc/google-inl.h"

#include <google/protobuf/descriptor.h>

#include <zlib.h>  // adler32

using namespace muduo;
using namespace muduo::net;

void lkpCodec::fillEmptyBuffer(Buffer* buf, const google::protobuf::Message& message)
{
  // buf->retrieveAll();
  assert(buf->readableBytes() == 0);

  const std::string& typeName = message.GetTypeName();
  int32_t nameLen = static_cast<int32_t>(typeName.size()+1);
  buf->appendInt32(nameLen);
  buf->append(typeName.c_str(), nameLen);

  // code copied from MessageLite::SerializeToArray() and MessageLite::SerializePartialToArray().
  GOOGLE_DCHECK(message.IsInitialized()) << InitializationErrorMessage("serialize", message);

  /**
   * 'ByteSize()' of message is deprecated in Protocol Buffers v3.4.0 firstly.
   * But, till to v3.11.0, it just getting start to be marked by '__attribute__((deprecated()))'.
   * So, here, v3.9.2 is selected as maximum version using 'ByteSize()' to avoid
   * potential effect for previous muduo code/projects as far as possible.
   * Note: All information above just INFER from
   * 1) https://github.com/protocolbuffers/protobuf/releases/tag/v3.4.0
   * 2) MACRO in file 'include/google/protobuf/port_def.inc'.
   * eg. '#define PROTOBUF_DEPRECATED_MSG(msg) __attribute__((deprecated(msg)))'.
   * In addition, usage of 'ToIntSize()' comes from Impl of ByteSize() in new version's Protocol Buffers.
   */

  #if GOOGLE_PROTOBUF_VERSION > 3009002
    int byte_size = google::protobuf::internal::ToIntSize(message.ByteSizeLong());
  #else
    int byte_size = message.ByteSize();
  #endif
  buf->ensureWritableBytes(byte_size);

  uint8_t* start = reinterpret_cast<uint8_t*>(buf->beginWrite());
  uint8_t* end = message.SerializeWithCachedSizesToArray(start);
  if (end - start != byte_size)
  {
    #if GOOGLE_PROTOBUF_VERSION > 3009002
      ByteSizeConsistencyError(byte_size, google::protobuf::internal::ToIntSize(message.ByteSizeLong()), static_cast<int>(end - start));
    #else
      ByteSizeConsistencyError(byte_size, message.ByteSize(), static_cast<int>(end - start));
    #endif
  }
  buf->hasWritten(byte_size);

  int32_t checkSum = static_cast<int32_t>(
      ::adler32(1,
                reinterpret_cast<const Bytef*>(buf->peek()),
                static_cast<int>(buf->readableBytes())));
  buf->appendInt32(checkSum);
  assert(buf->readableBytes() == sizeof nameLen + nameLen + byte_size + sizeof checkSum);
  int32_t len = sockets::hostToNetwork32(static_cast<int32_t>(buf->readableBytes()));
  buf->prepend(&len, sizeof len);
}

//
// no more google code after this
//

//
// FIXME: merge with RpcCodec
//

namespace
{
  const string kNoErrorStr = "NoError";
  const string kInvalidLengthStr = "InvalidLength";
  const string kCheckSumErrorStr = "CheckSumError";
  const string kInvalidNameLenStr = "InvalidNameLen";
  const string kUnknownMessageTypeStr = "UnknownMessageType";
  const string kParseErrorStr = "ParseError";
  const string kUnknownErrorStr = "UnknownError";
}

const string& lkpCodec::errorCodeToString(ErrorCode errorCode)
{
  switch (errorCode)
  {
   case kNoError:
     return kNoErrorStr;
   case kInvalidLength:
     return kInvalidLengthStr;
   case kCheckSumError:
     return kCheckSumErrorStr;
   case kInvalidNameLen:
     return kInvalidNameLenStr;
   case kUnknownMessageType:
     return kUnknownMessageTypeStr;
   case kParseError:
     return kParseErrorStr;
   default:
     return kUnknownErrorStr;
  }
}

void lkpCodec::defaultErrorCallback(const muduo::net::TcpConnectionPtr& conn,
                                         muduo::net::Buffer* buf,
                                         muduo::Timestamp,
                                         ErrorCode errorCode)
{
  LOG_ERROR << "lkpCodec::defaultErrorCallback - " << errorCodeToString(errorCode);
  if (conn && conn->connected())
  {
    conn->shutdown();
  }
}

int32_t asInt32(const char* buf)
{
  int32_t be32 = 0;
  ::memcpy(&be32, buf, sizeof(be32));
  return sockets::networkToHost32(be32);
}

void lkpCodec::onMessage(const TcpConnectionPtr& conn,
                              Buffer* buf,
                              Timestamp receiveTime)
{
  printf("Codec 被触发\n");
  while (buf->readableBytes() >= kMinMessageLen + kHeaderLen)
  {
    const int32_t len = buf->peekInt32();
    if (len > kMaxMessageLen || len < kMinMessageLen)
    {
      errorCallback_(conn, buf, receiveTime, kInvalidLength);
      break;
    }
    else if (buf->readableBytes() >= implicit_cast<size_t>(len + kHeaderLen))
    {
      ErrorCode errorCode = kNoError;
      MessagePtr message = parse(buf->peek()+kHeaderLen, len, &errorCode);//根据字节流中的typeName，创建对应的MessagePtr
      if (errorCode == kNoError && message)
      {
        messageCallback_(conn, message, receiveTime);
        buf->retrieve(kHeaderLen+len);
      }
      else
      {
        errorCallback_(conn, buf, receiveTime, errorCode);
        break;
      }
    }
    else
    {
      break;
    }
  }
}

google::protobuf::Message* lkpCodec::createMessage(const std::string& typeName)
{
  google::protobuf::Message* message = NULL;
  const google::protobuf::Descriptor* descriptor =
    google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(typeName);
  if (descriptor)
  {
    const google::protobuf::Message* prototype =
      google::protobuf::MessageFactory::generated_factory()->GetPrototype(descriptor);
    if (prototype)
    {
      message = prototype->New();
    }
  }
  return message;
}

MessagePtr lkpCodec::parse(const char* buf, int len, ErrorCode* error)
{
  MessagePtr message;

  // check sum
  int32_t expectedCheckSum = asInt32(buf + len - kHeaderLen);
  int32_t checkSum = static_cast<int32_t>(
      ::adler32(1,
                reinterpret_cast<const Bytef*>(buf),
                static_cast<int>(len - kHeaderLen)));
  //校验和通过才会继续
  if (checkSum == expectedCheckSum)
  {
    // get message type name
    int32_t nameLen = asInt32(buf);
    if (nameLen >= 2 && nameLen <= len - 2*kHeaderLen)
    {
      std::string typeName(buf + kHeaderLen, buf + kHeaderLen + nameLen - 1);
      // create message object
      message.reset(createMessage(typeName));//根据typeName创建MessagePtr
      if (message)
      {
        // parse from buffer
        const char* data = buf + kHeaderLen + nameLen;
        int32_t dataLen = len - nameLen - 2*kHeaderLen;
        if (message->ParseFromArray(data, dataLen))
        {
          *error = kNoError;
        }
        else
        {
          *error = kParseError;
        }
      }
      else
      {
        *error = kUnknownMessageType;
      }
    }
    else
    {
      *error = kInvalidNameLen;
    }
  }
  else
  {
    *error = kCheckSumError;
  }

  return message;
}
