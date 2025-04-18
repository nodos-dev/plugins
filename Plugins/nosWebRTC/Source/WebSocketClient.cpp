// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#include "WebSocketClient.h"
#include <iostream>
#include <exception>
#include "ssl-cert/cert.h"
#include "ssl-cert/key.h"

#include <Nodos/PluginAPI.h>

void WebSocketsLogCallback(int level, const char *line) {
	switch (level)
	{
	case LLL_ERR:
		nosEngine.LogE(line);
		break;
	case LLL_WARN:
		nosEngine.LogW(line);
		break;
	case LLL_INFO:
		nosEngine.LogI(line);
		break;
	default:
		nosEngine.LogD(line);
		break;
	}
}

nosWebSocketClient::nosWebSocketClient(const std::string fullIP, bool useHttps) : UseHttps(useHttps), FullAddress(fullIP)
{
	static bool logCallbackSet = false;
	if (!logCallbackSet)
	{
		lws_set_log_level(LLL_ERR | LLL_WARN | LLL_NOTICE | LLL_INFO, WebSocketsLogCallback);
		logCallbackSet = true;
	}
	try {
		StartWebSocket();
	}
	catch (std::exception& E) {
		throw(E);
	}
}

nosWebSocketClient::nosWebSocketClient(const std::string server, const int _port, const std::string _path, bool useHttps)
	: FullAddress(server + ":" + std::to_string(_port) + _path), UseHttps(useHttps)
{
	StartWebSocket();
}

nosWebSocketClient::~nosWebSocketClient()
{
 	shouldUpdate = false;
	if (pContext) {
		lws_context_destroy(pContext);
	}
}

//Not the most efficient way but does the job now
//Also webrtc does not communicates with server very often
void nosWebSocketClient::PushData(std::string&& message)
{
	message = std::string(LWS_PRE, ' ') + message;
	sendQueue.push(std::move(message));
}

//We trust return value optimization for large datas
std::string nosWebSocketClient::GetReceivedDataAsString()
{
	if (receivedQueue.empty())
		return std::string();

	std::string tempData = receivedQueue.front();
	receivedQueue.pop();
	return std::move(tempData);
}

//Use this in a thread
void nosWebSocketClient::Update()
{
	if (pContext) {
		lws_service(pContext, 0);
		lws_callback_on_writable(pWSI);
	}
}

void nosWebSocketClient::SetConnectionErrorCallback(const std::function<void()>& connectionErr)
{
	ConnectionErrorCallback = connectionErr;
}

void nosWebSocketClient::SetRawMessageReceivedCallback(const std::function<void(void*, size_t)>& messageReceived)
{
	MessageReceivedCallback = messageReceived;
}

void nosWebSocketClient::SetConnectionSuccesfulCallback(const std::function<void()>& connectionSuccesful)
{
	ConnectionSuccesfulCallback = connectionSuccesful;
}

void nosWebSocketClient::SetConnectionClosedCallback(const std::function<void()>& connectionClosed)
{
	ConnectionClosedCallback = connectionClosed;
}

void nosWebSocketClient::Send()
{
	if (sendQueue.empty())
		return;
	std::string message = sendQueue.front();
	sendQueue.pop();
	int remainingDataToSent = message.size() - LWS_PRE;
	while (remainingDataToSent > 0) {
		int sent = lws_write(pWSI, (unsigned char*)&message[LWS_PRE], message.size() - LWS_PRE, (lws_write_protocol)LWS_WRITE_TEXT);
		remainingDataToSent -= sent;
		if (remainingDataToSent > 0) {
			std::cout << "Data partially send in nosWebSocketClient" << std::endl;
		}
	}
}

struct ParsedAddress {
	std::string Host;
	std::string Path;
	int Port;
};

ParsedAddress ParseFullAddress(const std::string& fullAddress) {
	ParsedAddress result = { "", "/", 80 }; // Default values

	// Find the protocol separator "://"
	size_t protocolSeparatorPos = fullAddress.find("://");
	size_t addressStartPos = (protocolSeparatorPos != std::string::npos) ? protocolSeparatorPos + 3 : 0;

	// Extract Address (with protocol if it exists)
	size_t portSeparatorPos = fullAddress.find(':', addressStartPos);
	size_t pathSeparatorPos = fullAddress.find('/', addressStartPos);

	// Extract Host
	if (protocolSeparatorPos != std::string::npos) {
		size_t protocolEndPos = protocolSeparatorPos + 3;
		size_t hostEndPos = (portSeparatorPos != std::string::npos) ? portSeparatorPos : (pathSeparatorPos != std::string::npos) ? pathSeparatorPos : fullAddress.length();
		result.Host = fullAddress.substr(protocolEndPos, hostEndPos - protocolEndPos);
	}
	else {
		size_t hostEndPos = (portSeparatorPos != std::string::npos) ? portSeparatorPos : (pathSeparatorPos != std::string::npos) ? pathSeparatorPos : fullAddress.length();
		result.Host = fullAddress.substr(0, hostEndPos);
	}

	// Extract Port
	if (portSeparatorPos != std::string::npos && (pathSeparatorPos == std::string::npos || portSeparatorPos < pathSeparatorPos)) {
		size_t portStartPos = portSeparatorPos + 1;
		size_t portEndPos = (pathSeparatorPos != std::string::npos) ? pathSeparatorPos : fullAddress.length();
		result.Port = std::stoi(fullAddress.substr(portStartPos, portEndPos - portStartPos));
	}

	// Extract Path
	if (pathSeparatorPos != std::string::npos) {
		result.Path = fullAddress.substr(pathSeparatorPos);
	}

	return result;
}

void nosWebSocketClient::StartWebSocket()
{
	pContext = nullptr;
	memset(&Protocols, 0, sizeof(lws_protocols) * 3);

	Protocols[0].name = "mz-ws";
	Protocols[0].callback = mz_ws_callback;
	Protocols[0].per_session_data_size = 0;
	Protocols[0].rx_buffer_size = 10 * 1024 * 1024; //received buffer size. Note that data can be received in fragments!

	Protocols[1].name = nullptr;
	Protocols[1].callback = nullptr;
	Protocols[1].per_session_data_size = 0;

	lws_context_creation_info Info;
	memset(&Info, 0, sizeof Info);

	Info.port = CONTEXT_PORT_NO_LISTEN;
	Info.protocols = &Protocols[0];
	Info.gid = -1;
	Info.uid = -1;
	Info.user = this;
	Info.timeout_secs = 30;

	Info.options |= LWS_SERVER_OPTION_DISABLE_IPV6;
	if (UseHttps) {
		Info.options |= LWS_SERVER_OPTION_ALLOW_NON_SSL_ON_SSL_PORT;

		Info.client_ssl_cert_mem = cert_pem;
		Info.client_ssl_cert_mem_len = cert_pem_len;
		Info.client_ssl_key_mem = key_pem;
		Info.client_ssl_key_mem_len = key_pem_len;
	}

	pContext = lws_create_context(&Info);
	if (!pContext)
		throw std::exception("Port not found in the address!");

	auto addressInfo = ParseFullAddress(FullAddress);

	if (!addressInfo.Path.starts_with('/')) {
		addressInfo.Path = '/' + addressInfo.Path;
	}

	struct lws_client_connect_info connect_info = {};
	connect_info.context = pContext;
	connect_info.address = addressInfo.Host.c_str();
	connect_info.port = addressInfo.Port;
	connect_info.path = addressInfo.Path.c_str();
	connect_info.host = addressInfo.Host.c_str();
	connect_info.origin = "Nodos";
	connect_info.protocol = Protocols[0].name;
	if(UseHttps)
		connect_info.ssl_connection = LCCSCF_USE_SSL | LCCSCF_ALLOW_SELFSIGNED | LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK | LCCSCF_ALLOW_EXPIRED | LCCSCF_ALLOW_INSECURE;
	pWSI = lws_client_connect_via_info(&connect_info);
	if (!pWSI)
	{
		lws_context_destroy(pContext); // Clean up
		throw std::runtime_error("Failed to connect to server");
	}
}

//TODO: make it so that it can handle large chunks
void nosWebSocketClient::ProcessReceivedData(void* data, size_t length)
{
	//TODO: make it safer
	std::string strData = std::string(reinterpret_cast<char *>(data), length);
	receivedQueue.push(std::move(strData));
}

static int mz_ws_callback(struct lws* WSI, enum lws_callback_reasons Reason, void* User, void* In, size_t Len)
{
	struct lws_context* Context = lws_get_context(WSI);
	nosWebSocketClient* Socket = (nosWebSocketClient*)lws_context_user(Context);
	switch (Reason)
	{
		case LWS_CALLBACK_CLIENT_ESTABLISHED:
		{
			//Succesful connection
			if (Socket && Socket->pWSI == WSI && Socket->ConnectionSuccesfulCallback)
				Socket->ConnectionSuccesfulCallback();
			lws_set_timeout(WSI, NO_PENDING_TIMEOUT, 0);

		}
		break;
		case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
		{
			lwsl_err("Connection failed: %s\n", (const char*)In);
			//Unsuccesful connection
			if (Socket && Socket->pWSI == WSI && Socket->ConnectionErrorCallback) {
				Socket->ConnectionErrorCallback();
			}
		}
		break;
		case LWS_CALLBACK_CLIENT_RECEIVE:
		{
			//We received a message from server.
			if (Socket && Socket->pWSI == WSI) {
				Socket->ProcessReceivedData(In, Len);
				if(Socket->MessageReceivedCallback)
					Socket->MessageReceivedCallback(In, Len);
			}

			lws_set_timeout(WSI, NO_PENDING_TIMEOUT, 0);
			break;
		}
		case LWS_CALLBACK_CLIENT_WRITEABLE:
		{
			//This reason indicates that the other endpoint is ready for receiving message!
			//This is important because this is WHEN we should send the data.
			//It also means our WebSocket wrapper should hold the data to be sent at some data structure!!!

			//Also do not forget to call lws_callback_on_writable() method when you want to send data to a socket as soon as 
			// the socket is ready to accept more data, i.e. writable
			// You dont call this immediately before sending data, instead you call it when you know you have a data sendable
			// and then callback will be triggered with LWS_CALLBACK_CLIENT_WRITEABLE when the other endpoint is ready!
			// 
			if (Socket && Socket->pWSI == WSI) {
				//std::cout << "Socket is writable" << std::endl;
				Socket->Send();
			}
			//TODO: Use your SEND method with lws_write here!!!
			//check(Socket->Wsi == Wsi);
			//Socket->OnRawWebSocketWritable(Wsi);
			//lws_callback_on_writable(Wsi);
			//lws_set_timeout(Wsi, NO_PENDING_TIMEOUT, 0);
			break;
		}
		case LWS_CALLBACK_CLIENT_CLOSED:
		{
			if (Socket && Socket->pWSI == WSI && Socket->ConnectionClosedCallback)
				Socket->ConnectionClosedCallback();
		}
	}

	return 0;
}