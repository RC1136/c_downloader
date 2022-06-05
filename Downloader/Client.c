//#include <winsock2.h>
#include <Ws2tcpip.h>
//#include <stdlib.h>
#include <stdio.h>


#pragma comment(lib, "WS2_32")		//error LNK2019 майже у всіх функціях


#define DEF_BUFFLEN 4096
#define DEF_PORT 27015



SOCKET ConnectServer(char* ip, int port)
{

	char* strport = (char*)malloc(5 * sizeof(char));
	if (strport == NULL)
		return 1;
	_itoa((int)port, strport, 10);

	WSADATA wsaData;
	SOCKET ConnectSocket = INVALID_SOCKET;
	struct addrinfo* result = NULL,
		hints;
	int res;

	//ініціалізую сокет
	res = WSAStartup(//https://docs.microsoft.com/en-us/windows/win32/api/winsock/nf-winsock-wsastartup
		MAKEWORD(2, 2),
		&wsaData
	);
	if (res != 0) {
		printf("WSAStartup failed (%d)\n", res);
		return INVALID_SOCKET;
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	//отримую адресу і порт
	res = getaddrinfo(//https://docs.microsoft.com/en-us/windows/win32/api/ws2tcpip/nf-ws2tcpip-getaddrinfo
		ip,				//адреса
		strport,		//порт
		&hints,
		&result
	);
	if (res != 0) {
		printf("getaddrinfo failed (%d)\n", res);
		WSACleanup();
		return INVALID_SOCKET;
	}
	//створюю сокет для з'єднання з сервером
	ConnectSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (ConnectSocket == INVALID_SOCKET) {
		printf("socket failed (%ld)\n", WSAGetLastError());
		WSACleanup();
		return INVALID_SOCKET;
	}
	//з'єднююсь з сервером
	res = connect(ConnectSocket, result->ai_addr, (int)result->ai_addrlen);
	if (res == SOCKET_ERROR) {
		closesocket(ConnectSocket);
		return INVALID_SOCKET;
	}

	freeaddrinfo(result);
	if (ConnectSocket == INVALID_SOCKET) {
		printf("Unable to connect to server!\n");
		WSACleanup();
		return INVALID_SOCKET;
	}

	return ConnectSocket;
}


BOOL ReceiveFile(SOCKET ConnectSocket, const char* file_name, const int file_length)
{


	HANDLE File = INVALID_HANDLE_VALUE;
	File = CreateFileA(
		file_name,
		GENERIC_WRITE,
		0,
		NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);
	if (File == INVALID_HANDLE_VALUE) {
		printf("CreateFile failed (%d)\n", GetLastError());
		return FALSE;
	}


	int bytes_read, bytes_sent, res, flength = 0;
	char* buf = (char*)malloc(DEF_BUFFLEN * sizeof(char));
	if (buf == NULL)
		return FALSE;

	void* tmp = malloc(sizeof(int));
	printf("Bytes received:           ");
	do {
		//printf("1\t");
		bytes_read = recv(
			ConnectSocket,
			buf,
			DEF_BUFFLEN,
			0
		);

		//printf("2\t");
		flength += bytes_read;
		printf("\b\b\b\b\b\b\b\b\b\b% 10d", flength);

		//printf("Received %d bytes\n", bytes_read);
		res = WriteFile(
			File,
			buf,
			bytes_read,
			tmp,
			NULL
		);
		if (res < 0) {
			printf("WriteFile error (%d)\n", GetLastError());
			return FALSE;
		}
		//printf("4\t");
	} while (flength < file_length);
	putchar('\n');
	free(tmp);

	_itoa(flength, buf, 10);
	bytes_sent = send(ConnectSocket, buf, strlen(buf)+1, 0);
	if (bytes_sent < 0) {
		printf("flength send error (%d)\n", WSAGetLastError());
		return FALSE;
	}

	free(buf);
	CloseHandle(File);


	return TRUE;
}



BOOL ReceiveFolder(SOCKET ConnectSocket, const char* path)
{
	char* buf = (char*)malloc(DEF_BUFFLEN * sizeof(char));
	if (buf == NULL)
		return 1;
	char* file_name = (char*)malloc(MAX_PATH * sizeof(char));
	if (file_name == NULL)
		return 1;
	int res, bytes_read, bytes_sent, file_length;

	if (path == NULL) {
		printf("Receiving directory name\n");
		bytes_read = 0;
		//2
		do {
			printf("%d\n", bytes_read);
			bytes_read += recv(ConnectSocket, &file_name[bytes_read], MAX_PATH, 0);
			if (bytes_read > MAX_PATH)
				return FALSE;
		} while (file_name[bytes_read - 1] != '\0');

		path = file_name;
		_itoa(bytes_read, buf, 10);
		//3
		bytes_sent = send(ConnectSocket, buf, strlen(buf) + 1, 0);

	}
		
	res = CreateDirectoryA(path, NULL);

	if (!res) {
		if (GetLastError() == ERROR_ALREADY_EXISTS) {
			SetLastError(0);
		}
		else {
			printf("Failed to create directory (%d)\n", GetLastError());
			return FALSE;
		}
	}


	char* current_directory = (char*)malloc(MAX_PATH * sizeof(char));
	if (current_directory == NULL)
		return FALSE;
	GetCurrentDirectoryA(MAX_PATH, current_directory);
	SetCurrentDirectoryA(path);
	GetCurrentDirectoryA(MAX_PATH, buf);
	printf("Current directory is \"%s\"\n", buf);


	do {
		printf("Receiving file name\n");
		
		//Отримую назву файлу
		bytes_read = 0;
		//6
		do {
			bytes_read += recv(ConnectSocket, &file_name[bytes_read], MAX_PATH - bytes_read, 0);
			if (bytes_read > MAX_PATH)
				return 1;
		} while (file_name[bytes_read - 1] != '\0');
		if (bytes_read < 0) {
			printf("file_name recv error(%d)\n", WSAGetLastError());
			return 1;
		}

		if (bytes_read == 1) {
			bytes_sent = send(ConnectSocket, "\0", 1, 0);
			break;
		}

		//file_name[bytes_read] = '\0';
		printf("File name is %s \n", file_name);

		_itoa(bytes_read, buf, 10);
		//7
		bytes_sent = send(ConnectSocket, buf, strlen(buf) + 1, 0);
		if (bytes_sent < 1) {
			printf("file_name length sending error (%d)\n", WSAGetLastError());
			return 1;
		}

		if (file_name[1] == '\\') {
			ReceiveFolder(ConnectSocket, file_name);
		}
		else {

			//Отримую розмір файлу
			bytes_read = 0;
			//10
			do {
				bytes_read += recv(ConnectSocket, &buf[bytes_read], DEF_BUFFLEN - bytes_read, 0);
				if (bytes_read > DEF_BUFFLEN)
					return 1;
			} while (buf[bytes_read - 1] != '\0');

			if (bytes_read < 0) {
				printf("file length recv error(%d)\n", WSAGetLastError());
				return 1;
			}
			file_length = atoi(buf);
			_itoa(bytes_read, buf, 10);
			//11
			bytes_sent = send(ConnectSocket, buf, (int)strlen(buf) + 1, 0);
			if (bytes_sent < 0) {
				printf("file length length send error (%d)\n", WSAGetLastError());
				return 1;
			}


			//14
			ReceiveFile(ConnectSocket, file_name, file_length);

		}



	} while (1);
	printf("Folder received!\n");


	SetCurrentDirectoryA(current_directory);
	free(current_directory);
	free(buf);
	free(file_name);
	return TRUE;
}


int main(int argc, char* argv[])
{

	short port = DEF_PORT;
	char* ip = argc > 1 ? argv[1] : "127.0.0.1";
	SOCKET ConnectSocket;
	do {
		ConnectSocket = ConnectServer(ip, (int)port);
	} while (ConnectSocket == INVALID_SOCKET);

	//ловлю файлики
	ReceiveFolder(ConnectSocket, NULL);

	closesocket(ConnectSocket);
	WSACleanup();
	/**/

	while (getchar() != '\n');
	return 0;
}